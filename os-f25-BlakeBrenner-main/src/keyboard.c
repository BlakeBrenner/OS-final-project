#include "keyboard.h"
#include "interrupt.h"

#define KBD_BUF_SIZE 128

static volatile char buffer[KBD_BUF_SIZE];
static volatile unsigned int head = 0;
static volatile unsigned int tail = 0;

static const unsigned char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',     /* 9 */
    '9', '0', '-', '=', '\b',     /* Backspace */
    '\t',                 /* Tab */
    'q', 'w', 'e', 'r',   /* 19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', /* Enter key */
      0,                  /* 29   - Control */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',     /* 39 */
    '\'', '`',   0,                /* Left shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n',                    /* 49 */
    'm', ',', '.', '/',   0,                              /* Right shift */
    '*',
      0,  /* Alt */
    ' ',  /* Space bar */
      0,  /* Caps lock */
      0,  /* 59 - F1 key ... > */
      0,   0,   0,   0,   0,   0,   0,   0,
      0,  /* < ... F10 */
      0,  /* 69 - Num lock*/
      0,  /* Scroll Lock */
      0,  /* Home key */
      0,  /* Up Arrow */
      0,  /* Page Up */
    '-',
      0,  /* Left Arrow */
      0,
      0,  /* Right Arrow */
    '+',
      0,  /* 79 - End key*/
      0,  /* Down Arrow */
      0,  /* Page Down */
      0,  /* Insert Key */
      0,  /* Delete Key */
      0,   0,   0,
      0,  /* F11 Key */
      0,  /* F12 Key */
      0   /* All other keys are undefined */
};

static int buffer_is_full(void) {
    return ((head + 1) % KBD_BUF_SIZE) == tail;
}

static int buffer_is_empty(void) {
    return head == tail;
}

void keyboard_init(void) {
    head = tail = 0;
}

static void buffer_push(char c) {
    if (buffer_is_full()) {
        return; // drop on overflow
    }
    buffer[head] = c;
    head = (head + 1) % KBD_BUF_SIZE;
}

int keyboard_poll_char(char *out_char) {
    if (buffer_is_empty()) {
        return 0;
    }
    *out_char = buffer[tail];
    tail = (tail + 1) % KBD_BUF_SIZE;
    return 1;
}

void keyboard_interrupt_handler(void) {
    uint8_t status = inb(0x64);
    if (!(status & 1)) {
        return;
    }

    uint8_t scancode = inb(0x60);
    if (scancode >= 128) {
        return; // ignore releases
    }

    unsigned char mapped = keyboard_map[scancode];
    if (mapped) {
        buffer_push((char)mapped);
    }
}