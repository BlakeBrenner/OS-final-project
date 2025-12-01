#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
void keyboard_interrupt_handler(void);
int keyboard_poll_char(char *out_char);

#endif // KEYBOARD_H
