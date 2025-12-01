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
#define PIC_1_CTRL PIC1
#define PIC_2_CTRL PIC2

/* KEEP ALL YOUR ORIGINAL STRUCTS/DEFS */

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

struct interrupt_frame {
    uint32_t ip;
    uint32_t cs;
    uint32_t flags;
    uint32_t sp;
    uint32_t ss;
};

struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

/* Provided by interrupt.c */
void PIC_sendEOI(unsigned char irq);
void IRQ_clear_mask(unsigned char IRQline);
void IRQ_set_mask(unsigned char IRQline);
void init_idt();
void load_gdt();
void remap_pic(void);

/* Timer API */
void pit_init(uint32_t hz);
uint32_t timer_ticks(void);

/* Keyboard API - Basic */
int  keyboard_getchar(void);
char keyboard_read_char(void);

/* Keyboard API - Enhanced */
int keyboard_available(void);
void keyboard_clear_buffer(void);
int keyboard_peek_char(void);

/* Assembly stubs (if you have these) */
extern void stub_isr(void);
extern void idt_flush(struct idt_ptr *ptr);

#endif