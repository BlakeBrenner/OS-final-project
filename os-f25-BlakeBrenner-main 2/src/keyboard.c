#include "keyboard.h"
#include "console.h"
#include "rprintf.h"

#define BUFFER_SIZE 128

static unsigned char keyboard_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', '\b',
    '\t',
    'q', 'w', 'e', 'r',
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n',
    'm', ',', '.', '/', 0,
    '*',
    0,
    ' ',
    0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0,
    0,
    0,
    0,
    0,
    '-',
    0,
    0,
    0,
    '+',
    0,
    0,
    0,
    0,
    0,
    0, 0, 0,
    0,
    0,
    0
};

static volatile char ring_buffer[BUFFER_SIZE];
static volatile int head = 0;
static volatile int tail = 0;

void keyboard_init(void) {
    head = tail = 0;
}

static int buffer_empty(void) {
    return head == tail;
}

static int buffer_full(void) {
    return ((head + 1) % BUFFER_SIZE) == tail;
}

int keyboard_has_char(void) {
    return !buffer_empty();
}

char keyboard_read_char(void) {
    while (buffer_empty()) {
        asm volatile("hlt");
    }

    char c = ring_buffer[tail];
    tail = (tail + 1) % BUFFER_SIZE;
    return c;
}

void keyboard_handle_scancode(uint8_t scancode) {
    if (scancode & 0x80) {
        return; // ignore key releases
    }

    char ascii = 0;
    if (scancode < sizeof(keyboard_map)) {
        ascii = keyboard_map[scancode];
    }

    if (!ascii) return;

    if (buffer_full()) {
        return;
    }

    ring_buffer[head] = ascii;
    head = (head + 1) % BUFFER_SIZE;
}
