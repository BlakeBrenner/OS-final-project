#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

// Run the interactive kernel shell loop.
void shell_run(void);

// Utility exposed for commands
void clear_screen(void);
int putc(int ch);

// Hook for input
int shell_poll_char(char *out_char);

#endif // SHELL_H
