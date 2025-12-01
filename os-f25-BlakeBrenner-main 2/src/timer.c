#include "timer.h"
#include <stdint.h>

#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40
#define PIT_BASE_FREQUENCY 1193182

extern void outb(uint16_t _port, uint8_t val);

static volatile uint32_t tick_count = 0;
static uint32_t configured_hz = 0;

void timer_init(uint32_t frequency_hz) {
    configured_hz = frequency_hz;
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency_hz;
    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL0_PORT, divisor & 0xFF);
    outb(PIT_CHANNEL0_PORT, (divisor >> 8) & 0xFF);
}

void timer_handle_tick(void) {
    tick_count++;
}

uint32_t timer_ticks(void) {
    return tick_count;
}

uint32_t timer_milliseconds(void) {
    if (configured_hz == 0) return 0;
    return (tick_count * 1000u) / configured_hz;
}
