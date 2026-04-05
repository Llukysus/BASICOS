# 467OS – Minimal Educational Operating System

A minimal x86 OS for learning: multiboot bootloader, kernel in C with VGA and keyboard, basic multitasking (idle thread), and an **Interactive App Suite** (calculator, Tic-Tac-Toe, timer, notes with disk storage). Runs in QEMU. **Clean app UI**: each screen shows only the current app and a header (no scrollback history).

## Folder structure

```
467os/
├── boot/           # Bootloader (x86 assembly)
│   └── boot.asm
├── kernel/         # Kernel (C + linker script)
│   ├── kernel.c
│   ├── linker.ld
│   └── os.h
├── apps/           # Interactive App Suite
│   ├── menu.c
│   ├── calculator.c
│   ├── tictactoe.c
│   ├── timer.c
│   └── notes.c
├── grub/
│   └── grub.cfg
├── Makefile
└── README.md
```

## Prerequisites (Ubuntu / WSL)

Install build and run tools:

```bash
sudo apt update
sudo apt install -y build-essential nasm grub-pc-bin xorriso qemu-system-x86
```

- **gcc** – C compiler  
- **nasm** – assembler for `boot/boot.asm`  
- **grub-mkrescue**, **xorriso** – build bootable ISO  
- **qemu-system-x86** – emulator  

## Build

From the project root (`467os/`):

```bash
make
```

This compiles the bootloader and kernel (including apps) and produces **kernel.bin**.

## Create bootable ISO

```bash
make iso
```

This:

1. Builds `kernel.bin` if needed  
2. Creates `isodir/boot/` and copies `kernel.bin` and `grub/grub.cfg`  
3. Runs `grub-mkrescue` to generate **467os.iso**  

## Run in QEMU

```bash
make run
```

This builds the ISO (if needed), creates **disk.img** for the Notes app (if missing), and starts QEMU with the CD-ROM and the disk. You see the 467OS banner, then the Interactive App Suite menu.

- **1** – Calculator (e.g. `10 + 5`, `20 * 3`)  
- **2** – Tic-Tac-Toe (2 players, positions 1–9)  
- **3** – Timer (set seconds; screen blinks when done; stoppable in Timer app)  
- **4** – Notes (save/load to disk; **s** = save, **l** = load, **a** = add line, **q** = back)  

**Clean app UI:** The screen is cleared when you open the menu or an app. Only the current view and header are shown (no scrollback history).

To exit QEMU: type **`quit`** and Enter in the terminal (with `-monitor stdio`), or **Ctrl+A** then **X**.

## Quick reference

| Command       | Effect                          |
|---------------|----------------------------------|
| `make`        | Build `kernel.bin`               |
| `make iso`    | Build and create `467os.iso`     |
| `make run`    | Build ISO and run in QEMU        |
| `make clean`  | Remove build artifacts and ISO  |

## Design notes

- **Boot**: GRUB loads the multiboot kernel; `boot.asm` provides the multiboot header and entry, then calls `kernel_main()` in C.  
- **Kernel**: VGA text mode (80×25), PS/2 keyboard, ATA PIO disk (for Notes), and multitasking (round-robin).  
- **Apps**: Linked into the kernel; the menu launches Calculator, Tic-Tac-Toe, Timer, or Notes. Notes use **disk.img** (ATA) for persistence.

## AI usage explained

In this project, I used generative AI through Cursor as a tool to speed up development while maintaining full control over the design and implementation. I began by independently planning the entire system architecture, including the tech stack, project structure, core features, and implementation strategy. With this foundation in place, I used Cursor to assist in generating code that aligned with industry-standard syntax and best practices, improving development speed and code quality. Rather than relying on the generated output blindly, I reviewed every piece of code to make sure I understand everything and how it works to make sure that it matches my intended functionality. This process allowed me to catch any unwanted features, fill in missing components, and refine the code to meet my exact requirements.