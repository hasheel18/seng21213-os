/* =============================================================================
 * SENG21213-OS :: Mutex (L10 Section 3 - Mutual Exclusion)
 * File: kernel/mutex.c
 *
 * On a single CPU core, disabling interrupts (cli/sti) around the lock's own
 * bookkeeping is itself a valid, classic mutual-exclusion primitive (L10 3.2
 * covers hardware CAS/XCHG for multi-core; cli/sti is the uniprocessor
 * equivalent). Blocking is done by marking the waiting thread's PCB
 * PROC_BLOCKED so the round-robin scheduler (Stage 1) simply skips it.
 * ============================================================================*/
#include "mutex.h"
#include "process.h"
#include "scheduler.h"

void mutex_init(mutex_t *m) {
    m->locked = 0;
    m->wait_count = 0;
}

void mutex_lock(mutex_t *m) {
    __asm__ __volatile__("cli");

    if (!m->locked) {
        m->locked = 1;
        __asm__ __volatile__("sti");
        return;
    }

    pcb_t *cur = scheduler_current();
    if (cur && m->wait_count < 8) {
        m->waitq[m->wait_count++] = cur->pid;
        cur->state = PROC_BLOCKED;
    }
    __asm__ __volatile__("sti");
    schedule();   /* yield - resumes here once woken and rescheduled */
}

void mutex_unlock(mutex_t *m) {
    __asm__ __volatile__("cli");

    if (m->wait_count > 0) {
        int pid = m->waitq[0];
        for (int i = 1; i < m->wait_count; i++) m->waitq[i - 1] = m->waitq[i];
        m->wait_count--;

        pcb_t *p = process_get(pid);
        if (p) p->state = PROC_READY;   /* lock ownership hands directly to woken thread */
    } else {
        m->locked = 0;
    }

    __asm__ __volatile__("sti");
}
