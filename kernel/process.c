#include "process.h"

pcb_t process_table[MAX_PROCESSES];
int   process_count = 0;

static uint8_t stacks[MAX_PROCESSES][STACK_SIZE];

static uint32_t build_initial_stack(int slot, void (*entry)(void)) {
    uint32_t *sp = (uint32_t *)(stacks[slot] + STACK_SIZE);

    *(--sp) = (uint32_t)entry;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;

    return (uint32_t)sp;
}

int create_process(const char *name, void (*entry)(void), int priority) {
    if (process_count >= MAX_PROCESSES) return -1;

    int slot = process_count++;
    pcb_t *p = &process_table[slot];

    p->pid        = slot + 1;
    p->state      = PROC_READY;
    p->eip        = (uint32_t)entry;
    p->priority   = priority;
    p->esp        = build_initial_stack(slot, entry);
    p->parent_pid = 0;

    int i = 0;
    while (name[i] && i < 31) { p->name[i] = name[i]; i++; }
    p->name[i] = '\0';

    return p->pid;
}

pcb_t *process_get(int pid) {
    for (int i = 0; i < process_count; i++)
        if (process_table[i].pid == pid) return &process_table[i];
    return 0;
}
