#include "timer.h"
#include "interrupt.h"

#define PIT_FREQUENCY 1193182u

static volatile uint64_t tick_count = 0;
static uint32_t current_frequency = 0;

static void program_pit(uint32_t freq_hz) {
    if (freq_hz == 0) {
        return;
    }
    uint32_t divisor = PIT_FREQUENCY / freq_hz;
    outb(0x43, 0x36); // channel 0, lobyte/hibyte, mode 3
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

void timer_init(uint32_t frequency_hz) {
    current_frequency = frequency_hz;
    program_pit(frequency_hz);
}

void timer_interrupt_handler(void) {
    tick_count++;
}

uint64_t timer_ticks(void) {
    return tick_count;
}

uint32_t timer_frequency(void) {
    return current_frequency;
}
