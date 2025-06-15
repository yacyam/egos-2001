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

struct process *proc_curr, *proc_next;

/**
 * proc_switch_aftermath: sets up kernel state after a process is switched to.
 * Requires that `proc_curr` is the process that was switched from, and
 * `proc_next` is the process that was switched to.
 */
void proc_switch_aftermath() {
    proc_curr = proc_next;
    earth->mmu_switch(proc_curr->pid);
    earth->mmu_flush_cache();
    earth->timer_reset(core_in_kernel);
}

/**
 * ctx_entry: simulate an interrupt, and return from interrupt to newly
 * scheduled process. This function is called on the kernel stack of the newly
 * created process (although the SP could also in essence be the boot/trap stack)
 */
void ctx_entry() {
    proc_switch_aftermath();

    // simulate an interrupt (could clear out other regs but i am lazy).
    // app.s sets the stack pointer
    asm("csrw mepc, %0" ::"r"(APPS_ENTRY));
    asm("csrw mscratch, %0"::"r"(proc_curr->ksp));
    asm("mv a0, %0" ::"r"(APPS_ARG));     // address of argc
    asm("mv a1, %0" ::"r"(APPS_ARG + 4)); // argv
    asm("mret");
}

static void intr_entry(uint);
static void excp_entry(uint);

void kernel_entry() {
    asm("csrr %0, mhartid":"=r"(core_in_kernel));
    asm("csrr %0, mepc":"=r"(proc_curr->mepc));

    uint mcause;
    asm("csrr %0, mcause" : "=r"(mcause));
    (mcause & (1 << 31)) ? intr_entry(mcause & 0x3FF) : excp_entry(mcause);

    asm("csrw mepc, %0"::"r"(proc_curr->mepc));
}

#define INTR_ID_TIMER   7
#define EXCP_ID_ECALL_U 8
#define EXCP_ID_ECALL_M 11
static void proc_yield(queue_t queue);
static void proc_try_syscall();

static void excp_entry(uint id) {
    if (id == EXCP_ID_ECALL_M) {
        proc_curr->mepc += 4;
        memcpy(&proc_curr->syscall, (void*)SYSCALL_ARG, sizeof(struct syscall));
        proc_try_syscall();
        proc_yield(runQ);
        return;
    }

    FATAL("excp_entry: proc %d got unknown id %d", proc_curr->pid, id);
}

static void intr_entry(uint id) {
    if (id == INTR_ID_TIMER) { proc_yield(runQ); return; }
    
    FATAL("intr_entry: proc %d got unknown id %d", proc_curr->pid, id);
}

static void proc_yield(queue_t queue) {
    // push current process onto `queue` (can be runQ, or another queue)
    if (queue_push(queue, proc_curr) < 0)
        FATAL("proc_yield: failed to push current proc %d onto runQ", proc_curr->pid);

    // schedule another process (newest first)
    if (queue_length(readyQ) > 0) {
        if (queue_pop(readyQ, (void**)&proc_next) < 0)
            FATAL("proc_yield: failed to pop readyQ");
        
        ctx_start(&proc_curr->ksp, proc_next->ksp);
        proc_switch_aftermath();
    }
    else if (queue_length(runQ) > 0) {
        if (queue_pop(runQ, (void**)&proc_next) < 0)
            FATAL("proc_yield: failed to pop runQ");
        
        // both are pointers on purpose
        ctx_switch(&proc_curr->ksp, &proc_next->ksp);
        proc_switch_aftermath();
    }
    else {
        FATAL("proc_yield: no more processes to schedule %x", proc_curr->pid);
    }
}

/* * * * * * * */
// basically condition variables (self explanatory)

static void msg_wait() { proc_yield(proc_curr->msgwaitQ); }
static void msg_notify(struct process *recipient) {
    if (queue_length(recipient->msgwaitQ) == 0) return;
    if (queue_length(recipient->msgwaitQ) > 1)
        FATAL("notify: more than one process on proc %d's msgwaitQ", recipient->pid);
    
    if (queue_pop(recipient->msgwaitQ, EGOSNULL) < 0)
        FATAL("notify: failed to pop off of proc %d's msgwaitQ", recipient->pid);

    if (queue_push(runQ, recipient) < 0)
        FATAL("notify: failed to push recipient %d onto runQ", recipient->pid);
}

/* * * * * * * */

static void proc_try_send() {
    struct process *receiver = proc_pcb_find(proc_set, proc_curr->syscall.receiver);
    msg_notify(receiver);
    proc_yield(receiver->senderQ);
}

static void proc_try_recv() {
    // wait until someone wants to send a message to us (the receiver)
    while (!queue_length(proc_curr->senderQ))
        msg_wait();

    // attempt to find the desired sender from our senderQ
    struct process *sender;
    int sender_pid = proc_curr->syscall.sender;

    if (sender_pid == GPID_ALL) {
        // don't care who sends to us, head of our senderQ is chosen
        if (queue_pop(proc_curr->senderQ, (void**)&sender) < 0)
            FATAL("proc_try_recv: failed to pop off proc %d's non-empty senderQ", proc_curr->pid);
    }
    else {
        // wait until desired sender is on our senderQ, then delete
        while ((sender = proc_pcb_find(proc_curr->senderQ, sender_pid)) == EGOSNULL)
            msg_wait();
        
        if (queue_delete(proc_curr->senderQ, sender) < 0)
            FATAL("proc_try_recv: failed to delete proc %d off of proc %d's senderQ", sender->pid, proc_curr->pid);
    }

    // put sender back on runQ
    if (queue_push(runQ, sender) < 0)
        FATAL("proc_try_recv: failed to push %d onto runQ", sender->pid);

    // transfer message from sender's PCB to receiver's userspace msg buffer
    struct syscall *sc = (void*)SYSCALL_ARG;
    sc->sender = sender->pid;
    memcpy(sc->content, sender->syscall.content, SYSCALL_MSG_LEN);
}

static void proc_try_syscall() {
    switch (proc_curr->syscall.type) {
        case SYS_SEND:
            proc_try_send();
            break;
        case SYS_RECV:
            proc_try_recv();
            break;
        default:
            FATAL("proc_try_syscall: proc %d attempt unknown syscall type %d", \
                    proc_curr->pid, proc_curr->syscall.type);
    }
}
