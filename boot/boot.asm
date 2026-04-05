; =============================================================================
; 467OS Bootloader - x86 Assembly
; =============================================================================
; This file provides the Multiboot header and entry point. GRUB loads our
; kernel and jumps here. We set up a minimal environment and call kernel_main.
; =============================================================================

; Multiboot 1 specification constants
MBOOT_HEADER_MAGIC equ 0x1BADB002
MBOOT_PAGE_ALIGN   equ (1 << 0)   ; Align modules on page boundaries
MBOOT_MEM_INFO     equ (1 << 1)   ; Provide memory map
MBOOT_HEADER_FLAGS equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
MBOOT_HEADER_CHKSUM equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

; -----------------------------------------------------------------------------
; Multiboot header - must be in the first 8KB of the kernel for GRUB to find it
; -----------------------------------------------------------------------------
section .multiboot
align 4
    dd MBOOT_HEADER_MAGIC
    dd MBOOT_HEADER_FLAGS
    dd MBOOT_HEADER_CHKSUM

; -----------------------------------------------------------------------------
; Initial stack - kernel needs a stack before we call C code
; -----------------------------------------------------------------------------
section .bss
align 16
stack_bottom:
    resb 16384                  ; 16 KB stack
stack_top:

; -----------------------------------------------------------------------------
; Entry point - GRUB jumps here with:
;   EAX = 0x2BADB002 (multiboot magic)
;   EBX = physical address of multiboot info structure
; -----------------------------------------------------------------------------
section .text
global _start
extern kernel_main

_start:
    ; Disable interrupts until kernel sets up IDT
    cli

    ; Set up stack pointer (stack grows downward on x86)
    mov esp, stack_top

    ; Push multiboot info for kernel_main(magic, mboot_info*)
    push ebx                     ; Multiboot info structure pointer
    push eax                     ; Multiboot magic number

    ; Call the C kernel entry point
    call kernel_main

    ; If kernel_main ever returns, disable interrupts and halt
halt_loop:
    cli
    hlt
    jmp halt_loop

; -----------------------------------------------------------------------------
; Mark stack as non-executable (silences linker warning)
; -----------------------------------------------------------------------------
section .note.GNU-stack progbits
