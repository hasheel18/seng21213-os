#ifndef SEMAPHORE_H
#define SEMAPHORE_H
#include "../include/types.h"

typedef struct {
    volatile int count;
    int waitq[8];
    int wait_count;
} sem_t;

void sem_init(sem_t *s, int initial);
void sem_wait(sem_t *s);
void sem_signal(sem_t *s);

#endif
