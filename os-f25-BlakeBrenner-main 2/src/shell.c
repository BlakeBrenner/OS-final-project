#include <stdint.h>
#include "rprintf.h"
#include "page.h"
#include "paging.h"
#include "interrupt.h"
#include "shell.h"

extern int putc(int ch);

/* ---------- Helpers ---------- */

static void print_prompt(void) {
    esp_printf(putc, "> ");
}

static int readline(char *buf, int maxlen) {
    int len = 0;
    for (;;) {
        char c = keyboard_read_char();

        if (c == '\n' || c == '\r') {
            esp_printf(putc, "\n");
            buf[len] = 0;
            return len;
        }
        if ((c == '\b' || c == 127) && len > 0) {
            len--;
            esp_printf(putc, "\b \b");
            continue;
        }
        if (c >= 32 && c < 127) {
            if (len + 1 < maxlen) {
                buf[len++] = c;
                esp_printf(putc, "%c", c);
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
        "Commands:\n"
        "  help\n"
        "  cls\n"
        "  echo <text>\n"
        "  meminfo\n"
        "  frames\n"
        "  v2p <virt>\n"
        "  ptdump\n"
        "  read32 <addr>\n"
        "  uptime\n"
    );
}

static void cmd_cls(void) {
    for (int i=0;i<30;i++) esp_printf(putc, "\n");
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
    esp_printf(putc, "total frames: %u\n", total);
    esp_printf(putc, "free frames : %u\n", free);
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

        esp_printf(putc,"PDE %u: 0x%08x\n", i,(uint32_t)pde);

        unsigned long *pt = (unsigned long*)(0xFFC00000 + i*0x1000);
        int shown = 0;
        for (unsigned j=0;j<1024 && shown<4;j++) {
            unsigned long pte = pt[j];
            if (pte & 1) {
                esp_printf(putc,"  PTE %u: 0x%08x\n", j,(uint32_t)pte);
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
    esp_printf(putc,"ticks=%u seconds=%u\n", t, t/100);
}

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
        int len = readline(line,sizeof(line));
        int argc = tokenize(line,argv,8);
        handle_cmd(argc,argv);
    }
}
