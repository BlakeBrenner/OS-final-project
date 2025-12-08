#include <stdint.h>
#include "rprintf.h"
#include "page.h"
#include "paging.h"
#include "interrupt.h"
#include "shell.h"

#define VIDEO_ADDR 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static int cursor_x = 0;
static int cursor_y = 0;
static volatile uint16_t *video = (volatile uint16_t*)VIDEO_ADDR;

/* ==================== VGA TEXT MODE FUNCTIONS ==================== */

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        video[i] = 0x0720;  // Space with gray on black
    }
    cursor_x = 0;
    cursor_y = 0;
}

static void vga_scroll(void) {
    // Move all lines up
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            video[y * VGA_WIDTH + x] = video[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    // Clear bottom line
    for (int x = 0; x < VGA_WIDTH; x++) {
        video[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = 0x0720;
    }
    
    cursor_y = VGA_HEIGHT - 1;
}

int putc(int ch) {
    if (ch == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT) {
            vga_scroll();
        }
        return ch;
    }
    
    if (ch == '\r') {
        cursor_x = 0;
        return ch;
    }
    
    if (ch == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
        }
        video[cursor_y * VGA_WIDTH + cursor_x] = 0x0720;
        return ch;
    }
    
    if (ch == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= VGA_HEIGHT) {
                vga_scroll();
            }
        }
        return ch;
    }
    
    // Normal printable character
    if (ch >= 32 && ch < 127) {
        video[cursor_y * VGA_WIDTH + cursor_x] = (0x07 << 8) | (unsigned char)ch;
        cursor_x++;
        
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= VGA_HEIGHT) {
                vga_scroll();
            }
        }
    }
    
    return ch;
}

/* ==================== PAGING HELPERS ==================== */

extern char _end_kernel;

uint32_t align_down_page(uint32_t x) {
    return x & ~(PAGE_SIZE-1);
}

/* ==================== MAIN KERNEL ENTRY POINT ==================== */

void main(void) {
    // Clear screen first
    vga_clear();
    
    esp_printf(putc,"Kernel booting...\n");

    /* ---------- interrupts ---------- */
    esp_printf(putc,"Setting up interrupts...\n");
    remap_pic();   
    load_gdt();    
    init_idt();    

    /* ---------- paging setup ---------- */
    esp_printf(putc,"Setting up paging...\n");

    identity_map_range(0x00100000u, (uint32_t)&_end_kernel);

    uint32_t esp_val; 
    __asm__("mov %%esp,%0":"=r"(esp_val));
    uint32_t lo = align_down_page(esp_val) - 7*PAGE_SIZE;
    uint32_t hi = align_down_page(esp_val) + PAGE_SIZE;
    identity_map_range(lo, hi);

    identity_map_range(0x000B8000u, 0x000B8000u + PAGE_SIZE);

    paging_init_recursive(kernel_pd);
    loadPageDirectory(kernel_pd);
    enablePaging();

    esp_printf(putc,"Paging enabled.\n");

    /* ---------- page-frame allocator ---------- */
    esp_printf(putc,"Initializing memory allocator...\n");
    init_pfa_list();
    esp_printf(putc,"Free frames: %d\n", (int)pfa_free_count());

    /* ---------- PIT ---------- */
    esp_printf(putc,"Starting timer...\n");
    pit_init(100);

    /* enable interrupts */
    __asm__("sti");
    esp_printf(putc,"Interrupts enabled.\n\n");

    /* ---------- shell ---------- */
    shell_run();

    /* Should never reach here */
    while (1) {
        __asm__("hlt");
    }
}