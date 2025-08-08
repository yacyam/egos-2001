/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: kernel ≈ 2 handlers
 *   intr_entry() handles timer and device interrupts.
 *   excp_entry() handles system calls and faults (e.g., invalid memory access).
 */

void ctx_switch(void **old_sp, void **new_sp);
void ctx_start(void **old_sp, void *new_sp);

#include "process.h"
#include "queue.h"
#include "list.h"
#include <string.h>

uint core_in_kernel;

// holds the set of all alive processes
list_t proc_set;

queue_t runQ; // can be scheduled
queue_t readyQ; // can be scheduled (for the first time)

struct process * volatile proc_curr, * volatile proc_next;

/**
 * proc_switch_aftermath: sets up kernel state after a process is switched to.
 * Requires that `proc_curr` is the process that was switched from, and
 * `proc_next` is the process that was switched to.
 */
void proc_switch_aftermath() {
    earth->mmu_switch(&proc_curr->pgtbl, &proc_next->pgtbl);
    proc_curr = proc_next;
    earth->timer_reset(core_in_kernel);
}

void ctx_entry() {
    proc_switch_aftermath();
    proc_simulate_interrupt(proc_curr);
}

static void intr_entry(uint);
static void excp_entry(uint);

void kernel_entry() {
    asm("csrr %0, mhartid":"=r"(core_in_kernel));
    asm("csrr %0, mepc":"=r"(proc_curr->mepc));
    asm("csrr %0, mstatus":"=r"(proc_curr->mstatus)); // unnecessary, but it may bite me in the future if i dont do it

    uint mcause;
    asm("csrr %0, mcause" : "=r"(mcause));
    (mcause & (1 << 31)) ? intr_entry(mcause & 0x3FF) : excp_entry(mcause);

    asm("csrw mstatus, %0"::"r"(proc_curr->mstatus));
    asm("csrw mepc, %0"::"r"(proc_curr->mepc));
}

#define INTR_ID_TIMER   7

#define EXCP_ID_ECALL_U 8
#define EXCP_ID_ECALL_M 11

static void proc_yield(queue_t queue);
static void proc_try_syscall();

static void _excp_find_seg_contain_fault(void *seg, void *reason_fault) {
    segment *segment = seg;
    ppagefault_reason *reason = reason_fault;

    if (reason->page_num < segment->page_base || \
            reason->page_num >= segment->page_base + segment->num_pages) {
        return;
    }
    if ((reason->perms_fault & segment->perms_max) != reason->perms_fault) {
        FATAL("_excp_find_seg_containing_fault: segment at page=%d contains fault, but access=%x violates perms=%x", \
            segment->page_base, reason->perms_fault, segment->perms_max);
    }

    reason->seg_containing_fault = segment;
}

static void _excp_map_and_load_frame(ppagefault_reason *reason) {
    if (reason->seg_containing_fault == EGOSNULL) FATAL("_excp_map_and_load_frame: proc=%x on page=%x should have obtained segfault", proc_curr->pid, reason->page_num);
    if (reason->page_num >= NUM_PAGES) FATAL("_excp_map_and_load_frame: page=%x too big", reason->page_num);

    ppte *pte_fault = &proc_curr->pgtbl.tbl[reason->page_num];
    
    if (pte_fault->present) {
        INFO("_excp_map_and_load_frame: present case (COW). proc=%x, page=%x", proc_curr->pid, reason->page_num);

        if (reason->perms_fault != PERMS_WO) 
            FATAL("_excp_map_and_load_frame: not a write-protec page fault");

        switch (earth->mmu_refcnt(pte_fault->frame_num)) {
        case 0:
            FATAL("_excp_map_and_load_frame: proc=%d got refcnt=0 on frame=%x", 
                proc_curr->pid, pte_fault->frame_num);
            break;
        case 1:
            // upgrade perms of PTE to maximum inside segment
            earth->mmu_map(
                pte_fault, 
                pte_fault->frame_num, 
                reason->seg_containing_fault->perms_max
            );
            break;
        default:
            // Copy on write page fault:
            // alloc frame -> copy contents -> map PTE to new frame
            uint frame_copy = earth->mmu_alloc();
            
            // IMPORTANT: since page is write-protected, page and frame 
            // already synced up (no need to update both)
            memcpy((void*)FRAME_NUM_TO_REAL_ADDR(frame_copy), 
                (void*)FRAME_NUM_TO_REAL_ADDR(pte_fault->frame_num), PAGE_SIZE);

            earth->mmu_map(pte_fault, frame_copy, reason->seg_containing_fault->perms_max);
        }
    } else {
        // alloc frame, map it, load it by messaging file server (or zero-initializing)
        uint perms = reason->seg_containing_fault->perms_max;
        uint page_num_fault = reason->page_num;
        
        uint frame_num = earth->mmu_alloc();
        earth->mmu_map(pte_fault, frame_num, perms);

        if (reason->seg_containing_fault->in_file) {
            // CANT call file_read here (think about it).
            // convert faulting page num to block offset to read ELF of process

            // offset of segment in file + offset of page inside segment
            uint off_block = reason->seg_containing_fault->offset + \
                ((reason->page_num - reason->seg_containing_fault->page_base) * BLOCKS_PER_PAGE);

            // read every block that comprises the page
            for (int i = 0; i < BLOCKS_PER_PAGE; i++) {

                struct file_request req = (struct file_request) { 
                    .type = FILE_READ,
                    .ino = reason->seg_containing_fault->ino, .offset = off_block++
                };
                proc_curr->syscall = (struct syscall) {
                    .type = SYS_RPC, .receiver = GPID_FILE, .sender = GPID_FILE,
                };
                memcpy(proc_curr->syscall.content, &req, sizeof(req));
                proc_try_syscall();
                struct file_reply *reply = (struct file_reply *)proc_curr->syscall.content;
                if (reply->status == FILE_ERROR)
                    break;

                // synchronize both
                memcpy((void*)(PAGE_NUM_TO_REAL_ADDR(page_num_fault) + (i * BLOCK_SIZE)), reply->block.bytes, BLOCK_SIZE);
                memcpy((void*)(FRAME_NUM_TO_REAL_ADDR(frame_num) + (i * BLOCK_SIZE)), reply->block.bytes, BLOCK_SIZE);
            }
            
        } else {
            memset((void*)FRAME_NUM_TO_REAL_ADDR(frame_num), 0, PAGE_SIZE);
        }
    }
}

static void _ppagefaulthandler(uint page_num, uint perms_fault) {
    ppagefault_reason reason = (ppagefault_reason) {
        .page_num = page_num, .perms_fault = perms_fault,
        .seg_containing_fault = EGOSNULL
    };
    
    // find the segment containing the fault (segfault if not found)
    list_iterate(proc_curr->segtbl.segments, _excp_find_seg_contain_fault, &reason);
    if (reason.seg_containing_fault == EGOSNULL)
        FATAL("_ppagefaulthandler: segmentation fault on proc=%d and page=%x, mepc=%x", proc_curr->pid, page_num, proc_curr->mepc);

    _excp_map_and_load_frame(&reason);
}

static void excp_entry(uint id) {
    // system call handling
    if (id == EXCP_ID_ECALL_U || id == EXCP_ID_ECALL_M) {
        proc_curr->mepc += 4;
        memcpy(&proc_curr->syscall, (void*)SYSCALL_ARG, sizeof(struct syscall));
        proc_try_syscall();
        proc_yield(runQ);
        return;
    }

    // pseudo-page-fault handling
    if (id == EXCP_ID_FAULT_R || id == EXCP_ID_FAULT_W || id == EXCP_ID_FAULT_X) {
        uint mtval;
        asm("csrr %0, mtval":"=r"(mtval));

        // obtain the page number where the fault occurred 
        // (mepc for eXecute fault, mtval for rest)
        uint address_fault = proc_curr->mepc;
        if (id != EXCP_ID_FAULT_X)
            asm("csrr %0, mtval":"=r"(address_fault));

        _ppagefaulthandler(REAL_ADDR_TO_PAGE_NUM(address_fault), _convert_excp_id_to_perms(id));
        proc_yield(runQ);
        return;
    }

    FATAL("excp_entry: proc %d got unknown id %d, mepc %x", proc_curr->pid, id, proc_curr->mepc);
}

static void intr_entry(uint id) {
    if (id == INTR_ID_TIMER) { proc_yield(runQ); return; }
    
    FATAL("intr_entry: proc %d got unknown id %d", proc_curr->pid, id);
}

static void proc_yield(queue_t queue) {
    // push current process onto `queue` (can be runQ, or another queue)
    queue_push(queue, proc_curr);

    // schedule another process (newest first)
    if (queue_length(readyQ) > 0) {
        queue_pop(readyQ, (void**)&proc_next);
        ctx_start(&proc_curr->ksp, proc_next->ksp);
        proc_switch_aftermath();
    } 
    else if (queue_length(runQ) > 0) {
        queue_pop(runQ, (void**)&proc_next);
        
        // both are pointers on purpose
        ctx_switch(&proc_curr->ksp, &proc_next->ksp);
        proc_switch_aftermath();
    }
    else {
        FATAL("proc_yield: no more processes to schedule %x", proc_curr->pid);
    }
}

/* * * * * * * */
// basically condition variables

static void msg_wait() { proc_yield(proc_curr->msgwaitQ); }
static void msg_notify(struct process *recipient) {
    if (queue_length(recipient->msgwaitQ) == 0) 
        return;
    if (queue_length(recipient->msgwaitQ) > 1)
        FATAL("notify: more than one process on proc %d's msgwaitQ", recipient->pid);
    
    queue_pop(recipient->msgwaitQ, EGOSNULL);
    queue_push(runQ, recipient);
}

/* * * * * * * */

static void proc_try_send() {
    struct process *receiver = _proc_pcb_find(proc_set, proc_curr->syscall.receiver);
    msg_notify(receiver);
    proc_yield(receiver->senderQ);
}

static void proc_try_recv() {
    // wait until someone wants to send a message to us (the receiver)
    while (!queue_length(proc_curr->senderQ))
        msg_wait();

    // attempt to find the desired sender from our senderQ
    struct process * volatile sender;
    int volatile sender_pid = proc_curr->syscall.sender;

    if (sender_pid == GPID_ALL) {
        // take head of sendQ as the process that successfully sends to us
        queue_pop(proc_curr->senderQ, (void**)&sender);
    } else {
        // wait until desired sender is on our senderQ, then delete
        while ((sender = _proc_pcb_find(proc_curr->senderQ, sender_pid)) == EGOSNULL)
            msg_wait();
        queue_delete(proc_curr->senderQ, sender);
    }

    // make sender runnable
    queue_push(runQ, sender);

    // transfer message from sender's PCB to receiver's PCB and userspace msg buffer
    memcpy(proc_curr->syscall.content, sender->syscall.content, SYSCALL_MSG_LEN);
    proc_curr->syscall.sender = sender->pid;

    struct syscall *sc = (void*)SYSCALL_ARG;
    memcpy(sc, &proc_curr->syscall, sizeof(struct syscall));
}

static void proc_try_rpc() {
    proc_try_send();
    proc_try_recv();
}

static void proc_try_syscall() {
    switch (proc_curr->syscall.type) {
        case SYS_SEND:
            proc_try_send();
            break;
        case SYS_RECV:
            proc_try_recv();
            break;
        case SYS_RPC:
            proc_try_rpc();
            break;
        default:
            FATAL("proc_try_syscall: proc %d attempt unknown syscall type %d", \
                    proc_curr->pid, proc_curr->syscall.type);
    }
}
