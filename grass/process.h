#pragma once

#include "kmem.h"
#include "queue.h"
#include "syscall.h"

#define SIZE_KSTACK 0x4000 // default kernel stack size (16KB)

struct process {
    int pid;
    uint mepc;
    struct syscall syscall;
    void *kstack, *ksp;
};

ulonglong mtime_get();

struct process *proc_alloc();
void proc_set_ready(struct process *);
void proc_free(int);
