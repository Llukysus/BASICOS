# =============================================================================
# 467OS Makefile - Build bootloader, kernel, apps, and bootable ISO
# =============================================================================
# Usage: make          Build kernel.bin
#        make iso      Create 467os.iso (requires grub-mkrescue, xorriso)
#        make run      Run OS in QEMU
#        make clean    Remove build artifacts
# =============================================================================

# Tools
CC       = gcc
ASM      = nasm
LD       = ld
GRUB     = grub-mkrescue
QEMU     = qemu-system-x86_64

# Flags: freestanding (no libc), 32-bit for multiboot
# -nostdlib = no libc link; we keep standard headers for types (stdint.h, stddef.h, stdbool.h)
CFLAGS   = -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib \
           -Wall -Wextra -O2 -I. -Ikernel
ASMFLAGS = -f elf32
LDFLAGS  = -m elf_i386 -T kernel/linker.ld -nostdlib -z noexecstack

# Output
KERNEL_BIN = kernel.bin
ISO_IMAGE  = 467os.iso
ISODIR     = isodir

# Object files: boot + kernel + apps
BOOT_OBJ   = boot/boot.o boot/irq.o
KERNEL_OBJ = kernel/kernel.o
APP_OBJS   = apps/menu.o apps/calculator.o apps/tictactoe.o apps/timer.o apps/notes.o
ALL_OBJS   = $(BOOT_OBJ) $(KERNEL_OBJ) $(APP_OBJS)

.PHONY: all iso run clean

all: $(KERNEL_BIN)

# -----------------------------------------------------------------------------
# Assemble bootloader (multiboot header + entry point)
# -----------------------------------------------------------------------------
boot/boot.o: boot/boot.asm
	$(ASM) $(ASMFLAGS) $< -o $@

# -----------------------------------------------------------------------------
# Compile kernel C code
# -----------------------------------------------------------------------------
kernel/kernel.o: kernel/kernel.c kernel/os.h
	$(CC) $(CFLAGS) -c kernel/kernel.c -o $@

# -----------------------------------------------------------------------------
# Compile application suite (linked into kernel)
# -----------------------------------------------------------------------------
apps/menu.o: apps/menu.c kernel/os.h
	$(CC) $(CFLAGS) -c apps/menu.c -o $@

apps/calculator.o: apps/calculator.c kernel/os.h
	$(CC) $(CFLAGS) -c apps/calculator.c -o $@

apps/tictactoe.o: apps/tictactoe.c kernel/os.h
	$(CC) $(CFLAGS) -c apps/tictactoe.c -o $@

# -----------------------------------------------------------------------------
# Link kernel (boot.o + kernel.o + apps) into single ELF for GRUB multiboot
# -----------------------------------------------------------------------------
boot/irq.o: boot/irq.asm
	$(ASM) $(ASMFLAGS) $< -o $@

apps/timer.o: apps/timer.c kernel/os.h
	$(CC) $(CFLAGS) -c apps/timer.c -o $@

apps/notes.o: apps/notes.c kernel/os.h
	$(CC) $(CFLAGS) -c apps/notes.c -o $@

$(KERNEL_BIN): $(ALL_OBJS)
	$(LD) $(LDFLAGS) $(ALL_OBJS) -o $@

# -----------------------------------------------------------------------------
# Create bootable ISO with GRUB
# -----------------------------------------------------------------------------
iso: $(KERNEL_BIN)
	mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL_BIN) $(ISODIR)/boot/
	cp grub/grub.cfg $(ISODIR)/boot/grub/
	$(GRUB) -o $(ISO_IMAGE) $(ISODIR)
	@echo "Created $(ISO_IMAGE) - run with: make run"

# -----------------------------------------------------------------------------
# Disk image for Notes app (ATA). Create once; reused across runs.
# -----------------------------------------------------------------------------
disk.img:
	dd if=/dev/zero of=disk.img bs=512 count=2048 2>/dev/null || true

# -----------------------------------------------------------------------------
# Run in QEMU (use ISO so boot path matches GRUB; disk.img for notes storage)
# Monitor in same terminal: type "quit" and Enter to exit (Ctrl+A X often fails in WSL)
# -----------------------------------------------------------------------------
run: iso disk.img
	$(QEMU) -cdrom $(ISO_IMAGE) -drive file=disk.img,format=raw,if=ide -no-reboot -no-shutdown -monitor stdio

# Run kernel.bin directly (alternative; GRUB not used)
run-kernel: $(KERNEL_BIN)
	$(QEMU) -kernel $(KERNEL_BIN) -no-reboot -no-shutdown

clean:
	rm -f $(ALL_OBJS) $(KERNEL_BIN) $(ISO_IMAGE)
	rm -rf $(ISODIR)
