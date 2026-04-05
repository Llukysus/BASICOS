/*
 * =============================================================================
 * Timer app - set a timer in seconds; screen blinks white twice when it ends.
 * Uses kernel polling (no background task), so it never freezes the system.
 * =============================================================================
 */

#include "../kernel/os.h"

#define TICKS_PER_SEC 100
#define MIN_TICKS     100  /* minimum delay so timer never fires instantly */

void run_timer(void)
{
    if (!os_timer_available()) {
        os_puts("\n--- Timer ---\n");
        os_puts("Timer not available (time source disabled in this build).\n");
        os_puts("Press Enter to go back.\n");
        { char line[8]; os_readline(line, sizeof(line)); }
        return;
    }
    os_puts("Type a number and press Enter to start (e.g. 9).\n");
    os_puts("While running: Space = stop immediately. Esc = back.\n\n");

    char line[32];
    for (;;) {
        os_puts("> ");
        os_readline(line, sizeof(line));
        os_putchar('\n');

        if (line[0] == OS_KEY_ESC) {
            os_puts("Back to menu.\n");
            return;
        }

        /* Parse seconds from the typed line */
        uint32_t sec = 0;
        for (int i = 0; line[i] >= '0' && line[i] <= '9'; i++)
            sec = sec * 10 + (line[i] - '0');
        if (sec == 0) {
            os_puts("Enter seconds like: 9  (Esc to exit)\n");
            continue;
        }

        uint32_t ticks = sec * (uint32_t)TICKS_PER_SEC;
        if (ticks < MIN_TICKS) ticks = MIN_TICKS;
        os_timer_arm(ticks);

        os_puts("Timer started. Press Space to stop, Esc to exit.\n");

        /* Running loop: listen for Space/Esc without needing Enter */
        for (;;) {
            char c = os_getkey();
            if (c == ' ') {
                os_timer_disarm();
                os_puts("\nTimer stopped.\n");
                break;
            }
            if (c == OS_KEY_ESC) {
                os_puts("\nBack to menu.\n");
                return;
            }
        }
    }
}
