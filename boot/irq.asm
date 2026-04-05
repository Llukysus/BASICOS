; =============================================================================
; IDT handlers - dummy for unknown vectors, irq0 for PIT (vector 0x20).
; =============================================================================
global dummy_idt_handler
global irq0_handler
extern irq0_handler_c

; Any unexpected or spurious interrupt: just return (CPU already pushed EIP, CS, EFLAGS).
dummy_idt_handler:
    iret

irq0_handler:
    pusha
    push esp                 ; pass pointer to saved regs + iret frame
    call irq0_handler_c
    add esp, 4
    mov esp, eax             ; switch to (possibly new) task stack
    mov al, 0x20
    out 0x20, al          ; EOI to PIC1
    popa
    iret
