/*
 * =============================================================================
 * Notes app - full note from disk, editable. Enter = save, Esc = exit.
 * Backspace deletes from note; typing adds. Entire saved content is loaded.
 * =============================================================================
 */

#include "../kernel/os.h"

#define NOTEBUF_SIZE 51200

static char notebuf[NOTEBUF_SIZE];
static uint32_t notelen;

static void notes_redraw(void)
{
    os_clear_screen();
    os_puts("Enter = save, Esc = exit. Backspace = delete from note.\n\n");
    if (notelen > 0) {
        uint32_t i = 0;
        int col = 0;
        while (i < notelen && col < 80 * 22) {
            char c = notebuf[i++];
            if (c == '\n') {
                os_putchar('\n');
                col = 0;
            } else {
                os_putchar(c);
                col++;
            }
        }
        if (i < notelen)
            os_puts("\n...\n");
        else
            os_putchar('\n');
    }
}

void run_notes(void)
{
    notelen = 0;
    notebuf[0] = '\0';
    int r = os_notes_load(notebuf, NOTEBUF_SIZE);
    if (r >= 0)
        notelen = (uint32_t)r;
    notebuf[notelen] = '\0';

    for (;;) {
        notes_redraw();
        os_puts("> ");
        for (;;) {
            char c = os_getkey();
            if (!c) {
                os_yield();
                continue;
            }
            if (c == OS_KEY_ESC) {
                return;
            }
            if (c == '\n' || c == '\r') {
                os_putchar('\n');
                if (os_notes_save(notebuf, notelen) == 0)
                    os_puts("Saved.\n");
                else
                    os_puts("Save failed (no disk?).\n");
                break;
            }
            if (c == '\b') {
                if (notelen > 0) {
                    notelen--;
                    notebuf[notelen] = '\0';
                    notes_redraw();
                    os_puts("> ");
                }
                continue;
            }
            if (c >= 32 && notelen + 2 < NOTEBUF_SIZE) {
                notebuf[notelen++] = c;
                notebuf[notelen] = '\0';
                os_putchar(c);
            }
        }
    }
}
