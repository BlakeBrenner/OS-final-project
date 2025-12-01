#include <stdint.h>
#include "console.h"
#include "rprintf.h"
#include "page.h"
#include "paging.h"
#include "interrupt.h"
#include "keyboard.h"
#include "timer.h"
#include "shell.h"

#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6

const unsigned int multiboot_header[] __attribute__((section(".multiboot"))) = {
    MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16 + MULTIBOOT2_HEADER_MAGIC), 0, 12
};

uint8_t inb(uint16_t _port) {
    uint8_t rv;
    __asm__ __volatile__("inb %1, %0" : "=a"(rv) : "dN"(_port));
    return rv;
}

extern void outb(uint16_t _port, uint8_t val);

extern uint32_t _end_kernel;

static inline uint32_t align_down_page(uint32_t x) { return x & ~0xFFFu; }

static void identity_map_range(uint32_t start, uint32_t end) {
    const uint32_t LIM = 0x00400000u;
    if (start >= LIM) return;
    if (end > LIM) end = LIM;

    start = align_down_page(start);
    end = align_down_page(end + PAGE_SIZE - 1);

    for (uint32_t a = start; a < end; a += PAGE_SIZE) {
        struct ppage tmp; tmp.next = NULL; tmp.prev = NULL; tmp.physical_addr = (void*)a;
        (void)map_pages((void*)a, &tmp, kernel_pd);
    }
}

static void init_paging(void) {
    identity_map_range(0x00100000u, (uint32_t)&_end_kernel);

    uint32_t esp_val; __asm__ __volatile__("mov %%esp,%0" : "=r"(esp_val));
    uint32_t stack_lo  = align_down_page(esp_val) - 7*PAGE_SIZE;
    uint32_t stack_hi  = align_down_page(esp_val) + 1*PAGE_SIZE;
    identity_map_range(stack_lo, stack_hi);
    identity_map_range(0x000B8000u, 0x000B8000u + PAGE_SIZE);

    paging_init_recursive(kernel_pd);
    loadPageDirectory(kernel_pd);
    enablePaging();

    esp_printf(console_putc, "Paging enabled. PD=%p kernel=%p..%p stack~%p VGA=0xB8000\n",
               kernel_pd, (void*)0x00100000u, &_end_kernel, (void*)esp_val);
}

void main() {
    console_init();
    esp_printf(console_putc, "Booting kernel shell...\n");

    init_pfa_list();
    init_paging();

    remap_pic();
    load_gdt();
    init_idt();

    keyboard_init();
    timer_init(100);
    IRQ_clear_mask(0); // PIT
    IRQ_clear_mask(1); // Keyboard

    asm("sti");

    console_writeln("Interactive shell ready. Type 'help' for commands.");
    shell_run();

    while (1) {
        asm("hlt");
    }
}
