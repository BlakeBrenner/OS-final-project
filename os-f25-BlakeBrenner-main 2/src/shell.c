#include <stdint.h>
#include "rprintf.h"
#include "page.h"
#include "paging.h"
#include "interrupt.h"
#include "shell.h"

extern int putc(int ch);
extern void vga_clear(void);

/* ---------- Helpers ---------- */

static void print_prompt(void) {
    putc('>');
    putc(' ');
}

static int readline(char *buf, int maxlen) {
    int len = 0;
    for (;;) {
        char c = keyboard_read_char();

        if (c == '\n' || c == '\r') {
            putc('\n');
            buf[len] = 0;
            return len;
        }
        if ((c == '\b' || c == 127) && len > 0) {
            len--;
            putc('\b');
            putc(' ');
            putc('\b');
            continue;
        }
        if (c >= 32 && c < 127) {
            if (len + 1 < maxlen) {
                buf[len++] = c;
                putc(c);
            }
        }
    }
}

static int tokenize(char *line, char *argv[], int max_args) {
    int argc = 0;
    char *p = line;

    while (*p && argc < max_args) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) { *p = 0; p++; }
    }
    return argc;
}

static int parse_hex32(const char *s, uint32_t *out) {
    if (!s) return -1;

    if (s[0]=='0' && (s[1]=='x' || s[1]=='X')) s += 2;

    uint32_t v = 0;
    if (!*s) return -1;

    while (*s) {
        char c = *s++;
        uint32_t d;

        if (c>='0' && c<='9') d = c - '0';
        else if (c>='a' && c<='f') d = 10 + (c - 'a');
        else if (c>='A' && c<='F') d = 10 + (c - 'A');
        else return -1;

        v = (v<<4) | d;
    }
    *out = v;
    return 0;
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* ---------- Commands ---------- */

static void cmd_help(void) {
    esp_printf(putc,
        "Available Commands:\n"
        "  help              - show this help\n"
        "  cls               - clear screen\n"
        "  echo <text>       - print text\n"
        "  meminfo           - show memory statistics\n"
        "  frames            - list free page frames\n"
        "  alloc <n>         - allocate n pages (test)\n"
        "  v2p <addr>        - translate virtual to physical\n"
        "  ptdump            - dump page directory/tables\n"
        "  read32 <addr>     - read 32-bit value from address\n"
        "  write32 <a> <v>   - write value to address\n"
        "  hexdump <a> [len] - hex dump memory region\n"
        "  map <pa> <va>     - map physical to virtual page\n"
        "  uptime            - show system uptime\n"
        "  sleep <sec>       - sleep for N seconds\n"
        "  info              - kernel information\n"
        "  kbtest            - test keyboard buffer\n"
    );
}

static void cmd_cls(void) {
    vga_clear();
}

static void cmd_echo(int argc,char *argv[]) {
    for (int i=1;i<argc;i++) {
        esp_printf(putc, "%s", argv[i]);
        if (i+1<argc) esp_printf(putc, " ");
    }
    esp_printf(putc, "\n");
}

static void cmd_meminfo(void) {
    unsigned free = pfa_free_count();
    unsigned total = 128;
    esp_printf(putc, "total frames: %d\n", (int)total);
    esp_printf(putc, "free frames : %d\n", (int)free);
}

static void cmd_frames(void) {
    extern struct ppage *free_list_head;
    struct ppage *p = free_list_head;
    int i = 0;
    for (; p && i < 64; p = p->next, i++)
        esp_printf(putc, "#%02d: phys=0x%08x\n", i,
                   (uint32_t)p->physical_addr);
    if (p) esp_printf(putc, "(truncated)\n");
}

static void cmd_v2p(int argc,char *argv[]) {
    if (argc!=2) { esp_printf(putc,"usage: v2p <va>\n"); return; }
    uint32_t va;
    if (parse_hex32(argv[1],&va)) {
        esp_printf(putc,"invalid hex\n");
        return;
    }
    void *pa = get_physaddr((void*)va);
    if (!pa) esp_printf(putc,"not mapped\n");
    else     esp_printf(putc,"0x%08x -> 0x%08x\n",va,(uint32_t)pa);
}

static void cmd_ptdump(void) {
    unsigned long *pd = (unsigned long*)0xFFFFF000;
    esp_printf(putc,"PDE dump:\n");

    for (unsigned i = 0; i < 1024; i++) {
        unsigned long pde = pd[i];
        if (!(pde & 1)) continue;

        esp_printf(putc,"PDE %d: 0x%08x\n", (int)i,(uint32_t)pde);

        unsigned long *pt = (unsigned long*)(0xFFC00000 + i*0x1000);
        int shown = 0;
        for (unsigned j=0;j<1024 && shown<4;j++) {
            unsigned long pte = pt[j];
            if (pte & 1) {
                esp_printf(putc,"  PTE %d: 0x%08x\n", (int)j,(uint32_t)pte);
                shown++;
            }
        }
    }
}

static void cmd_read32(int argc,char *argv[]) {
    if (argc!=2) { esp_printf(putc,"usage: read32 <va>\n"); return; }
    uint32_t va;
    if (parse_hex32(argv[1],&va)) { esp_printf(putc,"invalid hex\n"); return; }
    void *pa = get_physaddr((void*)va);
    if (!pa) {
        esp_printf(putc,"not mapped\n");
        return;
    }
    uint32_t val = *(volatile uint32_t*)va;
    esp_printf(putc,"VA=0x%08x PA=0x%08x val=0x%08x\n",
               va,(uint32_t)pa,val);
}

static void cmd_uptime(void) {
    uint32_t t = timer_ticks();
    uint32_t seconds = t / 100;
    esp_printf(putc,"ticks=%d seconds=%d\n", (int)t, (int)seconds);
}

/* ==================== NEW COMMANDS ==================== */

static void cmd_write32(int argc, char *argv[]) {
    if (argc != 3) {
        esp_printf(putc, "usage: write32 <addr> <value>\n");
        return;
    }
    
    uint32_t va, val;
    if (parse_hex32(argv[1], &va)) {
        esp_printf(putc, "invalid address\n");
        return;
    }
    if (parse_hex32(argv[2], &val)) {
        esp_printf(putc, "invalid value\n");
        return;
    }
    
    void *pa = get_physaddr((void*)va);
    if (!pa) {
        esp_printf(putc, "not mapped\n");
        return;
    }
    
    *(volatile uint32_t*)va = val;
    esp_printf(putc, "wrote 0x%08x to VA=0x%08x (PA=0x%08x)\n",
               val, va, (uint32_t)pa);
}

static void cmd_hexdump(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        esp_printf(putc, "usage: hexdump <addr> [length]\n");
        return;
    }
    
    uint32_t va;
    if (parse_hex32(argv[1], &va)) {
        esp_printf(putc, "invalid address\n");
        return;
    }
    
    uint32_t len = 64;
    if (argc == 3) {
        if (parse_hex32(argv[2], &len)) {
            esp_printf(putc, "invalid length\n");
            return;
        }
    }
    
    if (len > 256) len = 256;
    
    uint32_t start = va & ~0xF;
    uint32_t end = (va + len + 15) & ~0xF;
    
    for (uint32_t addr = start; addr < end; addr += 16) {
        void *pa = get_physaddr((void*)addr);
        if (!pa) {
            esp_printf(putc, "0x%08x: [not mapped]\n", addr);
            continue;
        }
        
        esp_printf(putc, "0x%08x: ", addr);
        
        for (int i = 0; i < 16; i++) {
            if (addr + i < va || addr + i >= va + len) {
                esp_printf(putc, "   ");
            } else {
                uint8_t byte = *((volatile uint8_t*)(addr + i));
                esp_printf(putc, "%02x ", byte);
            }
        }
        
        esp_printf(putc, " |");
        
        for (int i = 0; i < 16; i++) {
            if (addr + i < va || addr + i >= va + len) {
                esp_printf(putc, " ");
            } else {
                uint8_t byte = *((volatile uint8_t*)(addr + i));
                if (byte >= 32 && byte < 127) {
                    esp_printf(putc, "%c", byte);
                } else {
                    esp_printf(putc, ".");
                }
            }
        }
        
        esp_printf(putc, "|\n");
    }
}

static void cmd_alloc(int argc, char *argv[]) {
    if (argc != 2) {
        esp_printf(putc, "usage: alloc <npages>\n");
        return;
    }
    
    uint32_t npages;
    if (parse_hex32(argv[1], &npages) || npages == 0) {
        esp_printf(putc, "invalid page count\n");
        return;
    }
    
    struct ppage *pages = allocate_physical_pages(npages);
    if (!pages) {
        esp_printf(putc, "allocation failed (out of memory)\n");
        return;
    }
    
    esp_printf(putc, "allocated %d page(s):\n", (int)npages);
    struct ppage *p = pages;
    int i = 0;
    while (p && i < 10) {
        esp_printf(putc, "  [%d] phys=0x%08x\n", i, (uint32_t)p->physical_addr);
        p = p->next;
        i++;
    }
    if (p) esp_printf(putc, "  ... (truncated)\n");
    
    free_physical_pages(pages);
    esp_printf(putc, "pages freed (test successful)\n");
}

static void cmd_info(void) {
    extern char _end_kernel;
    
    esp_printf(putc, "Kernel Information:\n");
    esp_printf(putc, "  Kernel end: 0x%08x\n", (uint32_t)&_end_kernel);
    esp_printf(putc, "  Page size:  %d bytes\n", (int)PAGE_SIZE);
    esp_printf(putc, "  PD address: 0x%08x\n", (uint32_t)kernel_pd);
    
    unsigned free = pfa_free_count();
    unsigned total = 128;
    unsigned used = total - free;
    esp_printf(putc, "  Memory:     %d / %d frames used\n", (int)used, (int)total);
    
    uint32_t t = timer_ticks();
    esp_printf(putc, "  Uptime:     %d seconds\n", (int)(t / 100));
}

static void cmd_sleep(int argc, char *argv[]) {
    if (argc != 2) {
        esp_printf(putc, "usage: sleep <seconds>\n");
        return;
    }
    
    uint32_t seconds;
    if (parse_hex32(argv[1], &seconds) || seconds == 0) {
        esp_printf(putc, "invalid duration\n");
        return;
    }
    
    if (seconds > 60) {
        esp_printf(putc, "duration too long (max 60 seconds)\n");
        return;
    }
    
    uint32_t start = timer_ticks();
    uint32_t target = start + (seconds * 100);
    
    esp_printf(putc, "sleeping for %d seconds...\n", (int)seconds);
    
    while (timer_ticks() < target) {
        __asm__ __volatile__("hlt");
    }
    
    esp_printf(putc, "awake!\n");
}

static void cmd_kbtest(void) {
    esp_printf(putc, "Keyboard buffer test - type 10 chars quickly:\n");
    
    for (int i = 0; i < 10; i++) {
        char c = keyboard_read_char();
        esp_printf(putc, "[%d] = '%c' (0x%02x)\n", i, c, (unsigned char)c);
    }
    
    esp_printf(putc, "test complete\n");
}

static void cmd_map(int argc, char *argv[]) {
    if (argc != 3) {
        esp_printf(putc, "usage: map <phys_addr> <virt_addr>\n");
        return;
    }
    
    uint32_t pa, va;
    if (parse_hex32(argv[1], &pa)) {
        esp_printf(putc, "invalid physical address\n");
        return;
    }
    if (parse_hex32(argv[2], &va)) {
        esp_printf(putc, "invalid virtual address\n");
        return;
    }
    
    pa &= ~0xFFF;
    va &= ~0xFFF;
    
    int result = map_page((void*)pa, (void*)va, 0x003);
    
    if (result == 0) {
        esp_printf(putc, "mapped PA=0x%08x -> VA=0x%08x\n", pa, va);
    } else {
        esp_printf(putc, "mapping failed (error %d)\n", result);
    }
}

/* ---------- Command Dispatcher ---------- */

static void handle_cmd(int argc,char *argv[]) {
    if (argc==0) return;

    if (!strcmp(argv[0],"help")) cmd_help();
    else if (!strcmp(argv[0],"cls")) cmd_cls();
    else if (!strcmp(argv[0],"echo")) cmd_echo(argc,argv);
    else if (!strcmp(argv[0],"meminfo")) cmd_meminfo();
    else if (!strcmp(argv[0],"frames")) cmd_frames();
    else if (!strcmp(argv[0],"v2p")) cmd_v2p(argc,argv);
    else if (!strcmp(argv[0],"ptdump")) cmd_ptdump();
    else if (!strcmp(argv[0],"read32")) cmd_read32(argc,argv);
    else if (!strcmp(argv[0],"write32")) cmd_write32(argc,argv);
    else if (!strcmp(argv[0],"hexdump")) cmd_hexdump(argc,argv);
    else if (!strcmp(argv[0],"alloc")) cmd_alloc(argc,argv);
    else if (!strcmp(argv[0],"info")) cmd_info();
    else if (!strcmp(argv[0],"sleep")) cmd_sleep(argc,argv);
    else if (!strcmp(argv[0],"kbtest")) cmd_kbtest();
    else if (!strcmp(argv[0],"map")) cmd_map(argc,argv);
    else if (!strcmp(argv[0],"uptime")) cmd_uptime();
    else esp_printf(putc,"unknown command\n");
}

/* ---------- Main Loop ---------- */

void shell_run(void) {
    char line[128];
    char *argv[8];

    esp_printf(putc,"\nKernel shell ready. Type 'help'.\n");

    for (;;) {
        print_prompt();
        readline(line,sizeof(line));
        int argc = tokenize(line,argv,8);
        handle_cmd(argc,argv);
    }
}