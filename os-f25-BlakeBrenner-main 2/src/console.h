#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

void console_init(void);
void console_clear(void);
int console_putc(int ch);
void console_write(const char *s);
void console_writeln(const char *s);
void console_backspace(void);

#endif
