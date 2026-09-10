#ifndef IRQ_H
#define IRQ_H
#include "../include/types.h"

extern volatile uint32_t timer_ticks;
void pit_init(uint32_t hz);

#endif
