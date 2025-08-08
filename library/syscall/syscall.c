/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: the system call interface for applications
 */

#include "egos.h"
#include "syscall.h"
#include <string.h>

static struct syscall* sc = (struct syscall*)SYSCALL_ARG;

void sys_send(int receiver, char* msg, uint size) {
    sc->type     = SYS_SEND;
    sc->receiver = receiver;
    // what happens if there is a page fault here ?
    memcpy(sc->content, msg, size);
    asm("ecall");
}

void sys_recv(int from, int* sender, char* buf, uint size) {
    sc->type   = SYS_RECV;
    sc->sender = from;
    asm("ecall");
    memcpy(buf, sc->content, size);
    if (sender) *sender = sc->sender;
}

void sys_rpc(int receiver, char *buf, uint size) {
    sc->type     = SYS_RPC;
    sc->receiver = receiver;
    sc->sender   = receiver; // receiver will send response back
    memcpy(sc->content, buf, size);
    asm("ecall");
    memcpy(buf, sc->content, size);
}
