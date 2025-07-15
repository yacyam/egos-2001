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

// nothing is loaded in initially. on-demand paging!
void elf_setup_user_proc_memory(struct process *proc, uint ino, int argc, void **argv) {
    FATAL("elf_setup_user_memory: unimplemented, proc=%d, ino=%d", proc->pid, ino);
}

// kernel processes cannot experience "page faults", so don't set up
// segment table, and load in entire memory (map all pages)
void elf_setup_kernel_proc_memory(struct process *proc, elf_reader reader) {
    /* Load the ELF header. */
    char hbuf[BLOCK_SIZE], buf[BLOCK_SIZE];
    reader(0, hbuf);
    struct elf32_header* header          = (void*)hbuf;
    struct elf32_program_header* pheader = (void*)(hbuf + header->e_phoff);

    // first program header is special RISCV SHT_RISCV_ATTRIBUTES. skip it.
    for (int i = 1; i < header->e_phnum; i++) {
        INFO("pheader %d: vaddr=%x, filesz=%x, memsz=%x", i, pheader[i].p_vaddr, pheader[i].p_filesz, pheader[i].p_memsz);
    }

    FATAL("elf_load: unimplemented, proc=%d", proc->pid);
}
