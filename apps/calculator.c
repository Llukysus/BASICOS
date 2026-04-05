/*
 * =============================================================================
 * Interactive App Suite - Command-line Calculator
 * =============================================================================
 * Supports addition, subtraction, multiplication, division.
 * Input format: number op number (e.g. 10 + 5, 20 * 3)
 * Type Esc to quit and return to the menu.
 * =============================================================================
 */

#include "../kernel/os.h"

static void put_num(long n)
{
    char buf[16];
    int i = 0;
    int neg = 0;
    if (n < 0) { neg = 1; n = -n; }
    if (n == 0) { os_putchar('0'); return; }
    while (n > 0) {
        buf[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    if (neg) os_putchar('-');
    while (i > 0) os_putchar(buf[--i]);
}

/* Parse decimal number from *s; advance *s past the number */
static long parse_num(const char **s)
{
    const char *p = *s;
    while (*p == ' ') p++;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    long n = 0;
    while (*p >= '0' && *p <= '9')
        n = n * 10 + (*p++ - '0');
    *s = p;
    return neg ? -n : n;
}

/* Skip spaces */
static void skip_spaces(const char **s)
{
    while (**s == ' ') (*s)++;
}

void run_calculator(void)
{
    char line[128];

    for (;;) {
        os_puts("> ");
        os_readline(line, sizeof(line));

        if (line[0] == OS_KEY_ESC) {
            os_puts("Leaving calculator.\n");
            return;
        }

        const char *p = line;
        long a = parse_num(&p);
        skip_spaces(&p);
        char op = *p;
        if (op != '+' && op != '-' && op != '*' && op != '/') {
            os_puts("Usage: number op number\n");
            continue;
        }
        p++;
        skip_spaces(&p);
        long b = parse_num(&p);

        if (op == '/' && b == 0) {
            os_puts("Error: division by zero\n");
            continue;
        }

        long result;
        if (op == '+') result = a + b;
        else if (op == '-') result = a - b;
        else if (op == '*') result = a * b;
        else result = a / b;

        put_num(a);
        os_putchar(' ');
        os_putchar(op);
        os_putchar(' ');
        put_num(b);
        os_puts(" = ");
        put_num(result);
        os_puts("\n");
    }
}
