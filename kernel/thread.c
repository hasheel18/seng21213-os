/* =============================================================================
 * SENG21213-OS :: Kernel Threads (L10 Section 1)
 * File: kernel/thread.c
 *
 * This kernel has a single flat address space (no paging - see L11), so
 * "threads sharing the process's address space" (L10 1.1) is automatically
 * true for every schedulable entity. Threads therefore reuse the exact same
 * PCB / scheduler / context_switch machinery built in Stage 1, tagged with
 * the pid of their logical parent process via pcb_t.parent_pid.
 * ============================================================================*/
#include "thread.h"
#include "process.h"

int create_thread(const char *name, void (*entry)(void), int priority, int parent_pid) {
    int pid = create_process(name, entry, priority);
    if (pid < 0) return -1;

    pcb_t *p = process_get(pid);
    if (p) p->parent_pid = parent_pid;

    return pid;
}
