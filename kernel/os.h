/*
 * 467OS - OS API for user applications
 * Declarations for display, keyboard, time, timer (implemented in kernel.c)
 */
#ifndef OS_H
#define OS_H

#define OS_KEY_ESC 0x1B

typedef unsigned long size_t;
typedef unsigned long uint32_t;

void os_putchar(char c);
void os_puts(const char *s);
void os_clear_screen(void);
char os_getkey(void);
void os_readline(char *buf, size_t len);
void os_yield(void);

uint32_t os_ticks(void);
int os_timer_available(void);
void os_screen_blink_twice(void);
void os_timer_arm(uint32_t ticks_from_now);
void os_timer_disarm(void);

int os_task_create(void (*entry)(void));
void os_task_destroy(int id);
void os_task_exit_self(void);

int os_notes_load(char *buf, uint32_t max_len);
int os_notes_save(const char *buf, uint32_t len);

#endif
