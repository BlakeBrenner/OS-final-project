#include "console.h"

#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH   80
#define VGA_HEIGHT  25

struct termbuf {
    char ascii;
    char color;
};

static struct termbuf *const vram = (struct termbuf *)VGA_ADDRESS;
static int cursor_row = 0;
static int cursor_column = 0;

static void scroll(void) {
    for (int row = 1; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            vram[(row - 1) * VGA_WIDTH + col] = vram[row * VGA_WIDTH + col];
        }
    }

    for (int col = 0; col < VGA_WIDTH; col++) {
        vram[(VGA_HEIGHT - 1) * VGA_WIDTH + col].ascii = ' ';
        vram[(VGA_HEIGHT - 1) * VGA_WIDTH + col].color = 7;
    }
}

void console_clear(void) {
    for (int row = 0; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            vram[row * VGA_WIDTH + col].ascii = ' ';
            vram[row * VGA_WIDTH + col].color = 7;
        }
    }
    cursor_row = 0;
    cursor_column = 0;
}

void console_init(void) {
    console_clear();
}

int console_putc(int ch) {
    if (ch == '\b') {
        console_backspace();
        return ch;
    }

    if (ch == '\n') {
        cursor_row++;
        cursor_column = 0;
    } else {
        vram[cursor_row * VGA_WIDTH + cursor_column].ascii = (char)ch;
        vram[cursor_row * VGA_WIDTH + cursor_column].color = 7;
        cursor_column++;
    }

    if (cursor_column >= VGA_WIDTH) {
        cursor_column = 0;
        cursor_row++;
    }

    if (cursor_row >= VGA_HEIGHT) {
        scroll();
        cursor_row = VGA_HEIGHT - 1;
    }

    return ch;
}

void console_backspace(void) {
    if (cursor_column == 0 && cursor_row == 0) {
        return;
    }

    if (cursor_column == 0) {
        cursor_row--;
        cursor_column = VGA_WIDTH - 1;
    } else {
        cursor_column--;
    }

    vram[cursor_row * VGA_WIDTH + cursor_column].ascii = ' ';
    vram[cursor_row * VGA_WIDTH + cursor_column].color = 7;
}

void console_write(const char *s) {
    while (*s) {
        console_putc(*s++);
    }
}

void console_writeln(const char *s) {
    console_write(s);
    console_putc('\n');
}
