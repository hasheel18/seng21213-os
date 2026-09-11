#ifndef PROCESS_H
#define PROCESS_H
#include "../include/types.h"

#define MAX_PROCESSES 32
#define STACK_SIZE    4096

typedef enum {
    PROC_READY   = 0,
    PROC_RUNNING = 1,
    PROC_BLOCKED = 2,
    PROC_ZOMBIE  = 3
} proc_state_t;

/* NOTE: 'esp' MUST stay at byte offset 8 - boot/switch.asm reads/writes it
 * directly by that offset. Don't reorder pid/state/esp. */
typedef struct {
    int          pid;
    proc_state_t state;
    uint32_t     esp;
    uint32_t     eip;
    int          priority;
    char         name[32];
    int          parent_pid;   /* 0 = top-level process; nonzero = thread of that pid (L10) */
} pcb_t;

extern pcb_t process_table[MAX_PROCESSES];
extern int   process_count;

int    create_process(const char *name, void (*entry)(void), int priority);
pcb_t *process_get(int pid);

#endif
