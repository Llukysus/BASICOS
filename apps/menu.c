/*
 * =============================================================================
 * Interactive App Suite - Main menu
 * =============================================================================
 * Presents a menu to choose: Calculator, Tic-Tac-Toe, or Quit (back to shell).
 * =============================================================================
 */

#include "../kernel/os.h"

/* Forward declarations for apps */
extern void run_calculator(void);
extern void run_tictactoe(void);
extern void run_timer(void);
extern void run_notes(void);

/* Read a single key and return it (for menu choice) */
static char menu_get_choice(void)
{
    char c;
    while (1) {
        c = os_getkey();
        if (c) break;
        os_yield();
    }
    return c;
}

/* Main entry: clean app UI - clear on each view, header at top */
void interactive_app_suite_menu(void)
{
    for (;;) {
        os_clear_screen();
        os_puts("=== APPS ===\n");
        os_puts("  1 - Calculator\n");
        os_puts("  2 - Tic-Tac-Toe (2 players)\n");
        os_puts("  3 - Timer (blinks when done; stoppable in Timer app)\n");
        os_puts("  4 - Notes (save/load to disk)\n");

        char choice = menu_get_choice();
        os_putchar(choice);
        os_puts("\n");

        if (choice == '1') {
            os_clear_screen();
            os_puts("=== Calculator ===  Type 'number op number' (e.g. 10 + 5). Esc = quit.\n\n");
            run_calculator();
        } else if (choice == '2') {
            os_clear_screen();
            os_puts("=== Tic-Tac-Toe ===  Positions 1-9. Esc = quit to menu.\n\n");
            run_tictactoe();
        } else if (choice == '3') {
            os_clear_screen();
            os_puts("=== Timer ===  Set timer (seconds); screen blinks when done. 3 or Esc = back.\n\n");
            run_timer();
        } else if (choice == '4') {
            os_clear_screen();
            os_puts("=== Notes ===  Enter = save, Esc = exit.\n\n");
            run_notes();
        } else {
            os_puts("Invalid choice. Enter 1-4.\n");
        }
    }
}
