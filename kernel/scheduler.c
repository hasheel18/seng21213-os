#include "scheduler.h"
#include "process.h"

extern void context_switch(pcb_t *cur, pcb_t *next);

static int current_index = -1;
static pcb_t boot_pcb;

void scheduler_init(void) {
    if (process_count == 0) return;
    current_index = 0;
    process_table[0].state = PROC_RUNNING;
}

void scheduler_start(void) {
    if (current_index < 0) return;
    context_switch(&boot_pcb, &process_table[current_index]);
}

pcb_t *scheduler_current(void) {
    if (current_index < 0) return 0;
    return &process_table[current_index];
}

void schedule(void) {
    if (process_count <= 1) return;

    pcb_t *cur = &process_table[current_index];
    if (cur->state == PROC_RUNNING) cur->state = PROC_READY;

    int next_index = current_index;
    for (int i = 0; i < process_count; i++) {
        next_index = (next_index + 1) % process_count;
        if (process_table[next_index].state == PROC_READY) break;
    }

    pcb_t *next = &process_table[next_index];
    next->state = PROC_RUNNING;

    int prev_index = current_index;
    current_index = next_index;

    context_switch(&process_table[prev_index], next);
}
