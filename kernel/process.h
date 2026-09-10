#ifndef PROCESS_H
#define PROCESS_H
#include "../include/types.h"

#define MAX_PROCESSES 8
#define STACK_SIZE    4096

typedef enum {
    PROC_READY   = 0,
    PROC_RUNNING = 1,
    PROC_BLOCKED = 2,
    PROC_ZOMBIE  = 3
} proc_state_t;

typedef struct {
    int          pid;
    proc_state_t state;
    uint32_t     esp;
    uint32_t     eip;
    int          priority;
    char         name[32];
} pcb_t;

extern pcb_t process_table[MAX_PROCESSES];
extern int   process_count;

int    create_process(const char *name, void (*entry)(void), int priority);
pcb_t *process_get(int pid);

#endif
