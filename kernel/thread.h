#ifndef THREAD_H
#define THREAD_H
#include "../include/types.h"

int create_thread(const char *name, void (*entry)(void), int priority, int parent_pid);

#endif
