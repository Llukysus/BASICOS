/*
 * =============================================================================
 * 467OS Kernel - Main kernel and system services
 * =============================================================================
 * Initializes the system, VGA text mode, keyboard, and basic multitasking
 * with an idle thread. Then runs the OS shell (Interactive App Suite menu).
 * =============================================================================
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* system tick counter is defined later; declare for early VGA/statusbar code */
extern volatile uint32_t system_ticks;

/* -----------------------------------------------------------------------------
 * VGA text mode - 80x25, memory-mapped at 0xB8000
 * With scrollback: keep many lines in memory, show 25 at a time; PgUp/PgDn scroll.
 * ----------------------------------------------------------------------------- */
#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_CONTENT_ROWS (VGA_HEIGHT - 1) /* row 0 reserved for status bar */
#define VGA_MEM     ((volatile uint16_t *)0xB8000)
#define SCROLLBACK_LINES 200

#define VGA_COLOR_BLACK   0
#define VGA_COLOR_WHITE   15
#define VGA_ENTRY(c, color) ((uint16_t)(c) | ((uint16_t)(color) << 8))

static uint8_t vga_color;

/* Scrollback: each line is space-padded to 80 chars + NUL so no ghost text on refresh */
static char line_buffer[SCROLLBACK_LINES][VGA_WIDTH + 1];
static int line_count;
static char current_line[VGA_WIDTH + 1];
static int current_len;
static int scroll_offset;   /* first logical line shown on screen (0 .. total_lines - 25) */

static inline int total_lines(void) { return line_count + 1; }

/* -----------------------------------------------------------------------------
 * Status bar (row 0): used for timer progress
 * ----------------------------------------------------------------------------- */
static bool statusbar_active;
static volatile bool statusbar_dirty;
static uint32_t statusbar_timer_start;
static uint32_t statusbar_timer_end;
static volatile uint32_t statusbar_clock_seconds;
static volatile char     statusbar_clock_spinner;

static void statusbar_draw_clock(char line[VGA_WIDTH])
{
    /* Render uptime clock at top-right: "MM:SS X" */
    uint32_t sec = statusbar_clock_seconds;
    uint32_t mm = (sec / 60) % 100;   /* 00-99 */
    uint32_t ss = sec % 60;
    char spin = statusbar_clock_spinner ? statusbar_clock_spinner : '|';

    int pos = VGA_WIDTH - 8; /* "MM:SS X" */
    if (pos < 0) return;
    line[pos + 0] = (char)('0' + (mm / 10));
    line[pos + 1] = (char)('0' + (mm % 10));
    line[pos + 2] = ':';
    line[pos + 3] = (char)('0' + (ss / 10));
    line[pos + 4] = (char)('0' + (ss % 10));
    line[pos + 5] = ' ';
    line[pos + 6] = spin;
    line[pos + 7] = ' ';
}

static void statusbar_draw_timer(uint32_t now_ticks)
{
    uint8_t attr = 0x1F; /* white on blue */
    char line[VGA_WIDTH];
    for (int i = 0; i < VGA_WIDTH; i++) line[i] = ' ';
    /* Reserve space for top-right clock/spinner. */
    const int clock_reserve = 10;
    const int usable_cols = VGA_WIDTH - clock_reserve;

    uint32_t total = (statusbar_timer_end > statusbar_timer_start) ? (statusbar_timer_end - statusbar_timer_start) : 1;
    uint32_t elapsed = (now_ticks > statusbar_timer_start) ? (now_ticks - statusbar_timer_start) : 0;
    if (elapsed > total) elapsed = total;

    uint32_t remaining_ticks = (statusbar_timer_end > now_ticks) ? (statusbar_timer_end - now_ticks) : 0;
    uint32_t remaining_sec = remaining_ticks / 100; /* PIT_FREQ_HZ = 100 */
    uint32_t percent = (elapsed * 100) / total;

    /* "TIMER 12s" */
    const char *lbl = "TIMER ";
    int p = 0;
    while (lbl[p] && p < usable_cols) { line[p] = lbl[p]; p++; }
    if (p < usable_cols && remaining_sec >= 100) line[p++] = (char)('0' + (remaining_sec / 100) % 10);
    if (p < usable_cols && remaining_sec >= 10)  line[p++] = (char)('0' + (remaining_sec / 10) % 10);
    if (p < usable_cols) line[p++] = (char)('0' + (remaining_sec % 10));
    if (p < usable_cols) line[p++] = 's';

    /* progress bar */
    int bar_start = 16;
    int bar_w = 30;
    if (bar_start + bar_w + 2 < usable_cols) {
        line[bar_start] = '[';
        line[bar_start + bar_w + 1] = ']';
        int filled = (int)((elapsed * (uint32_t)bar_w) / total);
        for (int i = 0; i < bar_w; i++)
            line[bar_start + 1 + i] = (i < filled) ? '#' : '-';
    }

    /* percent near right (leave room for clock): "100%" */
    int right = usable_cols - 3;
    if (right - 3 >= 0) {
        if (percent >= 100) {
            line[right - 3] = '1'; line[right - 2] = '0'; line[right - 1] = '0';
        } else {
            line[right - 3] = (char)('0' + (percent / 100) % 10);
            line[right - 2] = (char)('0' + (percent / 10) % 10);
            line[right - 1] = (char)('0' + (percent % 10));
        }
        line[right] = '%';
    }

    statusbar_draw_clock(line);

    for (int col = 0; col < VGA_WIDTH; col++)
        VGA_MEM[col] = VGA_ENTRY((unsigned char)line[col], attr);
}

static void statusbar_draw_base(void)
{
    uint8_t attr = 0x1F; /* white on blue */
    char line[VGA_WIDTH];
    for (int i = 0; i < VGA_WIDTH; i++) line[i] = ' ';
    statusbar_draw_clock(line);
    for (int col = 0; col < VGA_WIDTH; col++)
        VGA_MEM[col] = VGA_ENTRY((unsigned char)line[col], attr);
}

/* Copy the visible 25 rows from buffer to VGA memory */
static void vga_refresh(void)
{
    int max_offset = total_lines() - VGA_CONTENT_ROWS;
    if (max_offset < 0) max_offset = 0;
    if (scroll_offset > max_offset) scroll_offset = max_offset;

    /* Row 0 reserved for status bar; apps view starts at row 1. */
    for (int row = 1; row < VGA_HEIGHT; row++) {
        int logical = scroll_offset + (row - 1);
        const char *src;
        if (logical < line_count) {
            src = line_buffer[logical];
        } else if (logical == line_count) {
            src = current_line;
        } else {
            src = NULL;
        }
        for (int col = 0; col < VGA_WIDTH; col++) {
            /* Only show chars before the first NUL; rest are space (avoids ghost text) */
            char ch = ' ';
            if (src && src[col] != '\0')
                ch = src[col];
            VGA_MEM[row * VGA_WIDTH + col] = VGA_ENTRY((unsigned char)ch, vga_color);
        }
    }

    /* Render status bar last so it always stays visible. */
    if (statusbar_active)
        statusbar_draw_timer(system_ticks);
    else
        statusbar_draw_base();
}

/* Initialize VGA and scrollback state */
static void vga_init(void)
{
    vga_color = VGA_ENTRY(0, VGA_COLOR_WHITE) >> 8;
    line_count = 0;
    current_len = 0;
    current_line[0] = '\0';
    scroll_offset = 0;
    statusbar_active = false;
    statusbar_dirty = true;
    statusbar_clock_seconds = 0;
    statusbar_clock_spinner = '|';
    for (int i = 0; i < SCROLLBACK_LINES; i++)
        line_buffer[i][0] = '\0';
    vga_refresh();
}

/* Clear screen and scrollback (fresh view when switching app) */
static void vga_clear_all(void)
{
    line_count = 0;
    current_len = 0;
    current_line[0] = '\0';
    scroll_offset = 0;
    for (int i = 0; i < SCROLLBACK_LINES; i++)
        line_buffer[i][0] = '\0';
    vga_refresh();
}

/* Push current line into buffer (space-pad to 80 chars) and start a new line */
static void vga_newline(void)
{
    /* Pad current line to 80 chars with spaces so no ghost text from leftover bytes */
    for (int i = current_len; i < VGA_WIDTH; i++)
        current_line[i] = ' ';
    current_line[VGA_WIDTH] = '\0';

    if (line_count < SCROLLBACK_LINES) {
        for (int i = 0; i < VGA_WIDTH; i++)
            line_buffer[line_count][i] = current_line[i];
        line_buffer[line_count][VGA_WIDTH] = '\0';
        line_count++;
    } else {
        for (int i = 0; i < SCROLLBACK_LINES - 1; i++)
            for (int j = 0; j < VGA_WIDTH + 1; j++)
                line_buffer[i][j] = line_buffer[i + 1][j];
        for (int i = 0; i < VGA_WIDTH + 1; i++)
            line_buffer[SCROLLBACK_LINES - 1][i] = current_line[i];
    }
    /* Clear entire current_line so no old text bleeds into next line */
    current_len = 0;
    for (int i = 0; i < VGA_WIDTH + 1; i++)
        current_line[i] = '\0';
    scroll_offset = total_lines() - VGA_CONTENT_ROWS;
    if (scroll_offset < 0) scroll_offset = 0;
    vga_refresh();
}

/* Put a character: append to current line or newline; then refresh */
static void vga_putchar(char c)
{
    if (c == '\n') {
        vga_newline();
        return;
    }
    if (c == '\r') {
        return;
    }
    if (c == '\b') {
        /* Backspace: remove last character from current line */
        if (current_len > 0) {
            current_len--;
            current_line[current_len] = '\0';
        }
        scroll_offset = total_lines() - VGA_CONTENT_ROWS;
        if (scroll_offset < 0) scroll_offset = 0;
        vga_refresh();
        return;
    }
    if (current_len < VGA_WIDTH) {
        current_line[current_len++] = c;
        current_line[current_len] = '\0';
    } else {
        vga_newline();
        current_line[0] = c;
        current_len = 1;
        current_line[1] = '\0';
    }
    scroll_offset = total_lines() - VGA_CONTENT_ROWS;
    if (scroll_offset < 0) scroll_offset = 0;
    vga_refresh();
}

/* Scroll view up/down by one page or one line (used by keyboard) */
void vga_scroll_up(void)
{
    if (scroll_offset > 0) {
        scroll_offset -= VGA_CONTENT_ROWS;
        if (scroll_offset < 0) scroll_offset = 0;
        vga_refresh();
    }
}
void vga_scroll_down(void)
{
    int max_offset = total_lines() - VGA_CONTENT_ROWS;
    if (max_offset > 0 && scroll_offset < max_offset) {
        scroll_offset += VGA_CONTENT_ROWS;
        if (scroll_offset > max_offset) scroll_offset = max_offset;
        vga_refresh();
    }
}
void vga_scroll_line_up(void)
{
    if (scroll_offset > 0) {
        scroll_offset--;
        vga_refresh();
    }
}
void vga_scroll_line_down(void)
{
    int max_offset = total_lines() - VGA_CONTENT_ROWS;
    if (max_offset > 0 && scroll_offset < max_offset) {
        scroll_offset++;
        vga_refresh();
    }
}

/* Print a null-terminated string */
static void vga_puts(const char *s)
{
    while (*s)
        vga_putchar(*s++);
}

/* -----------------------------------------------------------------------------
 * Keyboard - read from port 0x60 (PS/2 keyboard data)
 * ----------------------------------------------------------------------------- */
static inline uint8_t inb(uint16_t port)
{
    uint8_t r;
    __asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static inline void outb(uint16_t port, uint8_t v)
{
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t r;
    __asm__ volatile("inw %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static inline void outw(uint16_t port, uint16_t v)
{
    __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(port));
}

/* Scancode set 1 (US QWERTY) - key press only (ignore release 0x80+). 0x01 = Esc -> 0x1B */
static const char keyboard_map[] = {
    0, 0x1B, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
};
/* Shifted characters (0 = use unshifted); index = scancode. E.g. '=' (scancode 0x0D) -> '+' */
static const char keyboard_shift_map[] = {
    0, 0x1B, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0
};
#define SCAN_LSHIFT 0x2A
#define SCAN_RSHIFT 0x36
#define SCAN_LSHIFT_UP 0xAA
#define SCAN_RSHIFT_UP 0xB6

static char keyboard_scancode_to_ascii(uint8_t scancode, bool shifted)
{
    if (scancode & 0x80)
        return 0;
    if (scancode >= (int)(sizeof(keyboard_map) / sizeof(keyboard_map[0])))
        return 0;
    if (shifted && scancode < (int)(sizeof(keyboard_shift_map) / sizeof(keyboard_shift_map[0]))
        && keyboard_shift_map[scancode])
        return keyboard_shift_map[scancode];
    return keyboard_map[scancode];
}

/* Forward declaration (defined in multitasking section below) */
void task_yield(void);

/* Check if a key is available */
static bool keyboard_has_key(void)
{
    return (inb(0x64) & 1) != 0;
}

/* Scancode set 1: cursor and page keys (press only; ignore release 0x80+) */
#define SCAN_UP    0x48
#define SCAN_DOWN  0x50
#define SCAN_PGUP  0x49
#define SCAN_PGDN  0x51

static bool shift_pressed;

/* Kernel timer state: set in IRQ0 handler, consumed in keyboard wait loop */
static uint32_t timer_end_ticks;
static bool     timer_armed;
static bool     timer_do_blink;

void os_screen_blink_twice(void);   /* implemented later; used when timer fires in wait loop */

/* Get next key press; PgUp/PgDn/Up/Down scroll; Shift+key gives shifted character (e.g. = -> +) */
static char keyboard_getkey(void)
{
    for (;;) {
        while (!keyboard_has_key()) {
            if (statusbar_dirty) {
                statusbar_dirty = false;
                if (statusbar_active)
                    statusbar_draw_timer(system_ticks);
                else
                    statusbar_draw_base();  /* show clock in top-right when no timer */
            }
            if (timer_do_blink) {
                timer_do_blink = false;
                vga_puts("\n*** TIMER! ***\n");
                os_screen_blink_twice();
                statusbar_active = false;
                statusbar_dirty = true;
            }
            task_yield();   /* sleep until next tick */
        }
        uint8_t sc = inb(0x60);
        if (sc == SCAN_LSHIFT || sc == SCAN_RSHIFT) { shift_pressed = true; continue; }
        if (sc == SCAN_LSHIFT_UP || sc == SCAN_RSHIFT_UP) { shift_pressed = false; continue; }
        if (sc & 0x80) continue;   /* ignore other key releases */
        if (sc == SCAN_PGUP) { vga_scroll_up(); continue; }
        if (sc == SCAN_PGDN) { vga_scroll_down(); continue; }
        if (sc == SCAN_UP)   { vga_scroll_line_up(); continue; }
        if (sc == SCAN_DOWN) { vga_scroll_line_down(); continue; }
        return keyboard_scancode_to_ascii(sc, shift_pressed);
    }
}

/* Read a line into buf (max len chars); returns when user presses Enter or Esc (Esc -> buf[0]=0x1B) */
static void keyboard_readline(char *buf, size_t len)
{
    size_t i = 0;
    while (i < len - 1) {
        char c = keyboard_getkey();
        if (c == 0x1B) {
            buf[0] = 0x1B;
            buf[1] = '\0';
            return;
        }
        if (c == '\n' || c == '\r') {
            buf[i] = '\0';
            vga_putchar('\n');
            return;
        }
        if (c == '\b' && i > 0) {
            i--;
            vga_putchar('\b');   /* kernel now handles \b: removes last char from line */
            continue;
        }
        if (c >= 32) {
            buf[i++] = c;
            vga_putchar(c);
        }
    }
    buf[i] = '\0';
}

/* -----------------------------------------------------------------------------
 * PIT (Programmable Interval Timer) + IDT for system_ticks (real time)
 * ----------------------------------------------------------------------------- */
#define PIT_FREQ_HZ   100
#define PIT_BASE     1193180

volatile uint32_t system_ticks;
bool timer_available;   /* true only when PIT + sti are enabled */

extern void dummy_idt_handler(void);
extern void irq0_handler(void);

/* IDT entry: offset lo, selector, zero, type, offset hi */
struct idt_entry { uint16_t off_lo; uint16_t sel; uint8_t zero; uint8_t type; uint16_t off_hi; };
static struct idt_entry idt[256];
struct { uint16_t limit; uint32_t base; } __attribute__((packed)) idt_desc;

static void pic_remap(void)
{
    /* Mask all interrupts first, then reinit PIC so IRQ0 = int 0x20 */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
    outb(0x20, 0x11);   /* ICW1: edge, cascade, need ICW4 */
    outb(0xA0, 0x11);
    outb(0x21, 0x20);   /* ICW2: vector 0x20 for PIC1 */
    outb(0xA1, 0x28);   /* ICW2: vector 0x28 for PIC2 */
    outb(0x21, 0x04);   /* ICW3: slave on IRQ2 */
    outb(0xA1, 0x02);
    outb(0x21, 0x01);   /* ICW4: 8086 mode */
    outb(0xA1, 0x01);
    outb(0x21, 0xFE);   /* unmask IRQ0 only (timer) */
    outb(0xA1, 0xFF);
}

static void idt_set_gate(int vec, void *handler)
{
    uint32_t addr = (uint32_t)handler;
    idt[vec].off_lo = addr & 0xFFFF;
    /* In this GRUB environment our kernel code segment is 0x10 (see QEMU log CS=0x10). */
    idt[vec].sel    = 0x10;
    idt[vec].zero   = 0;
    idt[vec].type   = 0x8E;
    idt[vec].off_hi = (addr >> 16) & 0xFFFF;
}

static void idt_init(void)
{
    /* Every vector points to dummy first (handles spurious / wrong vector) */
    for (int i = 0; i < 256; i++)
        idt_set_gate(i, dummy_idt_handler);
    /* IRQ0 (timer) = vector 0x20 */
    idt_set_gate(0x20, irq0_handler);
    idt_desc.limit = sizeof(idt) - 1;
    idt_desc.base  = (uint32_t)idt;
    __asm__ volatile("lidt %0" : : "m"(idt_desc));
}

static void pit_init(void);
static void pit_init(void)
{
    uint32_t div = PIT_BASE / PIT_FREQ_HZ;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(div & 0xFF));
    outb(0x40, (uint8_t)((div >> 8) & 0xFF));
}

/* -----------------------------------------------------------------------------
 * Real multi-tasking: N tasks, round-robin scheduler, create_task/destroy_task
 * Task 0 = main (kernel stack). Others get their own stack; first switch "rets" to entry.
 * ----------------------------------------------------------------------------- */
#define TASK_STACK_SIZE 2048
#define MAX_TASKS       8

typedef struct {
    uint32_t stack[TASK_STACK_SIZE];
    uint32_t saved_esp;
    bool     in_use;
} tcb_t;

static tcb_t tasks[MAX_TASKS];
static int   current_task;

static inline void cpu_hlt(void) { __asm__ volatile("hlt"); }
static inline void irq_disable(void) { __asm__ volatile("cli"); }
static inline void irq_enable(void) { __asm__ volatile("sti"); }

/* Voluntary yield: sleep until next interrupt (PIT tick). Scheduler is preemptive. */
void task_yield(void) { cpu_hlt(); }

/* -----------------------------------------------------------------------------
 * Background clock task (shows uptime at top-right)
 * ----------------------------------------------------------------------------- */
static void clock_task(void)
{
    static const char spins[] = { '|', '/', '-', '\\' };
    uint32_t last_sec = 0xFFFFFFFFu;
    int spin_i = 0;
    for (;;) {
        uint32_t now_sec = system_ticks / 100; /* PIT_FREQ_HZ = 100 */
        if (now_sec != last_sec) {
            last_sec = now_sec;
            statusbar_clock_seconds = now_sec;
            statusbar_clock_spinner = spins[spin_i++ & 3];
            statusbar_dirty = true;
        }
        task_yield();
    }
}

/* Choose next runnable task (round-robin). */
static int pick_next_task(void)
{
    int next = (current_task + 1) % MAX_TASKS;
    while (!tasks[next].in_use && next != current_task)
        next = (next + 1) % MAX_TASKS;
    return next;
}

/* IRQ0 handler in C. esp points to pushad frame; below it is the CPU iret frame. Returns new esp. */
uint32_t irq0_handler_c(uint32_t esp)
{
    system_ticks++;
    if (timer_armed && system_ticks >= timer_end_ticks) {
        timer_armed = false;
        timer_do_blink = true;
        statusbar_dirty = true;
    }
    if (statusbar_active && (system_ticks % 10) == 0)
        statusbar_dirty = true;

    /* Save current task context */
    tasks[current_task].saved_esp = esp;

    /* Preempt every tick: pick next runnable task */
    int next = pick_next_task();
    current_task = next;
    return tasks[current_task].saved_esp;
}

/* Create a new task that will run entry() when first scheduled. Returns task id or -1. */
int task_create(void (*entry)(void))
{
    for (int i = 1; i < MAX_TASKS; i++) {
        if (!tasks[i].in_use) {
            /* Build an initial stack frame that matches IRQ0 handler expectations:
             * [edi..eax] from pushad, then [eip, cs, eflags] for iret.
             *
             * After irq0_handler does:
             *   pusha
             *   ... maybe switch esp ...
             *   popa
             *   iret
             *
             * the stack pointer MUST point at a pushad frame (top=EDI),
             * followed by a valid iret frame (EIP/CS/EFLAGS). */
            uint32_t *sp = &tasks[i].stack[TASK_STACK_SIZE];
            /* iret frame (low->high): EIP, CS, EFLAGS. We push in reverse. */
            *(--sp) = 0x00000202;          /* EFLAGS (IF=1) */
            *(--sp) = 0x00000010;          /* CS (kernel code selector) */
            *(--sp) = (uint32_t)entry;     /* EIP */

            /* pushad frame (low->high): EDI,ESI,EBP,ESP,EBX,EDX,ECX,EAX.
             * We push in reverse so that saved_esp points to EDI (top of stack). */
            *(--sp) = 0; /* eax */
            *(--sp) = 0; /* ecx */
            *(--sp) = 0; /* edx */
            *(--sp) = 0; /* ebx */
            *(--sp) = 0; /* esp (dummy slot popped by popa) */
            *(--sp) = 0; /* ebp */
            *(--sp) = 0; /* esi */
            *(--sp) = 0; /* edi */
            tasks[i].saved_esp = (uint32_t)sp;
            tasks[i].in_use = true;
            return i;
        }
    }
    return -1;
}

void task_destroy(int id)
{
    if (id > 0 && id < MAX_TASKS && tasks[id].in_use) {
        tasks[id].in_use = false;
    }
}

/* Call once at boot: task 0 = main thread. */
static void task_init(void)
{
    for (int i = 0; i < MAX_TASKS; i++)
        tasks[i].in_use = false;
    tasks[0].in_use = true;
    current_task = 0;
}

/* -----------------------------------------------------------------------------
 * Application suite declarations (implemented in apps/)
 * ----------------------------------------------------------------------------- */
void interactive_app_suite_menu(void);

/* -----------------------------------------------------------------------------
 * Kernel entry point - called from boot.asm
 * ----------------------------------------------------------------------------- */
void kernel_main(uint32_t magic, uint32_t mboot_info)
{
    (void)magic;
    (void)mboot_info;

    vga_init();
    vga_puts("467OS - Minimal Educational Operating System\n");
    vga_puts("Kernel loaded. Initializing...\n");

    task_init();
    pic_remap();
    idt_init();
    vga_puts("VGA: OK  Keyboard: OK  ");
    pit_init();
    timer_available = true;
    irq_enable();
    vga_puts("Timer (PIT 100Hz)  Preemption: ready\n");
    vga_puts("Starting Interactive App Suite.\n\n");

    /* Start background clock as a second task. */
    (void)task_create(clock_task);

    /* Shell loop: menu is always available (like Windows desktop). "Quit" returns here. */
    for (;;) {
        interactive_app_suite_menu();
        vga_puts("\nBack to menu.\n\n");
    }
}

/* Delay for approximately n ticks (yields so timer IRQ can run) */
static void delay_ticks(uint32_t n)
{
    uint32_t t = system_ticks;
    while (system_ticks - t < n)
        cpu_hlt();
}

/* Blink entire screen white twice (used when timer fires). Use many ticks so it's visible. */
void os_screen_blink_twice(void)
{
    uint32_t delay = 80;   /* ~80 yields per phase so white stays visible */
    for (int blink = 0; blink < 2; blink++) {
        for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
            VGA_MEM[i] = VGA_ENTRY(' ', 0x0F);  /* white bg */
        delay_ticks(delay);
        vga_refresh();
        delay_ticks(delay);
    }
}

/* -----------------------------------------------------------------------------
 * ATA PIO disk driver (primary master, LBA28). Used for notes storage.
 * ----------------------------------------------------------------------------- */
#define ATA_DATA    0x1F0
#define ATA_SECCNT  0x1F2
#define ATA_LBA0    0x1F3
#define ATA_LBA1    0x1F4
#define ATA_LBA2    0x1F5
#define ATA_DRIVE   0x1F6
#define ATA_CMD     0x1F7
#define ATA_BSY     0x80
#define ATA_DRQ     0x08

static void ata_wait(void)
{
    while (inb(ATA_CMD) & ATA_BSY)
        ;
}

static int ata_read_sector(uint32_t lba, void *buf)
{
    ata_wait();
    outb(ATA_DRIVE, 0xE0 | (uint8_t)(lba >> 24));
    outb(ATA_SECCNT, 1);
    outb(ATA_LBA0, (uint8_t)(lba));
    outb(ATA_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_LBA2, (uint8_t)(lba >> 16));
    outb(ATA_CMD, 0x20);
    ata_wait();
    if (!(inb(ATA_CMD) & ATA_DRQ))
        return -1;
    uint16_t *p = (uint16_t *)buf;
    for (int i = 0; i < 256; i++)
        p[i] = inw(ATA_DATA);
    return 0;
}

static int ata_write_sector(uint32_t lba, const void *buf)
{
    ata_wait();
    outb(ATA_DRIVE, 0xE0 | (uint8_t)(lba >> 24));
    outb(ATA_SECCNT, 1);
    outb(ATA_LBA0, (uint8_t)(lba));
    outb(ATA_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_LBA2, (uint8_t)(lba >> 16));
    outb(ATA_CMD, 0x30);
    ata_wait();
    if (!(inb(ATA_CMD) & ATA_DRQ))
        return -1;
    const uint16_t *p = (const uint16_t *)buf;
    for (int i = 0; i < 256; i++)
        outw(ATA_DATA, p[i]);
    return 0;
}

#define NOTES_SECTOR0_SIZE 508
#define NOTES_MAX_SECTORS  100
#define NOTES_MAX_SIZE     (4 + NOTES_SECTOR0_SIZE + (NOTES_MAX_SECTORS - 1) * 512)

/* Notes on disk: sector 0 = length (4 bytes LE) + first 508 bytes; sectors 1.. = 512 each */
int os_notes_load(char *buf, uint32_t max_len)
{
    uint8_t sec0[512];
    if (ata_read_sector(0, sec0) != 0)
        return -1;
    uint32_t len = (uint32_t)sec0[0] | ((uint32_t)sec0[1] << 8) | ((uint32_t)sec0[2] << 16) | ((uint32_t)sec0[3] << 24);
    if (len > max_len - 1 || len > NOTES_MAX_SIZE)
        len = max_len - 1;
    if (len == 0) {
        buf[0] = '\0';
        return 0;
    }
    uint32_t copied = 0;
    if (len <= NOTES_SECTOR0_SIZE) {
        for (uint32_t i = 0; i < len; i++)
            buf[i] = sec0[4 + i];
        buf[len] = '\0';
        return (int)len;
    }
    for (uint32_t i = 0; i < NOTES_SECTOR0_SIZE; i++)
        buf[copied++] = sec0[4 + i];
    uint32_t remaining = len - NOTES_SECTOR0_SIZE;
    uint32_t sector = 1;
    while (remaining > 0 && sector < NOTES_MAX_SECTORS) {
        uint8_t sec[512];
        if (ata_read_sector(sector, sec) != 0)
            break;
        uint32_t n = remaining > 512 ? 512 : remaining;
        for (uint32_t i = 0; i < n; i++)
            buf[copied++] = sec[i];
        remaining -= n;
        sector++;
    }
    buf[copied] = '\0';
    return (int)copied;
}

int os_notes_save(const char *buf, uint32_t len)
{
    if (len > NOTES_MAX_SIZE)
        len = NOTES_MAX_SIZE;
    uint8_t sec0[512];
    sec0[0] = (uint8_t)(len);
    sec0[1] = (uint8_t)(len >> 8);
    sec0[2] = (uint8_t)(len >> 16);
    sec0[3] = (uint8_t)(len >> 24);
    uint32_t written = 0;
    for (uint32_t i = 0; i < NOTES_SECTOR0_SIZE && i < len; i++)
        sec0[4 + i] = (uint8_t)buf[i];
    written = len < NOTES_SECTOR0_SIZE ? len : NOTES_SECTOR0_SIZE;
    if (ata_write_sector(0, sec0) != 0)
        return -1;
    uint32_t remaining = len - written;
    uint32_t sector = 1;
    while (remaining > 0 && sector < NOTES_MAX_SECTORS) {
        uint8_t sec[512];
        uint32_t n = remaining > 512 ? 512 : remaining;
        for (uint32_t i = 0; i < n; i++)
            sec[i] = (uint8_t)buf[written + i];
        for (uint32_t i = n; i < 512; i++)
            sec[i] = 0;
        if (ata_write_sector(sector, sec) != 0)
            return -1;
        written += n;
        remaining -= n;
        sector++;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
 * OS API for apps: display, keyboard, time, timer cancel, yield, notes
 * ----------------------------------------------------------------------------- */
void os_putchar(char c)   { vga_putchar(c); }
void os_puts(const char *s) { vga_puts(s); }
void os_clear_screen(void) { vga_clear_all(); }
char os_getkey(void)      { return keyboard_getkey(); }
void os_readline(char *buf, size_t len) { keyboard_readline(buf, len); }
void os_yield(void)       { task_yield(); }
uint32_t os_ticks(void)   { return system_ticks; }
int os_timer_available(void) { return timer_available ? 1 : 0; }
void os_timer_arm(uint32_t ticks_from_now)
{
    irq_disable();
    statusbar_timer_start = system_ticks;
    timer_end_ticks = system_ticks + ticks_from_now;
    statusbar_timer_end = timer_end_ticks;
    timer_armed = true;
    statusbar_active = true;
    statusbar_dirty = true;
    irq_enable();
}
void os_timer_disarm(void)
{
    irq_disable();
    timer_armed = false;
    statusbar_active = false;
    statusbar_dirty = true;
    irq_enable();
}
int os_task_create(void (*entry)(void)) { return task_create(entry); }
void os_task_destroy(int id)       { task_destroy(id); }
void os_task_exit_self(void)       { task_destroy(current_task); }
