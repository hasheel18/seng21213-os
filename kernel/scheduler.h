#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "process.h"

void   scheduler_init(void);
void   scheduler_start(void);
void   schedule(void);
pcb_t *scheduler_current(void);

#endif
