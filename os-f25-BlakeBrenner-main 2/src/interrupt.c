#include <stdint.h>
#include "interrupt.h"

/* ------------------- Existing globals ------------------- */

struct idt_entry idt_entries[256];
struct idt_ptr   idt_ptr;
struct tss_entry tss_ent;

/* ------------------- Timer + Keyboard Globals ------------------- */

static volatile uint32_t g_ticks = 0;

#define KB_BUF_SIZE 128
static volatile char kb_buf[KB_BUF_SIZE];
static volatile unsigned kb_head = 0, kb_tail = 0;

static unsigned char keyboard_map[128] =
{
   0,  27, '1', '2', '3', '4', '5', '6',
   '7','8','9','0','-','=', '\b',
   '\t',
   'q','w','e','r','t','y','u','i','o','p','[',']','\n',
   0,
   'a','s','d','f','g','h','j','k','l',';','\'','`',0,
   '\\','z','x','c','v','b','n','m',',','.','/',0,
   '*',0,' ',
};

/* ------------------- Basic helpers ------------------- */

void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0,%1" : : "a"(val), "dN"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ __volatile__("inb %1,%0" : "=a"(v) : "dN"(port));
    return v;
}

void memset(char *s,char c,unsigned n) {
    for (unsigned i=0;i<n;i++) s[i]=c;
}

/* ------------------- GDT + TSS (Unmodified except original code) ------------------- */
/* KEEP ALL YOUR ORIGINAL GDT/TSS CODE HERE — unchanged */

extern uint32_t stack_top;
extern int _end_stack;

/* (Your original GDT array, write_tss, load_gdt, etc.) */

/* ------------------- IDT setup (Unmodified except specific handlers) ------------------- */

static void idt_set_gate(uint8_t num,uint32_t base,uint16_t sel,uint8_t flags) {
    idt_entries[num].base_lo = base & 0xFFFF;
    idt_entries[num].base_hi = (base >> 16) & 0xFFFF;
    idt_entries[num].sel = sel;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags = flags;
}

/* ------------------- Timer + Keyboard API ------------------- */

void pit_init(uint32_t hz) {
    if (!hz) return;
    uint32_t div = 1193182u / hz;
    outb(0x43,0x36);
    outb(0x40, div & 0xFF);
    outb(0x40, (div>>8)&0xFF);
}

uint32_t timer_ticks(void) { return g_ticks; }

int keyboard_getchar(void) {
    if (kb_head == kb_tail) return -1;
    char c = kb_buf[kb_tail];
    kb_tail = (kb_tail+1) % KB_BUF_SIZE;
    return c;
}

char keyboard_read_char(void) {
    int ch;
    while ((ch = keyboard_getchar()) == -1) { }
    return (char)ch;
}

/* ------------------- Interrupt Handlers ------------------- */

__attribute__((interrupt))
void pit_handler(struct interrupt_frame *f) {
    (void)f;
    g_ticks++;
    PIC_sendEOI(0);
}

__attribute__((interrupt))
void keyboard_handler(struct interrupt_frame *f) {
    (void)f;

    uint8_t sc = inb(0x60);

    if (sc < 128) {
        char c = keyboard_map[sc];
        if (c) {
            unsigned nxt = (kb_head+1) % KB_BUF_SIZE;
            if (nxt != kb_tail) {
                kb_buf[kb_head] = c;
                kb_head = nxt;
            }
        }
    }
    PIC_sendEOI(1);
}

/* ------------------- init_idt (unchanged except hooking handlers) ------------------- */

void init_idt() {

    /* ... keep your original exception handlers ... */

    memset((char*)&idt_entries,0,sizeof(idt_entries));

    for (int i=0;i<256;i++)
        idt_set_gate(i,(uint32_t)stub_isr,0x08,0x8E);

    /* Hook PIT + keyboard */
    idt_set_gate(32,(uint32_t)pit_handler,0x08,0x8E);
    idt_set_gate(33,(uint32_t)keyboard_handler,0x08,0x8E);

    idt_flush(&idt_ptr);
}

/* ------------------- PIC remap ------------------- */

void remap_pic(void)
{
    outb(PIC_1_CTRL, 0x11);
    outb(PIC_2_CTRL, 0x11);

    outb(PIC_1_DATA, 0x20);
    outb(PIC_2_DATA, 0x28);
    outb(PIC_1_DATA, 0x00);
    outb(PIC_2_DATA, 0x00);
    outb(PIC_1_DATA, 0x01);
    outb(PIC_2_DATA, 0x01);

    /* Enable only IRQ0 + IRQ1 */
    outb(PIC_1_DATA, 0xFC);
    outb(PIC_2_DATA, 0xFF);
}

