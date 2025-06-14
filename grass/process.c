/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: helper functions for process management
 */

#include "process.h"
#include "list.h"
extern list_t proc_set;
extern queue_t readyQ;

void proc_set_ready(struct process *proc) { 
    if (queue_push(readyQ, proc) < 0)
        FATAL("proc_set_ready: failed to push proc %d onto readyQ", proc->pid);
}

/**
 * proc_alloc: alloc PCB and kernel stack of process, and push onto proc_list.
 * Returns NULL if OOM.
 */
struct process *proc_alloc() {
    static uint curr_pid = 0;

    struct process *proc = egozalloc(sizeof(struct process));
    if (proc == EGOSNULL)
        FATAL("proc_alloc: failed to alloc PCB");

    proc->pid    = ++curr_pid;
    proc->kstack = egosalloc(SIZE_KSTACK);
    proc->ksp    = (void*)((uint)proc->kstack + SIZE_KSTACK);

    if (proc->kstack == EGOSNULL)
        FATAL("proc_alloc: failed to alloc kstack");
    if (list_append(proc_set, proc) < 0)
        FATAL("proc_alloc: failed to push new proc onto proc_set");
    return proc;
}

void proc_free(int pid) {
    FATAL("proc_free: unimplemented");
}
