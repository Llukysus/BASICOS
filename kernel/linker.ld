/* =============================================================================
 * 467OS Kernel Linker Script
 * =============================================================================
 * Tells the linker how to arrange kernel sections. The multiboot header must
 * appear early so GRUB can find it. Entry point is _start from boot.asm.
 * ============================================================================= */

ENTRY(_start)

SECTIONS
{
    /* Kernel is loaded at 1MB by GRUB (multiboot convention) */
    . = 1M;

    /* Multiboot header must be first */
    .multiboot ALIGN(4K) : AT(ADDR(.multiboot))
    {
        *(.multiboot)
    }

    .text ALIGN(4K) : AT(ADDR(.text))
    {
        *(.text .text.*)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata))
    {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4K) : AT(ADDR(.data))
    {
        *(.data .data.*)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss))
    {
        *(COMMON)
        *(.bss .bss.*)
        . = ALIGN(16);
    }

    /* End of kernel for size calculation */
    _kernel_end = .;
}
