#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include <stdint.h>

#define PIC_EOI	0x20
#define PIC1	0x20
#define PIC2	0xA0
#define PIC_1_COMMAND PIC1
#define PIC_2_COMMAND PIC2
#define PIC_1_DATA 0x21
#define PIC_2_DATA 0xA1

/* ... KEEP ALL YOUR ORIGINAL STRUCTS/DEFS ... */

struct idt_entry {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_hi;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* (KEEP ALL THE OTHER STRUCTS IN YOUR ORIGINAL FILE) */

/* Provided by interrupt.c */
void PIC_sendEOI(unsigned char irq);
void IRQ_clear_mask(unsigned char IRQline);
void IRQ_set_mask(unsigned char IRQline);
void init_idt();
void load_gdt();
void remap_pic(void);

/* Added for shell + drivers */
void pit_init(uint32_t hz);
uint32_t timer_ticks(void);

int  keyboard_getchar(void);
char keyboard_read_char(void);

#endif
