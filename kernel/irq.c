#include "irq.h"
#include "io.h"
#include "scheduler.h"

#define PIT_CHANNEL0   0x40
#define PIT_COMMAND    0x43
#define PIT_FREQUENCY  1193182u

volatile uint32_t timer_ticks = 0;

void pit_init(uint32_t hz) {
    uint16_t divisor = (uint16_t)(PIT_FREQUENCY / hz);
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

void irq0_handler(void) {
    timer_ticks++;
    outb(0x20, 0x20);
    schedule();
}
