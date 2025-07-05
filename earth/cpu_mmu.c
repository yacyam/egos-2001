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
#include <string.h>

#define PAGE_NO_TO_ADDR(x) (char*)(x * PAGE_SIZE)
#define PAGE_ID_TO_ADDR(x) ((char*)APPS_FRAMES_BASE + x * PAGE_SIZE)
#define APPS_FRAMES_CNT    (RAM_END - APPS_FRAMES_BASE) / PAGE_SIZE

// maintains metadata on each physical frame in memory
coremap_entry coremap[APPS_FRAMES_CNT];

// returns a frame number that has a reference count of zero
uint frame_alloc() {
    FATAL("frame_alloc: unimplemented");
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void _pmp_init() {
    FATAL("_pmp_init: unimplemented");
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void ppt_unmap(ppte pte) {
    FATAL("ppt_unmap: unimplemented");
}

void ppt_map(ppte pte, uint frame_num, uint perms) {
    FATAL("ppt_map: unimplemented");
}

void ppt_switch(pseudopgtbl pgtbl_old, pseudopgtbl pgtbl_new) {
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


    /* Setup a PMP region for the whole 4GB address space. */
    asm("csrw pmpaddr0, %0" : : "r"(0x40000000));
    asm("csrw pmpcfg0, %0" : : "r"(0xF));

    FATAL("mmu_init: set up state for pmp registers");

}
