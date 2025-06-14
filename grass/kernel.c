/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: kernel ≈ 2 handlers
 *   intr_entry() handles timer and device interrupts.
 *   excp_entry() handles system calls and faults (e.g., invalid memory access).
 */

void ctx_switch(void **old_sp, void **new_sp);

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

static void intr_entry(uint);
static void excp_entry(uint);

void kernel_entry() {
    asm("csrr %0, mhartid":"=r"(core_in_kernel));

    uint mcause;
    asm("csrr %0, mcause" : "=r"(mcause));
    (mcause & (1 << 31)) ? intr_entry(mcause & 0x3FF) : excp_entry(mcause);
}

#define INTR_ID_TIMER   7
#define EXCP_ID_ECALL_U 8
#define EXCP_ID_ECALL_M 11
static void proc_yield(queue_t queue);
static void proc_try_syscall(struct process* proc);

static void excp_entry(uint id) {
    FATAL("excp_entry: unimplemented");
}

static void intr_entry(uint id) {
    if (id == INTR_ID_TIMER) { proc_yield(runQ); return; }
    
    FATAL("intr_entry: id %d", id);
}

static void proc_yield(queue_t queue) {
    // push current process onto `queue` (can be runQ, or another queue)
    if (queue_push(queue, proc_curr) < 0)
        FATAL("proc_yield: failed to push current proc %d onto runQ", proc_curr->pid);

    // schedule another process (newest first)
    if (queue_length(readyQ) > 0) {
        FATAL("proc_yield: about to schedule a new process!");
    }
    else if (queue_length(runQ) > 0) {
        if (queue_pop(runQ, (void**)&proc_next) < 0)
            FATAL("proc_yield: failed to pop runQ");
        
        // both are pointers on purpose
        ctx_switch(&proc_curr->ksp, &proc_next->ksp);
        proc_curr = proc_next;

        earth->mmu_switch(proc_next->pid);
        earth->mmu_flush_cache();
        earth->timer_reset(core_in_kernel);
    }
    else {
        FATAL("proc_yield: no more processes to schedule");
    }
}

static void proc_try_send(struct process* sender) {
    FATAL("proc_try_send: unimplemented");
}

static void proc_try_recv(struct process* receiver) {
    FATAL("proc_try_recv: unimplemented");
}

static void proc_try_syscall(struct process* proc) {
    FATAL("proc_try_syscall: unimplemented");
}
