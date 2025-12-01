#include <stdint.h>
#include "rprintf.h"
#include "page.h"
#include "paging.h"
#include "interrupt.h"
#include "shell.h"

#define VIDEO_ADDR 0xB8000

int putc(int ch) {
    static int video_index=0;
    volatile char *video = (volatile char*)VIDEO_ADDR;

    video[video_index] = (char) ch;
    video[video_index+1] = 0x07;
    video_index += 2;

    if (video_index >= 2*80*25) video_index=0;
    return ch;
}

extern char _end_kernel;

uint32_t align_down_page(uint32_t x) {
    return x & ~(PAGE_SIZE-1);
}

void main() {

    esp_printf(putc,"Hello, World!\n");

    /* ---------- interrupts ---------- */
    remap_pic();   
    load_gdt();    
    init_idt();    

    /* ---------- paging setup ---------- */

    identity_map_range(0x00100000u, (uint32_t)&_end_kernel);

    uint32_t esp_val; __asm__("mov %%esp,%0":"=r"(esp_val));
    uint32_t lo = align_down_page(esp_val) - 7*PAGE_SIZE;
    uint32_t hi = align_down_page(esp_val) + PAGE_SIZE;
    identity_map_range(lo,hi);

    identity_map_range(0x000B8000u, 0x000B8000u + PAGE_SIZE);

    paging_init_recursive(kernel_pd);
    loadPageDirectory(kernel_pd);
    enablePaging();

    esp_printf(putc,"Paging enabled.\n");

    /* ---------- page-frame allocator ---------- */
    init_pfa_list();
    esp_printf(putc,"Free frames: %u\n", pfa_free_count());

    /* ---------- PIT ---------- */
    pit_init(100);

    /* enable interrupts */
    __asm__("sti");

    /* ---------- shell ---------- */
    shell_run();

    while (1) {}
}
