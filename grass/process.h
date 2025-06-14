#pragma once

#include "egos.h"
#include "syscall.h"

struct process {
    int pid;
    struct syscall syscall;
    /* Student's code goes here (Preemptive Scheduler | System Call). */

    /* Add new fields for lifecycle statistics, MLFQ or process sleep. */

    /* Student's code ends here. */
};

ulonglong mtime_get();

int proc_alloc();
void proc_free(int);
