#pragma once

#include "kmem.h"
#include "queue.h"
#include "list.h"
#include "syscall.h"

#define SIZE_KSTACK 0x4000 // default kernel stack size (16KB)

struct process {
    int pid;
    uint mepc;
    struct syscall syscall;
    queue_t senderQ;  // queue of processes that want to send a message to this process
    queue_t msgwaitQ; // temporary place that a receiver can wait in until they get msg (INVARIANT: always at most one process on msgwaitQ)
    void *kstack, *ksp;
};

ulonglong mtime_get();

struct process *proc_alloc();
struct process *proc_pcb_find(queue_t, int);
void proc_set_ready(struct process *);
void proc_free(int);
