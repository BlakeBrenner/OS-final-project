#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "rprintf.h"
#include "page.h"
#include "paging.h"
#include "timer.h"
#include <stdint.h>

#define MAX_INPUT 128
#define MAX_ARGS 8

static void shell_print_prompt(void) {
    console_write("> ");
}

static int is_space(char c) {
    return c == ' ' || c == '\t';
}

static int strcmp_simple(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return a[i] - b[i];
        i++;
    }
    return a[i] - b[i];
}

static uint32_t parse_hex(const char *s) {
    uint32_t value = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s) {
        char c = *s;
        uint32_t digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F') digit = 10 + (c - 'A');
        else break;
        value = (value << 4) | digit;
        s++;
    }
    return value;
}

static void cmd_help(int argc, char **argv);
static void cmd_cls(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_meminfo(int argc, char **argv);
static void cmd_frames(int argc, char **argv);
static void cmd_v2p(int argc, char **argv);
static void cmd_ptdump(int argc, char **argv);
static void cmd_read32(int argc, char **argv);
static void cmd_uptime(int argc, char **argv);

struct command {
    const char *name;
    const char *desc;
    void (*fn)(int argc, char **argv);
};

static struct command commands[] = {
    {"help", "List available commands", cmd_help},
    {"cls", "Clear the screen", cmd_cls},
    {"echo", "Echo text", cmd_echo},
    {"meminfo", "Show total/free page frames", cmd_meminfo},
    {"frames", "List free frame addresses", cmd_frames},
    {"v2p", "Translate virtual address to physical", cmd_v2p},
    {"ptdump", "Dump present page directory entries", cmd_ptdump},
    {"read32", "Read 32-bit value at address", cmd_read32},
    {"uptime", "Show tick count and milliseconds since boot", cmd_uptime},
};

static int command_count(void) { return sizeof(commands) / sizeof(commands[0]); }

static void cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; i < command_count(); i++) {
        esp_printf(console_putc, "%-8s - %s\n", commands[i].name, commands[i].desc);
    }
}

static void cmd_cls(int argc, char **argv) {
    (void)argc; (void)argv;
    console_clear();
}

static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        esp_printf(console_putc, "%s%s", argv[i], (i + 1 < argc) ? " " : "");
    }
    console_putc('\n');
}

static void cmd_meminfo(int argc, char **argv) {
    (void)argc; (void)argv;
    unsigned int total = pfa_total_count();
    unsigned int free = pfa_free_count();
    esp_printf(console_putc, "Total frames: %u (size %u bytes each)\n", total, PFA_PAGE_BYTES);
    esp_printf(console_putc, "Free frames : %u\n", free);
}

static void cmd_frames(int argc, char **argv) {
    (void)argc; (void)argv;
    struct ppage *cur = free_list_head;
    int idx = 0;
    while (cur) {
        esp_printf(console_putc, "[%d] 0x%08x\n", idx++, (uint32_t)(uintptr_t)cur->physical_addr);
        cur = cur->next;
    }
    if (idx == 0) {
        console_writeln("(no free frames)");
    }
}

static void cmd_v2p(int argc, char **argv) {
    if (argc < 2) {
        console_writeln("Usage: v2p <virtual_addr>");
        return;
    }
    uint32_t vaddr = parse_hex(argv[1]);
    void *paddr = get_physaddr((void *)(uintptr_t)vaddr);
    if (!paddr) {
        console_writeln("Not mapped or not present");
        return;
    }
    esp_printf(console_putc, "VA 0x%08x -> PA 0x%08x\n", vaddr, (uint32_t)(uintptr_t)paddr);
}

static void cmd_ptdump(int argc, char **argv) {
    (void)argc; (void)argv;
    volatile uint32_t *pd = (uint32_t *)0xFFFFF000;
    for (uint32_t i = 0; i < 1024; i++) {
        uint32_t entry = pd[i];
        if (entry & 0x1) {
            uint32_t pt_phys = entry & 0xFFFFF000;
            esp_printf(console_putc, "PDE[%3u]: frame=0x%08x flags=0x%03x\n", i, pt_phys, entry & 0xFFF);
            volatile uint32_t *pt = (uint32_t *)(0xFFC00000 + (i * 0x1000));
            for (uint32_t j = 0; j < 4; j++) {
                uint32_t pte = pt[j];
                if (pte & 0x1) {
                    esp_printf(console_putc, "  PTE[%u]: frame=0x%08x flags=0x%03x\n", j, pte & 0xFFFFF000, pte & 0xFFF);
                }
            }
        }
    }
}

static void cmd_read32(int argc, char **argv) {
    if (argc < 2) {
        console_writeln("Usage: read32 <address>");
        return;
    }
    uint32_t addr = parse_hex(argv[1]);
    uint32_t value = *(volatile uint32_t *)(uintptr_t)addr;
    esp_printf(console_putc, "[0x%08x] = 0x%08x\n", addr, value);
}

static void cmd_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t ticks = timer_ticks();
    uint32_t ms = timer_milliseconds();
    esp_printf(console_putc, "Ticks: %u  (%u ms)\n", ticks, ms);
}

static int tokenize(char *line, char **argv) {
    int argc = 0;
    int i = 0;
    while (line[i] && argc < MAX_ARGS) {
        while (line[i] && is_space(line[i])) i++;
        if (!line[i]) break;
        argv[argc++] = &line[i];
        while (line[i] && !is_space(line[i])) i++;
        if (line[i]) {
            line[i] = '\0';
            i++;
        }
    }
    return argc;
}

static void dispatch_command(int argc, char **argv) {
    if (argc == 0) return;
    for (int i = 0; i < command_count(); i++) {
        if (strcmp_simple(argv[0], commands[i].name) == 0) {
            commands[i].fn(argc, argv);
            return;
        }
    }
    console_writeln("Unknown command. Type 'help'.");
}

void shell_run(void) {
    char line[MAX_INPUT];
    while (1) {
        shell_print_prompt();
        int len = 0;
        while (1) {
            char c = keyboard_read_char();
            if (c == '\r') c = '\n';
            if (c == '\n') {
                console_putc('\n');
                line[len] = '\0';
                break;
            } else if (c == '\b') {
                if (len > 0) {
                    len--;
                    console_backspace();
                }
            } else if (len < MAX_INPUT - 1) {
                line[len++] = c;
                console_putc(c);
            }
        }
        char *argv[MAX_ARGS];
        int argc = tokenize(line, argv);
        dispatch_command(argc, argv);
    }
}
