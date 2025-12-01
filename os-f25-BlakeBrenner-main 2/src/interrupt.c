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

/* ------------------- Enhanced Keyboard State ------------------- */

static volatile uint8_t shift_pressed = 0;
static volatile uint8_t caps_lock = 0;
static volatile uint8_t ctrl_pressed = 0;

/* Basic keyboard map (no modifiers) */
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

/* Shifted keyboard map */
static unsigned char keyboard_map_shifted[128] =
{
   0,  27, '!', '@', '#', '$', '%', '^',
   '&','*','(',')','_','+', '\b',
   '\t',
   'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
   0,
   'A','S','D','F','G','H','J','K','L',':','"','~',0,
   '|','Z','X','C','V','B','N','M','<','>','?',0,
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

/* ------------------- GDT + TSS (Keep your original code) ------------------- */

extern uint32_t stack_top;
extern int _end_stack;

/* NOTE: If you have GDT/TSS functions in your original interrupt.c,
   keep them here. This includes:
   - GDT array
   - write_tss()
   - load_gdt()
   - Any other GDT/TSS related code
   
   I'm leaving this section blank since I don't have your original code.
   Just copy your original GDT/TSS code here.
*/

/* ------------------- IDT setup ------------------- */

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

uint32_t timer_ticks(void) { 
    return g_ticks; 
}

int keyboard_getchar(void) {
    if (kb_head == kb_tail) return -1;
    char c = kb_buf[kb_tail];
    kb_tail = (kb_tail+1) % KB_BUF_SIZE;
    return c;
}

char keyboard_read_char(void) {
    int ch;
    while ((ch = keyboard_getchar()) == -1) { 
        __asm__ __volatile__("hlt"); // Save power while waiting
    }
    return (char)ch;
}

/* ------------------- Enhanced Keyboard Functions ------------------- */

int keyboard_available(void) {
    if (kb_head >= kb_tail) {
        return kb_head - kb_tail;
    } else {
        return KB_BUF_SIZE - kb_tail + kb_head;
    }
}

void keyboard_clear_buffer(void) {
    kb_head = kb_tail = 0;
}

int keyboard_peek_char(void) {
    if (kb_head == kb_tail) return -1;
    return kb_buf[kb_tail];
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

    /* Handle key releases (scancode >= 0x80) */
    if (sc >= 0x80) {
        sc -= 0x80; /* Convert to press scancode */
        
        /* Track shift release */
        if (sc == 0x2A || sc == 0x36) { /* Left/Right Shift */
            shift_pressed = 0;
        }
        /* Track ctrl release */
        if (sc == 0x1D) { /* Ctrl */
            ctrl_pressed = 0;
        }
        
        PIC_sendEOI(1);
        return;
    }

    /* Handle key presses */
    
    /* Shift keys */
    if (sc == 0x2A || sc == 0x36) { /* Left/Right Shift */
        shift_pressed = 1;
        PIC_sendEOI(1);
        return;
    }
    
    /* Caps Lock toggle */
    if (sc == 0x3A) {
        caps_lock = !caps_lock;
        PIC_sendEOI(1);
        return;
    }
    
    /* Ctrl key */
    if (sc == 0x1D) {
        ctrl_pressed = 1;
        PIC_sendEOI(1);
        return;
    }

    /* Get character from appropriate map */
    char c = 0;
    
    if (sc < 128) {
        if (shift_pressed) {
            c = keyboard_map_shifted[sc];
        } else {
            c = keyboard_map[sc];
        }
        
        /* Apply caps lock to letters */
        if (caps_lock && c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        } else if (caps_lock && c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
        
        /* Handle Ctrl combinations */
        if (ctrl_pressed && c >= 'a' && c <= 'z') {
            c = c - 'a' + 1; /* Ctrl+A = 0x01, etc. */
        } else if (ctrl_pressed && c >= 'A' && c <= 'Z') {
            c = c - 'A' + 1;
        }
        
        /* Add to buffer if valid character */
        if (c) {
            unsigned nxt = (kb_head + 1) % KB_BUF_SIZE;
            if (nxt != kb_tail) {
                kb_buf[kb_head] = c;
                kb_head = nxt;
            }
        }
    }
    
    PIC_sendEOI(1);
}

/* ------------------- PIC Functions ------------------- */

void PIC_sendEOI(unsigned char irq) {
    if (irq >= 8)
        outb(PIC2, PIC_EOI);
    outb(PIC1, PIC_EOI);
}

void IRQ_set_mask(unsigned char IRQline) {
    uint16_t port;
    uint8_t value;

    if(IRQline < 8) {
        port = PIC_1_DATA;
    } else {
        port = PIC_2_DATA;
        IRQline -= 8;
    }
    value = inb(port) | (1 << IRQline);
    outb(port, value);
}

void IRQ_clear_mask(unsigned char IRQline) {
    uint16_t port;
    uint8_t value;

    if(IRQline < 8) {
        port = PIC_1_DATA;
    } else {
        port = PIC_2_DATA;
        IRQline -= 8;
    }
    value = inb(port) & ~(1 << IRQline);
    outb(port, value);
}

/* ------------------- init_idt ------------------- */

void init_idt() {
    memset((char*)&idt_entries, 0, sizeof(idt_entries));

    /* If you have a stub_isr, uncomment this:
    for (int i=0; i<256; i++)
        idt_set_gate(i, (uint32_t)stub_isr, 0x08, 0x8E);
    */

    /* Hook PIT (IRQ0) + keyboard (IRQ1) */
    idt_set_gate(32, (uint32_t)pit_handler, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)keyboard_handler, 0x08, 0x8E);

    /* Setup IDT pointer */
    idt_ptr.limit = sizeof(idt_entries) - 1;
    idt_ptr.base = (uint32_t)&idt_entries;

    /* If you have idt_flush assembly function, use it: */
    idt_flush(&idt_ptr);
    
    /* Otherwise use inline assembly:
    __asm__ __volatile__("lidt %0" : : "m"(idt_ptr));
    */
}

/* ------------------- PIC remap ------------------- */

void remap_pic(void) {
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