/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: ELF-format executable file loader
 */

#include "egos.h"
#include "elf.h"
#include "disk.h"
#include "servers.h"
#include <string.h>

// basically swap bits 0 and 2
static inline int __convert_elf_flags_into_perms(int p_flags) {
    return (BIT(p_flags, 0) << 2) | (BIT(p_flags, 1)) | (BIT(p_flags, 2) >> 2);
}

static uint __alloc_frame_and_map_page(struct process *proc, uint page, int perms) {
    if (page >= NUM_PAGES) FATAL("alloc_frame_and_map_page: page %x too large", page);

    earth->mmu_map(&proc->pgtbl.tbl[page], earth->mmu_alloc(), perms);
    return proc->pgtbl.tbl[page].frame_num;
}

// nothing is loaded in initially (except for arguments). on-demand paging!
void elf_setup_user_proc_memory(struct process *proc, uint ino, int argc, char argv[CMD_NARGS][CMD_ARG_LEN]) {
    char hbuf[BLOCK_SIZE], buf[BLOCK_SIZE];
    uint frame;

    file_read(ino, 0, hbuf);
    struct elf32_header* header          = (void*)hbuf;
    struct elf32_program_header* pheader = (void*)(hbuf + header->e_phoff);

    for (int i = 1; i < header->e_phnum; i++) {
        if (!pheader[i].p_memsz)
            continue;

        segment *prog_segment = grass->sys_egosalloc(sizeof(segment));
        *prog_segment = (segment) {
            .page_base = REAL_ADDR_TO_PAGE_NUM(pheader[i].p_vaddr),
            .num_pages = ceiling(pheader[i].p_memsz, PAGE_SIZE), 
            .perms_max = __convert_elf_flags_into_perms(pheader[i].p_flags),
            // file stuff
            .in_file = (pheader[i].p_filesz > 0),
            .ino     = ino, 
            .offset  = pheader[i].p_offset / BLOCK_SIZE
        };

        list_append(proc->segtbl.segments, prog_segment);
    }

    // Setup STACK!! and ARGC/ARGV (at end of stack)

    // handles userspace copying of syscall arguments nicely
    __alloc_frame_and_map_page(proc, REAL_ADDR_TO_PAGE_NUM(SYSCALL_ARG), PERMS_RW);
    proc->pgtbl.tbl[REAL_ADDR_TO_PAGE_NUM(SYSCALL_ARG)].pinned = egostrue;

    // set up one page for APPS_ARGS (argv + argc)
    frame = __alloc_frame_and_map_page(proc, REAL_ADDR_TO_PAGE_NUM(APPS_ARG), PERMS_RW);

    uint addr_real_argc = FRAME_NUM_TO_REAL_ADDR(frame) + REAL_ADDR_GET_OFFSET(APPS_ARG);
    *((int*)addr_real_argc) = argc;

    // the pointers have to be mirrored in frame AND page!
    void **addr_real_argvptrs = (void**)(addr_real_argc + sizeof(int));
    void *addr_real_argvvals = (void*)((uint)addr_real_argvptrs + CMD_NARGS*sizeof(void*));
    void *addr_virt_argvvals = (void*)(APPS_ARG + sizeof(int) + CMD_NARGS*sizeof(void*));
    for (int i = 0; i < argc; i++) {
        addr_real_argvptrs[i] = addr_virt_argvvals;
        memcpy(addr_real_argvvals, argv[i], sizeof(argv[i]));

        addr_real_argvvals = (void*)((uint)addr_real_argvvals + CMD_ARG_LEN);
        addr_virt_argvvals = (void*)((uint)addr_virt_argvvals + CMD_ARG_LEN);
    }

    // push stack segment into segment table
    segment *stack_segment = grass->sys_egosalloc(sizeof(segment));
    *stack_segment = (segment) {
        .page_base = REAL_ADDR_TO_PAGE_NUM(APPS_STACK_BASE),
        .num_pages = (APPS_STACK_TOP - APPS_STACK_BASE) / PAGE_SIZE,
        .perms_max = PERMS_RW,
        .in_file = egosfalse
    };
    list_append(proc->segtbl.segments, stack_segment);
}

// kernel processes cannot experience "page faults", so don't need to set up
// segment table, and load in entire memory (map all pages)
void elf_setup_kernel_proc_memory(struct process *proc, elf_reader reader) {
    /* Load the ELF header. */
    char hbuf[BLOCK_SIZE], buf[BLOCK_SIZE];
    uint frame;

    reader(0, hbuf);
    struct elf32_header* header          = (void*)hbuf;
    struct elf32_program_header* pheader = (void*)(hbuf + header->e_phoff);

    // first program header is special RISCV SHT_RISCV_ATTRIBUTES (section-header tbl). skip it.
    for (int i = 1; i < header->e_phnum; i++) {
        int perms = __convert_elf_flags_into_perms(pheader[i].p_flags);
        uint page = REAL_ADDR_TO_PAGE_NUM(pheader[i].p_vaddr);
        uint block_no = pheader[i].p_offset / BLOCK_SIZE; // byte offset to block offset

        // every segment is set up to either completely be inside or outside a file (in app.lds)
        // TODO: Do better job of handling non-page-aligned memsz.
        for (uint _ = 0; _ < pheader[i].p_memsz; _ += PAGE_SIZE) {
            frame = __alloc_frame_and_map_page(proc, page++, perms);

            if (!pheader[i].p_filesz) {
                memset((void*)FRAME_NUM_TO_REAL_ADDR(frame), 0, PAGE_SIZE);
                continue;
            }

            for (uint off = 0; off < PAGE_SIZE; off += BLOCK_SIZE)
                reader(block_no++, (char*)(FRAME_NUM_TO_REAL_ADDR(frame) + off));
        }
    }

    // setup 6 pages for stack
    uint page_stack_base = REAL_ADDR_TO_PAGE_NUM(APPS_STACK_BASE);
    while (page_stack_base < REAL_ADDR_TO_PAGE_NUM(APPS_STACK_TOP)) {
        frame = __alloc_frame_and_map_page(proc, page_stack_base++, PERMS_RW);
        memset((void*)FRAME_NUM_TO_REAL_ADDR(frame), 0, PAGE_SIZE);
    }
}
