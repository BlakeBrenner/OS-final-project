; multiboot2.s - Multiboot2 header for GRUB
section .multiboot
align 8

multiboot2_header_start:
    dd 0xE85250D6                ; Magic number (multiboot2)
    dd 0                         ; Architecture (0 = i386)
    dd multiboot2_header_end - multiboot2_header_start
    dd -(0xE85250D6 + 0 + (multiboot2_header_end - multiboot2_header_start))

    ; End tag
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
multiboot2_header_end: