#ifndef MUTEX_H
#define MUTEX_H
#include "../include/types.h"

typedef struct {
    volatile int locked;
    int waitq[8];
    int wait_count;
} mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

#endif
