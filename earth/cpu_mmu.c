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

#define APPS_FRAMES_CNT (RAM_END - APPS_FRAMES_BASE) / PAGE_SIZE

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

void _pmp_init() {
    // map every page in a process' memory with no perms
    for (int page = 0; page < NUM_PAGES; page++) {
        uint addr_from_page_as_napot = PMP_34BIT_CONVERT(
            PAGE_NUM_TO_REAL_ADDR(page) | ((PAGE_SIZE - 1) >> 1)
        );
        _pmp_addr_write(page, addr_from_page_as_napot);
        _pmp_cfg_set(page, PMP_REGION_NAPOT | PERMS_NONE);
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
    if (pgtbl_new == EGOSNULL)
        FATAL("????");

    /** TODO: set up pmp registers */
    for (uint page = 0; page < NUM_PAGES; page++) {
        if (pgtbl_old && pgtbl_old->tbl[page].present)
            memcpy((void*)FRAME_NUM_TO_REAL_ADDR(pgtbl_old->tbl[page].frame_num), \
                (void*)PAGE_NUM_TO_REAL_ADDR(page), PAGE_SIZE);
    }

    for (uint page = 0; page < NUM_PAGES; page++) {
        if (pgtbl_new->tbl[page].present) {
            memcpy((void*)PAGE_NUM_TO_REAL_ADDR(page), \
                (void*)FRAME_NUM_TO_REAL_ADDR(pgtbl_new->tbl[page].frame_num), PAGE_SIZE);

            if (pgtbl_new->tbl[page].perms >= 0b111)
                FATAL("ppt_switch: perms %x invalid", pgtbl_new->tbl[page].perms);

            _pmp_cfg_set(page, PMP_REGION_NAPOT | pgtbl_new->tbl[page].perms);
        }
        else {
            _pmp_cfg_set(page, PMP_REGION_NAPOT | PERMS_NONE);
        }
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void mmu_init() {
    // ptmap, ptunmap, ptswitch
    // frame_alloc
    earth->mmu_alloc  = frame_alloc;
    earth->mmu_map    = ppt_map;
    earth->mmu_unmap  = ppt_unmap;
    earth->mmu_switch = ppt_switch;

    _pmp_init();
}
