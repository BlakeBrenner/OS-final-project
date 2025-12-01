#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
void keyboard_handle_scancode(uint8_t scancode);
char keyboard_read_char(void);
int keyboard_has_char(void);

#endif
