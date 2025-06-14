/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: kernel ≈ 2 handlers
 *   intr_entry() handles timer and device interrupts.
 *   excp_entry() handles system calls and faults (e.g., invalid memory access).
 */

#include "process.h"
#include <string.h>

static void intr_entry(uint);
static void excp_entry(uint);

void kernel_entry() {
    FATAL("kernel_entry: unimplemented");

    uint mcause;
    asm("csrr %0, mcause" : "=r"(mcause));
    (mcause & (1 << 31)) ? intr_entry(mcause & 0x3FF) : excp_entry(mcause);
}

#define INTR_ID_TIMER   7
#define EXCP_ID_ECALL_U 8
#define EXCP_ID_ECALL_M 11
static void proc_yield();
static void proc_try_syscall(struct process* proc);

static void excp_entry(uint id) {
    FATAL("excp_entry: unimplemented");
}

static void intr_entry(uint id) {
    FATAL("intr_entry: unimplemented");
}

static void proc_yield() {
    // things to remember: mmu switch, flush cache, timer reset
    FATAL("proc_yield: unimplemented");
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
