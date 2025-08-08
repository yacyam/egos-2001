/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: the process management system server
 * Handle the creation and termination of other processes.
 */

#include "app.h"
#include "elf.h"
#include "disk.h"
#include "process.h"
#include <string.h>

static int app_ino, app_pid;
static void sys_spawn(uint base);
static int app_spawn(struct proc_request* req);

void _fork_copy_segment(void *segment_sender, void *segtbl_child) {
    if (segment_sender == EGOSNULL || segtbl_child == EGOSNULL)
        FATAL("sys_process: segtbl copy gone wrong");
    if (queue_push(segtbl_child, segment_sender) < 0)
        FATAL("sys_process: pushing segment onto segtbl of child failed");
}

struct multicore {
    int boot_lock, booted_core_cnt; /* See earth/boot.s */
};

int main(int unused, struct multicore* boot) {
    SUCCESS("Enter kernel process GPID_PROCESS");

    /* Student's code goes here (Multicore & Locks). */

    /* Release the boot lock, so the other 3 cores can start
     * to run; Wait for all the 4 cores to finish booting. */

    /* Student's code ends here. */

    int sender, shell_waiting;
    char buf[SYSCALL_MSG_LEN];

    sys_spawn(SYS_TERM_EXEC_START);
    grass->sys_recv(GPID_TERMINAL, NULL, buf, SYSCALL_MSG_LEN);
    INFO("sys_process receives: %s", buf);

    sys_spawn(SYS_FILE_EXEC_START);
    grass->sys_recv(GPID_FILE, NULL, buf, SYSCALL_MSG_LEN);
    INFO("sys_process receives: %s", buf);

    sys_spawn(SYS_SHELL_EXEC_START);

    while (1) {
        struct proc_request* req = (void*)buf;
        struct proc_reply* reply = (void*)buf;
        grass->sys_recv(GPID_ALL, &sender, buf, SYSCALL_MSG_LEN);

        switch (req->type) {
        case PROC_SPAWN:
            reply->type = app_spawn(req);

            shell_waiting =
                (req->argv[req->argc - 1][0] != '&') && (reply->type == CMD_OK);
            if (!shell_waiting && reply->type == CMD_OK)
                INFO("process %d running in the background", app_pid);
            grass->sys_send(GPID_SHELL, (void*)reply, sizeof(*reply));
            break;
        case PROC_EXIT:
            grass->proc_free(sender);

            if (shell_waiting && app_pid == sender)
                grass->sys_send(GPID_SHELL, (void*)reply, sizeof(*reply));
            else if (app_pid == sender)
                INFO("background process %d terminated", sender);
            break;
        case PROC_KILLALL:
            grass->proc_free(GPID_ALL);
            break;
        /* Student's code goes here (System Call & Protection). */

        /* Add a case which handles process sleep. */

        /* Student's code ends here. */

        case PROC_FORK:
            /**
             * Workflow of Forking in EGOS-2000: Current Setup
             * 
             * A simple process model is <regs, memory, OS state>. Each should be copied.
             * 
             * sender's kernel stack [ regs | trap/kernel/excp_entry | syscall | rpc | send + recv ]
             * 
             * copying sender's kernel stack to child handles <regs> and some of <OS state>.
             * Now need to also handle the fact that child will now send + recv once scheduled
             * (more OS state management). Note that the senderQ of the sibling won't be copied
             * to the child, as that would completely blow up the scheduler (think about why...). 
             * But the segment table should be copied.
             * 
             * For memory, there are two approaches:
             * 1. For each mapped page in sender's page table, alloc new frame and copy contents of page into child
             * 2. Only copy the page table, but mark every PTE as read-only. Copy-on-write.
             * 
             * (2) is more optimal than (1) in terms of fork latency.
             */

            // get sender's PCB and alloc its child
            struct process *pcb_sender = grass->proc_set_get(sender);
            struct process *pcb_child  = grass->proc_alloc();  

            // UPDATE: DOESNT WORK
            // copy ENTIRE kernel stack + ksp + syscall (now child is also doing RPC)
            //memcpy(pcb_child->kstack, pcb_sender->kstack, SIZE_KSTACK);
            //memcpy(&pcb_child->syscall, &pcb_sender->syscall, sizeof(struct syscall));
            //pcb_child->ksp = pcb_sender->ksp;

            // copy mepc + mstatus (so child resumes at same point as sender)
            pcb_child->mepc = pcb_sender->mepc;
            pcb_child->mstatus = pcb_sender->mstatus;

            // copy entire segment table
            queue_iterate(pcb_sender->segtbl.segments, _fork_copy_segment, pcb_child->segtbl.segments);

            // FOR NOW ASSUME sender is currently blocked waiting to recv a message 
            // from GPID_PROC, and make child also block to recv a message
            if (queue_length(pcb_sender->msgwaitQ) != 1)
                FATAL("sys_process: sender has queue length=%d instead of 1", queue_length(pcb_sender->msgwaitQ));
            
            if (queue_push(pcb_child->msgwaitQ, pcb_child) < 0)
                FATAL("sys_process: child failed to be pushed onto msgwaitQ");

            // handle memory in COW fashion (write protect all pages in both address spaces)
            for (int page = 0; page < NUM_PAGES; page++) {
                if (pcb_sender->pgtbl.tbl[page].present) {
                    // both sender and child should map same frames as RO
                    uint frame_sender = pcb_sender->pgtbl.tbl[page].frame_num;
                    // TODO: map with permissions of segment
                    earth->mmu_map(&pcb_sender->pgtbl.tbl[page], frame_sender, PERMS_RX);
                    earth->mmu_map(&pcb_child->pgtbl.tbl[page], frame_sender, PERMS_RX);
                }
            }
            // parent gets PID of child
            reply->pid = pcb_child->pid;
            reply->type = CMD_OK;
            grass->sys_send(sender, (void*)reply, sizeof(*reply));
            
            // child gets PID of zero
            //reply->pid = 0;
            //grass->sys_send(pcb_child->pid, (void*)reply, sizeof(*reply));
            break;
        default:
            FATAL("sys_process: invalid request %d", req->type);
        }
    }
}

static void app_read(uint off, char* dst) { file_read(app_ino, off, dst); }

static int app_spawn(struct proc_request* req) {
    int bin_ino = dir_lookup(0, "bin/");
    if ((app_ino = dir_lookup(bin_ino, req->argv[0])) < 0) return CMD_ERROR;
    int argc = req->argv[req->argc - 1][0] == '&' ? req->argc - 1 : req->argc;

    struct process *app = grass->proc_alloc();
    elf_setup_user_proc_memory(app, app_ino, argc, req->argv);
    grass->proc_set_ready(app);

    app_pid = app->pid;
    return CMD_OK;
}

static int sys_apps_base;
char* sys_apps[] = {"sys_process", "sys_terminal", "sys_file", "sys_shell"};

static void sys_proc_read(uint block_no, char* dst) {
    earth->disk_read(sys_apps_base + block_no, 1, dst);
}

static void sys_spawn(uint base) {
    struct process *proc_sys = grass->proc_alloc();
    INFO("Load kernel process #%d: %s", proc_sys->pid, sys_apps[proc_sys->pid - 1]);

    sys_apps_base = base;
    elf_setup_kernel_proc_memory(proc_sys, sys_proc_read);
    grass->proc_set_ready(proc_sys);
}
