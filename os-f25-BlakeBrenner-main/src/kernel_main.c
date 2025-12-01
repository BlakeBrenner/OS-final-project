#include <stdint.h>
#include "rprintf.h"
#include "page.h"
#include "paging.h"
#include "shell.h"
#include "interrupt.h"
#include "keyboard.h"
#include "timer.h"

#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6

// Multiboot2 header for GRUB
const unsigned int multiboot_header[] __attribute__((section(".multiboot"))) = {
    MULTIBOOT2_HEADER_MAGIC,           // magic
    0,                                // architecture (0 = i386)
    24,                               // total header length in bytes (must match array size)
    -(MULTIBOOT2_HEADER_MAGIC + 24),  // checksum: magic + arch + len + checksum == 0
    0, 8                              // end tag: type = 0, size = 8
};


// VGA definitions
#define VGA_ADDRESS 0xb8000
#define VGA_WIDTH   80
#define VGA_HEIGHT  25

struct termbuf {
    char ascii;
    char color;
};

static int cursor_row = 0;
static int cursor_column = 0;

// Pointer to VGA memory
static struct termbuf* const vram = (struct termbuf*)VGA_ADDRESS;

// Scroll the screen up by one line
void scroll() {
    // Move each line up
    for (int row = 1; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            vram[(row - 1) * VGA_WIDTH + col] = vram[row * VGA_WIDTH + col];
        }
    }
    
    // Clear the last line
    for (int col = 0; col < VGA_WIDTH; col++) {
        vram[(VGA_HEIGHT - 1) * VGA_WIDTH + col].ascii = ' ';
        vram[(VGA_HEIGHT - 1) * VGA_WIDTH + col].color = 7; // White on black
    }
}

void clear_screen() {
    for (int row = 0; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            vram[row * VGA_WIDTH + col].ascii = ' ';
            vram[row * VGA_WIDTH + col].color = 7;
        }
    }
    cursor_row = 0;
    cursor_column = 0;
}

// Print a single character
int putc(int ch) {
    if (ch == '\n') {
        cursor_row++;
        cursor_column = 0;
    } else {
        vram[cursor_row * VGA_WIDTH + cursor_column].ascii = (char)ch;
        vram[cursor_row * VGA_WIDTH + cursor_column].color = 7;
        cursor_column++;
    }

    // Wrap to next line if end of row
    if (cursor_column >= VGA_WIDTH) {
        cursor_column = 0;
        cursor_row++;
    }

    // Scroll if we're past the bottom
    if (cursor_row >= VGA_HEIGHT) {
        scroll();
        cursor_row = VGA_HEIGHT - 1;
    }

    return ch;
}



void test_page_allocator(void) {
    esp_printf(putc, "\n=== PAGE FRAME ALLOCATOR TEST ===\n");

    // Initialize allocator
    init_pfa_list();
    esp_printf(putc, "Initial free pages: %u\n", pfa_free_count());

    // Allocate 4 pages
    struct ppage *block = allocate_physical_pages(4);
    if (!block) {
        esp_printf(putc, "Allocation failed!\n");
        return;
    }

    esp_printf(putc, "Free pages after alloc(4): %u\n", pfa_free_count());

    // Display allocated pages and their addresses
    struct ppage *cur = block;
    int i = 0;
    while (cur) {
        esp_printf(putc, "Page %d addr: 0x%08x\n", i++, (uint32_t)cur->physical_addr);
        cur = cur->next;
    }

    // Free the pages back
    free_physical_pages(block);
    esp_printf(putc, "Free pages after free: %u\n", pfa_free_count());

    // Print final summary
    esp_printf(putc, "Allocator test complete.\n");
    esp_printf(putc, "Total managed memory: %u MiB\n",
                (unsigned int)((128 * (PFA_PAGE_BYTES >> 20)))); // 128 * 2 MiB = 256
}

extern uint32_t _end_kernel; 

/* ====== Tiny paging helpers (kept local to this file to stay contained) ====== */
static inline uint32_t align_down_page(uint32_t x) { return x & ~0xFFFu; }

/* identity-map [start, end) using the assignment's temp-ppage trick
   NOTE: works with the simplified paging (single PT for 0..4MiB). */
static void identity_map_range(uint32_t start, uint32_t end) {
    const uint32_t LIM = 0x00400000u; // first 4MiB only
    if (start >= LIM) return;
    if (end   >  LIM) end = LIM;

    start = align_down_page(start);
    end   = align_down_page(end + PAGE_SIZE - 1);

    for (uint32_t a = start; a < end; a += PAGE_SIZE) {
        struct ppage tmp; tmp.next = NULL; tmp.physical_addr = (void *)(uintptr_t)a; // VA==PA
        (void)map_pages((void*)a, &tmp, kernel_pd);
    }
}

// Kernel entry point
void main() {
    clear_screen();
    esp_printf(putc, "Kernel shell booting...\n");


    init_pfa_list();

    /* ---- page bring-up ---- */
    // 1) Identity-map kernel [0x0010_0000, &_end_kernel)
    identity_map_range(0x00100000u, (uint32_t)&_end_kernel);

    // 2) Identity-map a small window around the current stack (8 pages)
    uint32_t esp_val; __asm__ __volatile__("mov %%esp,%0" : "=r"(esp_val));
    uint32_t stack_lo  = align_down_page(esp_val) - 7*PAGE_SIZE;
    uint32_t stack_hi  = align_down_page(esp_val) + 1*PAGE_SIZE;
    identity_map_range(stack_lo, stack_hi);

    // 3) Identity-map VGA text buffer @ 0xB8000 (one page is enough)
    identity_map_range(0x000B8000u, 0x000B8000u + PAGE_SIZE);

    paging_init_recursive(kernel_pd);

    // 4) Load CR3 and enable paging (CR0.PE | CR0.PG)
    loadPageDirectory(kernel_pd);
    enablePaging();
    esp_printf(putc, "Paging enabled. PD=%p  kernel=%p..%p  stack~%p  VGA=0xB8000\n",
               kernel_pd, (void*)0x00100000u, &_end_kernel, (void*)esp_val);
    /* ---- end paging bring-up ---- */
    
    remap_pic();
    load_gdt();
    init_idt();

    keyboard_init();
    timer_init(100);

    asm("sti");

    shell_run();

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
