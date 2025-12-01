#include "shell.h"
#include "keyboard.h"
#include "rprintf.h"
#include "page.h"
#include "paging.h"
#include "timer.h"

#define SHELL_MAX_LINE 128
#define MAX_ARGS 8

static char line_buf[SHELL_MAX_LINE];

struct command {
    const char *name;
    const char *help;
    void (*fn)(int argc, char **argv);
};

static void cmd_help(int argc, char **argv);
static void cmd_cls(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_meminfo(int argc, char **argv);
static void cmd_frames(int argc, char **argv);
static void cmd_v2p(int argc, char **argv);
static void cmd_read32(int argc, char **argv);
static void cmd_ptdump(int argc, char **argv);
static void cmd_uptime(int argc, char **argv);

static struct command commands[] = {
    {"help", "List available commands", cmd_help},
    {"cls", "Clear the screen", cmd_cls},
    {"echo", "Echo the given text", cmd_echo},
    {"meminfo", "Show total and free frames", cmd_meminfo},
    {"frames", "List free frame addresses", cmd_frames},
    {"v2p", "Translate virtual address to physical", cmd_v2p},
    {"read32", "Read a 32-bit value from memory", cmd_read32},
    {"ptdump", "Dump present PDEs and sample PTEs", cmd_ptdump},
    {"uptime", "Show tick count and seconds since boot", cmd_uptime},
};

static unsigned int command_count(void) {
    return sizeof(commands) / sizeof(commands[0]);
}

static int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        a++; b++;
    }
    return (*(const unsigned char *)a) - (*(const unsigned char *)b);
}

static int is_space(char c) {
    return c == ' ' || c == '\t';
}

static int parse_uint32(const char *s, uint32_t *out) {
    if (!s || !out) return 0;
    uint32_t base = 10;
    uint32_t val = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    }
    if (!*s) return 0;

    while (*s) {
        char c = *s++;
        uint32_t digit;
        if (c >= '0' && c <= '9') digit = (uint32_t)(c - '0');
        else if (base == 16 && c >= 'a' && c <= 'f') digit = 10u + (uint32_t)(c - 'a');
        else if (base == 16 && c >= 'A' && c <= 'F') digit = 10u + (uint32_t)(c - 'A');
        else return 0;
        val = val * base + digit;
    }
    *out = val;
    return 1;
}

static int tokenize(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max_args) {
        while (*p && is_space(*p)) p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && !is_space(*p)) p++;
        if (*p) {
            *p = '\0';
            p++;
        }
    }
    return argc;
}

static void print_prompt(void) {
    esp_printf(putc, "> ");
}

static void read_line(void) {
    unsigned int len = 0;
    while (1) {
        char c;
        while (!keyboard_poll_char(&c)) {
            // spin until a character arrives
        }

        if (c == '\n' || c == '\r') {
            putc('\n');
            line_buf[len] = '\0';
            return;
        } else if (c == '\b') {
            if (len > 0) {
                len--;
                putc('\b');
                putc(' ');
                putc('\b');
            }
        } else if (len < SHELL_MAX_LINE - 1) {
            line_buf[len++] = c;
            putc(c);
        }
    }
}

static void cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    for (unsigned int i = 0; i < command_count(); ++i) {
        esp_printf(putc, "%s - %s\n", commands[i].name, commands[i].help);
    }
}

static void cmd_cls(int argc, char **argv) {
    (void)argc; (void)argv;
    clear_screen();
}

static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        esp_printf(putc, "%s", argv[i]);
        if (i + 1 < argc) {
            putc(' ');
        }
    }
    putc('\n');
}

static void cmd_meminfo(int argc, char **argv) {
    (void)argc; (void)argv;
    unsigned int free_frames = pfa_free_count();
    unsigned int total_frames = pfa_total_count();
    unsigned int total_bytes = total_frames * PFA_PAGE_BYTES;
    unsigned int free_bytes = free_frames * PFA_PAGE_BYTES;
    esp_printf(putc, "Frames: %u/%u free (%u / %u KiB)\n",
               free_frames, total_frames,
               free_bytes >> 10, total_bytes >> 10);
}

static void cmd_frames(int argc, char **argv) {
    (void)argc; (void)argv;
    struct ppage *cur = free_list_head;
    unsigned int idx = 0;
    while (cur) {
        esp_printf(putc, "[%u] 0x%08x\n", idx, (uint32_t)(uintptr_t)cur->physical_addr);
        cur = cur->next;
        idx++;
    }
    esp_printf(putc, "Total free: %u\n", pfa_free_count());
}

static void cmd_v2p(int argc, char **argv) {
    if (argc < 2) {
        esp_printf(putc, "Usage: v2p <virtual_addr>\n");
        return;
    }
    uint32_t va;
    if (!parse_uint32(argv[1], &va)) {
        esp_printf(putc, "Invalid address\n");
        return;
    }
    void *phys = get_physaddr((void*)(uintptr_t)va);
    if (!phys) {
        esp_printf(putc, "Not mapped\n");
    } else {
        esp_printf(putc, "VA 0x%08x -> PA 0x%08x\n", va, (uint32_t)(uintptr_t)phys);
    }
}

static void cmd_read32(int argc, char **argv) {
    if (argc < 2) {
        esp_printf(putc, "Usage: read32 <addr>\n");
        return;
    }
    uint32_t addr;
    if (!parse_uint32(argv[1], &addr)) {
        esp_printf(putc, "Invalid address\n");
        return;
    }
    uint32_t val = *((volatile uint32_t *)(uintptr_t)addr);
    esp_printf(putc, "[0x%08x] = 0x%08x\n", addr, val);
}

static void cmd_ptdump(int argc, char **argv) {
    (void)argc; (void)argv;
    volatile uint32_t *pd = (uint32_t *)0xFFFFF000;
    for (uint32_t pdi = 0; pdi < 1024; ++pdi) {
        uint32_t entry = pd[pdi];
        if (!(entry & 0x1)) {
            continue;
        }
        uint32_t pt_base = entry & ~0xFFFu;
        esp_printf(putc, "PDE %3u -> PT 0x%08x\n", pdi, pt_base);
        volatile uint32_t *pt = (uint32_t *)0xFFC00000 + (pdi * 1024);
        for (uint32_t pti = 0; pti < 4; ++pti) {
            uint32_t pte = pt[pti];
            if (pte & 0x1) {
                esp_printf(putc, "    PTE[%u] = 0x%08x\n", pti, pte);
            }
        }
    }
}

static void cmd_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t ticks = (uint32_t)timer_ticks();
    uint32_t freq = timer_frequency();
    uint32_t seconds = (freq > 0) ? (uint32_t)(ticks / freq) : 0;
    esp_printf(putc, "Ticks: %u (freq %u Hz) uptime: %u s\n", (uint32_t)ticks, freq, seconds);
}

static void execute_line(char *line) {
    char *argv[MAX_ARGS];
    int argc = tokenize(line, argv, MAX_ARGS);
    if (argc == 0) {
        return;
    }

    for (unsigned int i = 0; i < command_count(); ++i) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].fn(argc, argv);
            return;
        }
    }
    esp_printf(putc, "Unknown command: %s\n", argv[0]);
}

void shell_run(void) {
    esp_printf(putc, "Kernel shell ready. Type 'help' for commands.\n");
    while (1) {
        print_prompt();
        read_line();
        execute_line(line_buf);
    }
}

int shell_poll_char(char *out_char) {
    return keyboard_poll_char(out_char);
}
