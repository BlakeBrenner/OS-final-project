# OS Final Project: Kernel Shell with System Introspection  
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

## Repository  
https://github.com/BlakeBrenner/OS-final-project

## Project Overview  
This project implements a **kernel-resident command shell** for a teaching OS kernel.  
Instead of a static kernel that boots and halts, this shell allows interactive commands after boot.  
You can inspect memory, view page tables, check timer uptime, read/write memory, and more.

## Key Features  
- Interrupt-driven keyboard input & VGA text console prompt  
- Line-editing, backspace support, tokenized commands  
- Memory introspection: `meminfo`, `frames`, `alloc`, `free`  
- Paging commands: `v2p`, `ptdump`, `read32`, `write32`  
- Timer commands: `uptime`, (optional) `sleep <ms>`  
- Additional commands: `help`, `cls`, `echo <text>`  
- Easily extensible for future debugging commands

## Architecture  
1. **Keyboard subsystem**  
   - IRQ1 handler captures scancodes → ASCII  
   - Ring buffer for characters  
   - `keyboard_read_char()` API consumed by shell

2. **Shell subsystem**  
   - `shell_init()` and `shell_run()`  
   - Prompt (`> `), input buffer, backspace handling  
   - Command parser + dispatcher table

3. **Command modules**  
   - Memory / allocator support  
   - Paging support via recursive mapping  
   - Timer support via PIT (IRQ0 tick counter)  
   - Utilities and debugging commands

## Getting Started  
### Prerequisites  
- QEMU (or other x86 emulator)  
- `make`, `nasm`, `gcc` toolchain for kernel build  
- Multiboot2-compatible bootloader (e.g., GRUB)

### Build & Run  
```bash
git clone https://github.com/BlakeBrenner/OS-final-project.git
cd OS-final-project
make
qemu-system-i386 -kernel build/kernel.bin
