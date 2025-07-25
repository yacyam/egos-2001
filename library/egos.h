#pragma once
#include "types.h"
#include "mmu.h"

struct earth {
    void (*timer_reset)(uint core_id);

    int  (*mmu_alloc)();
    void (*mmu_map)(ppte *pte, uint frame_num, int perms);
    void (*mmu_unmap)(ppte *pte);
    void (*mmu_switch)(pseudopgtbl *pgtbl_old, pseudopgtbl *pgtbl_new);

    void (*tty_read)(char* c);
    void (*tty_write)(char c);
    uint (*tty_input_empty)();
    void (*disk_read)(uint block_no, uint nblocks, char* dst);
    void (*disk_write)(uint block_no, uint nblocks, char* src);

    enum { ARTY, QEMU } platform;
    enum { PAGE_TABLE, SOFT_TLB } translation;
};

struct grass {
    struct process *(*proc_alloc)();
    void (*proc_set_ready)(struct process *proc);
    void (*proc_free)(int pid);


    void (*sys_send)(int receiver, char* msg, uint size);
    void (*sys_recv)(int from, int* sender, char* buf, uint size);
    void (*sys_rpc)(int receiver, char *buf, uint size);
    
    // so sys-proc uses the same version of egosalloc as the kernel
    void *(*sys_egosalloc)(uint size);
    void *(*sys_egozalloc)(uint size);
    void (*sys_egosfree)(void *ptr);
};

extern struct earth* earth;
extern struct grass* grass;

/* Below is the physical memory layout in egos-2000. */
#define HEAP_END          0x82000000
#define HEAP_START        0x81000000 /* 16MB HEAP */
#define RAM_END           0x81000000 /* 16MB memory [0x80000000,0x81000000) */
#define APPS_FRAMES_BASE  0x80800000 /* 8MB free for mmu_alloc              */

// end of app pages
#define APPS_STACK_TOP    0x80410000 /* 6 page app stack (growing down)     */
#define SYSCALL_ARG       0x8040B000 /* struct syscall                      */
#define APPS_ARG          0x8040A020 /* main() arguments (argc and argv)    */
#define SHELL_WORK_DIR    0x8040A000 /* current work directory for shell    */
#define APPS_STACK_BASE   0x8040A000

#define APPS_ENTRY        0x80400000 /* 10 pages of app code + data + heap  */
// start of app pages

#define BOOT_STACK_TOP    0x80400000 /* 2MB boot stack (growing down)       */
#define GRASS_STRUCT_BASE 0x80201000 /* struct grass                        */
#define EARTH_STRUCT_BASE 0x80200000 /* struct earth                        */
#define RAM_START         0x80000000 /* 2MB egos code and data              */
#define BOARD_FLASH_ROM   0x20400000 /* 4MB disk image on Arty board ROM    */

/* Below is the memory-mapped I/O layout in egos-2000. */
#define ETHMAC_CSR_BASE  0xF0002000
#define ETHMAC_RX_BUFFER 0x90000000
#define ETHMAC_TX_BUFFER 0x90001000
#define SPI_BASE         (earth->platform == ARTY ? 0xF0008800UL : 0x10050000UL)
#define UART_BASE        (earth->platform == ARTY ? 0xF0001000UL : 0x10010000UL)
#define CLINT_BASE       (earth->platform == ARTY ? 0xF0010000UL : 0x02000000UL)

// a real address is a 32 bit value that references physical memory

// every address a process will reference is inside of 0x80400000 - 0x80440000
#define REAL_ADDR_TO_PAGE_NUM(addr) (((addr - APPS_ENTRY) >> PAGE_NBITS))
#define PAGE_NUM_TO_REAL_ADDR(page) (((page << PAGE_NBITS) + APPS_ENTRY))

// every address referencing a frame is inside the region 0x80800000 - 0x81000000
#define REAL_ADDR_TO_FRAME_NUM(addr)  (((addr - APPS_FRAMES_BASE) >> PAGE_NBITS))
#define FRAME_NUM_TO_REAL_ADDR(frame) (((frame << PAGE_NBITS) + APPS_FRAMES_BASE))

#define REAL_ADDR_TRUNC_OFFSET(addr) ((addr >> PAGE_NBITS))

/* Below are some common macros/declarations for I/O, multicore and printing. */
static inline int ceiling(int num, int den) { return (num + den - 1) / den; }

#define ACCESS(x)          (*(__typeof__(*x) volatile*)(x))
#define REGW(base, offset) (ACCESS((uint*)(base + offset)))
#define REGB(base, offset) (ACCESS((uchar*)(base + offset)))

#define NCORES     4
#define release(x) __sync_lock_release(&x);
#define acquire(x) while (__sync_lock_test_and_set(&x, 1) != 0);
extern int boot_lock, kernel_lock, booted_core_cnt;

#define printf my_printf
int INFO(const char* format, ...);
int FATAL(const char* format, ...);
int SUCCESS(const char* format, ...);
int CRITICAL(const char* format, ...);
int my_printf(const char* format, ...);
