/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: grass layer initialization
 * Spawn the first process, GPID_PROCESS (pid=1).
 */

#include "process.h"
#include "elf.h"
#include "queue.h"
#include "list.h"

/*
#include <stdlib.h>
void memwrite(void *p, char c, int size) {
    char *ptr = p;
    for (int i = 0; i < size; i++) ptr[i] = c;
}

void memread(void *p, char c, int size) {
    char *ptr = p;
    for (int i = 0; i < size; i++)
        if (ptr[i] != c)
            FATAL("memread: fail!");
}

// if this works, probably right
void malloc_stresstest() {
    #define NUM_REGIONS 128
    #define REGION_MAXSZ 0x1000
    #define NUM_ITERATIONS 200
    void *arr[NUM_REGIONS];
    uint sizes[NUM_REGIONS];
    char chars[NUM_REGIONS];


    for (int _ = 0; _ < NUM_ITERATIONS; _++) {
        for (int i = 0; i < NUM_REGIONS; i++) {
            chars[i] = rand() % sizeof(char);
            sizes[i] = rand() % REGION_MAXSZ;
            arr[i] = egozalloc(sizes[i]);
        }
    
        for (int i = 0; i < NUM_REGIONS; i++)
            memwrite(arr[i], chars[i], sizes[i]);
        for (int i = 0; i < NUM_REGIONS; i++)
            memread(arr[i], chars[i], sizes[i]);

        for (int i = 0; i < NUM_REGIONS; i++)
            egosfree(arr[i]);
    }
}
*/

extern list_t proc_set;
extern queue_t runQ, readyQ;
extern struct process *proc_curr;

static void sys_proc_read(uint block_no, char* dst) {
    earth->disk_read(SYS_PROC_EXEC_START + block_no, 1, dst);
}

void grass_entry() {
    SUCCESS("Enter the grass layer");

    /* Initialize the grass interface. */
    grass->proc_free      = proc_free;
    grass->proc_alloc     = proc_alloc;
    grass->proc_set_ready = proc_set_ready;
    grass->sys_send       = sys_send;
    grass->sys_recv       = sys_recv;
    /* Student's code goes here (System Call | Multicore & Locks). */

    /* Initialize the grass interface for proc_sleep() or proc_coresinfo(). */

    /* Student's code ends here. */

    /* Load GPID_PROCESS. */
    INFO("Load kernel process #%d: sys_process", GPID_PROCESS);
    elf_load(GPID_PROCESS, sys_proc_read, 0, 0);

    /* create kernel data structures */
    if ((proc_set = list_new()) == EGOSNULL)
        FATAL("grass_entry: failed to create proc_set");
    if ((runQ = queue_new()) == EGOSNULL)
        FATAL("grass_entry: failed to create runQ");
    if ((readyQ = queue_new()) == EGOSNULL)
        FATAL("grass_entry: failed to create readyQ");

    proc_curr = proc_alloc();
    earth->mmu_switch(GPID_PROCESS);
    earth->mmu_flush_cache();

    uint mstatus, M_MODE = 3, U_MODE = 0;
    uint GRASS_MODE = (earth->translation == SOFT_TLB) ? M_MODE : U_MODE;
    asm("csrr %0, mstatus" : "=r"(mstatus));
    mstatus = (mstatus & ~(3 << 11)) | (GRASS_MODE << 11);
    asm("csrw mstatus, %0" ::"r"(mstatus));

    asm("csrw mepc, %0" ::"r"(APPS_ENTRY));
    asm("csrw mscratch, %0"::"r"(proc_curr->ksp)); // for kernel stack switch on trap entry
    asm("mv a0, %0" ::"r"(APPS_ARG));
    asm("mv a1, %0" ::"r"(&boot_lock));
    asm("mret");
    /* If using page table translation, the CPU will enter the user mode after
     * this mret and thus page table translation will start to take effect. */
}
