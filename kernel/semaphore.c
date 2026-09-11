/* =============================================================================
 * SENG21213-OS :: Counting Semaphore (L10 Section 3 / Stallings 5.11)
 * File: kernel/semaphore.c
 * ============================================================================*/
#include "semaphore.h"
#include "process.h"
#include "scheduler.h"

void sem_init(sem_t *s, int initial) {
    s->count = initial;
    s->wait_count = 0;
}

void sem_wait(sem_t *s) {
    __asm__ __volatile__("cli");
    s->count--;

    if (s->count < 0) {
        pcb_t *cur = scheduler_current();
        if (cur && s->wait_count < 8) {
            s->waitq[s->wait_count++] = cur->pid;
            cur->state = PROC_BLOCKED;
        }
        __asm__ __volatile__("sti");
        schedule();
        return;
    }

    __asm__ __volatile__("sti");
}

void sem_signal(sem_t *s) {
    __asm__ __volatile__("cli");
    s->count++;

    if (s->count <= 0 && s->wait_count > 0) {
        int pid = s->waitq[0];
        for (int i = 1; i < s->wait_count; i++) s->waitq[i - 1] = s->waitq[i];
        s->wait_count--;

        pcb_t *p = process_get(pid);
        if (p) p->state = PROC_READY;
    }

    __asm__ __volatile__("sti");
}
