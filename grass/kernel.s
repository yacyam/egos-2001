/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: entry point of the kernel
 * When receiving an interrupt or exception, the CPU sets
 * its program counter to the first instruction of trap_entry.
 */
    .section .text
    .global trap_entry, ctx_switch, ctx_start, ctx_entry, kernel_lock

.macro SAVE_REGS
    sw a0,  0(sp)
    sw a1,  4(sp)
    sw a2,  8(sp)
    sw a3,  12(sp)
    sw a4,  16(sp)
    sw a5,  20(sp)
    sw a6,  24(sp)
    sw a7,  28(sp)
    sw t0,  32(sp)
    sw t1,  36(sp)
    sw t2,  40(sp)
    sw t3,  44(sp)
    sw t4,  48(sp)
    sw t5,  52(sp)
    sw t6,  56(sp)
    sw s0,  60(sp)
    sw s1,  64(sp)
    sw s2,  68(sp)
    sw s3,  72(sp)
    sw s4,  76(sp)
    sw s5,  80(sp)
    sw s6,  84(sp)
    sw s7,  88(sp)
    sw s8,  92(sp)
    sw s9,  96(sp)
    sw s10, 100(sp)
    sw s11, 104(sp)
    sw ra,  108(sp)
    sw gp,  112(sp)
    sw tp,  116(sp)
.endm

.macro RESTORE_REGS
    lw a0,  0(sp)
    lw a1,  4(sp)
    lw a2,  8(sp)
    lw a3,  12(sp)
    lw a4,  16(sp)
    lw a5,  20(sp)
    lw a6,  24(sp)
    lw a7,  28(sp)
    lw t0,  32(sp)
    lw t1,  36(sp)
    lw t2,  40(sp)
    lw t3,  44(sp)
    lw t4,  48(sp)
    lw t5,  52(sp)
    lw t6,  56(sp)
    lw s0,  60(sp)
    lw s1,  64(sp)
    lw s2,  68(sp)
    lw s3,  72(sp)
    lw s4,  76(sp)
    lw s5,  80(sp)
    lw s6,  84(sp)
    lw s7,  88(sp)
    lw s8,  92(sp)
    lw s9,  96(sp)
    lw s10, 100(sp)
    lw s11, 104(sp)
    lw ra,  108(sp)
    lw gp,  112(sp)
    lw tp,  116(sp)
.endm

/*
    ctx_switch(void **old_sp, void **new_sp):
        slightly different (so a process can context switch to itself)
*/
ctx_switch:
    addi sp, sp, -128
    SAVE_REGS
    sw sp, 0(a0)
    lw sp, 0(a1)
    RESTORE_REGS
    addi sp, sp, 128
    ret

/*
    ctx_start(void **old_sp, void *new_sp):
*/
ctx_start:
    addi sp, sp, -128
    SAVE_REGS
    sw sp, 0(a0)
    mv sp, a1
    call ctx_entry

trap_entry:
    csrrw sp, mscratch, sp /* SWAP kernel sp (of the process) and user sp */

    addi sp, sp, -128 /* set up register trap frame on kernel stack of process */
    SAVE_REGS

    csrr t0,  mscratch /* Step1 has written sp to mscratch */
    sw t0,  120(sp)   /* t0 holds the value of the old sp before trap_entry */

    /* invoke the C handler */
    call kernel_entry

    RESTORE_REGS
    /* flush kernel stack (should now be "empty"), and write kernel sp back into mscratch */
    addi sp, sp, 128
    csrw mscratch, sp

    /* load back user stack pointer (pushed onto kernel stack) */
    lw sp, -8(sp)
    mret

.bss
    kernel_lock:     .word 0
