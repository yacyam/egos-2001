/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: wrapping the CPU interface for memory management unit (MMU)
 * This file contains functions for memory allocation/free, virtual memory
 * address translation (software TLB and page table), and cache flushing.
 */

#include "egos.h"
#include "mmu.h"
#include "kmem.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define PAGE_NO_TO_ADDR(x)     (char*)(x * PAGE_SIZE)
#define COREMAP_IDX_TO_ADDR(x) ((char*)APPS_FRAMES_BASE + x * PAGE_SIZE)
#define APPS_FRAMES_CNT        (RAM_END - APPS_FRAMES_BASE) / PAGE_SIZE

// maintains metadata on each physical frame in memory
coremap_entry coremap[APPS_FRAMES_CNT];

// returns a frame number that has a reference count of zero
int frame_alloc() {
    for (int frame_num = 0; frame_num < APPS_FRAMES_CNT; frame_num++)
        if (coremap[frame_num].refcnt == 0)
            return frame_num;
    FATAL("frame_alloc: no more free frames");
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define NUM_PMPSLOTS         NUM_PAGES
#define NUM_PMPSLOTS_PER_REG 4

#define PMPCFG_START         0x3A0
#define PMPADDR_START        0x3B0

#define PMP_34BIT_CONVERT()

/**
 * lifted from https://github.com/ultraembedded/FPGAmp/blob/master/firmware/arch/riscv/csr.h#L45
 * 
 * reg: integer address of CSR
 * val: value to place inside CSR
 */
#define csr_write(reg, val) ({ \
  asm volatile ("csrw " #reg ", %0" :: "rK"(val)); })

void _pmp_addr_set(uint idx) {
    if (idx >= NUM_PMPSLOTS)
        FATAL("_pmp_addr_set: pmp slot %d is out of range of %d slots", idx, NUM_PMPSLOTS);

    FATAL("_pmp_addr_set: unimplemented");
}

void _pmp_cfg_set(uint idx, uint perms) {
    if (idx >= NUM_PMPSLOTS)
        FATAL("_pmp_cfg_set: attempting to set pmp slot %d which is out of \
            range for %d slots", idx, NUM_PMPSLOTS);
    
    FATAL("_pmp_cfg_set: unimplemented");
}

void _pmp_init() {
    for (int i = 0; i < NUM_PMPSLOTS; i++) {
        _pmp_addr_set(i);
        _pmp_cfg_set(i, PERMS_NONE);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void ppt_unmap(ppte *pte) {
    if (!pte->present) FATAL("ppt_unmap: cannot unmap page that isn't there");

    coremap[pte->frame_num].refcnt--;
    memset(pte, 0, sizeof(*pte));
}

void ppt_map(ppte *pte, uint frame_num, int perms) {
    if (pte->present) FATAL("ppt_map: handle present case");
    if (frame_num >= APPS_FRAMES_CNT) FATAL("ppt_map: frame %x too large", frame_num);

    *pte = (ppte) {
        .frame_num = frame_num,
        .perms = perms, .present = egostrue
    };
    coremap[frame_num].refcnt++;
}

void ppt_switch(pseudopgtbl *pgtbl_old, pseudopgtbl *pgtbl_new) {
    FATAL("ppt_switch: unimplemented");
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void mmu_init() {
    // ptmap, ptunmap, ptswitch
    // frame_alloc
    earth->mmu_alloc  = frame_alloc;
    earth->mmu_map    = ppt_map;
    earth->mmu_unmap  = ppt_unmap;
    earth->mmu_switch = ppt_switch;

    // TODO: finish initializing PMP registers
    //_pmp_init();
}
