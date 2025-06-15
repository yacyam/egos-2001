/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: helper functions for process management
 */

#include "process.h"
#include "list.h"
extern list_t proc_set;
extern queue_t runQ;
extern queue_t readyQ;

struct process *proc_found;
/**
 * __proc_pcb_find_enumerate: will cast `item` into a process, and will set
 * `proc_found` to the current process being enumerated through if its PID
 * matches with `pid`
 */
void __proc_pcb_find_enumerate(void *item, void *pid) {
    if (item == EGOSNULL)
        FATAL("__proc_pcb_find_enumerate: item is NULL (should be PCB)");
    struct process *p = item;
    if (p->pid == (int)pid)
        proc_found = p;
}

/**
 * proc_pcb_find: find a process (in the `queue`) that matches `pid`. 
 * Returns EGOSNULL if a process with pid equal to `pid` is not found in `queue`
 */
struct process *proc_pcb_find(queue_t queue, int pid) {
    proc_found = EGOSNULL;
    queue_iterate(queue, __proc_pcb_find_enumerate, (void*)pid);

    if (proc_found->pid != pid)
        FATAL("proc_pcb_find: found proc %d instead of %d", proc_found->pid, pid);
    return proc_found;
}

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

    proc->senderQ  = queue_new();
    proc->msgwaitQ = queue_new();

    list_append(proc_set, proc);
    return proc;
}

/**
 * proc_free: free the memory associated with process `pid` and its PCB. This
 * function should only be called by GPID_PROCESS.
 * 
 * TODO: resolve any outstanding messages being sent to this process.
 */
void proc_free(int pid) {
    if (pid == GPID_ALL) {
        FATAL("proc_free: killing all user processes unimplemented");
    }

    struct process *proc_being_killed;
    if ((proc_being_killed = proc_pcb_find(proc_set, pid)) < 0)
        FATAL("proc_free: failed to find pcb of proc %d", pid);

    if (queue_length(proc_being_killed->senderQ) > 0)
        FATAL("proc_free: non-empty senderQ of process being killed");

    // remove from runQ (if there) and proc_set
    queue_delete(runQ, proc_being_killed);
    list_delete(proc_set, proc_being_killed);
    
    // free app memory, kernel stack, senderQ, msgwaitQ, and PCB
    earth->mmu_free(pid);
    egosfree(proc_being_killed->kstack);
    queue_free(proc_being_killed->senderQ);
    queue_free(proc_being_killed->msgwaitQ);
    egosfree(proc_being_killed);
}
