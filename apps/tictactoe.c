/*
 * =============================================================================
 * Interactive App Suite - Tic-Tac-Toe (2 players, ASCII)
 * =============================================================================
 * Two players take turns. Player 1 = X, Player 2 = O.
 * Enter position 1-9 to place your mark. Type Esc to quit.
 * =============================================================================
 */

#include "../kernel/os.h"

#define N 3
static char board[N][N];
static int current_player;
static int game_in_progress;   /* 0 = no game, 1 = game in progress (state saved when leaving) */

static void board_init(void)
{
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            board[i][j] = '1' + (char)(i * N + j);
}

static void board_draw(void)
{
    os_puts("\n");
    for (int i = 0; i < N; i++) {
        os_puts(" ");
        for (int j = 0; j < N; j++) {
            os_putchar(board[i][j]);
            if (j < N - 1) os_puts(" | ");
        }
        os_puts("\n");
        if (i < N - 1) os_puts("---+---+---\n");
    }
    os_puts("\n");
}

/* Check for win: 1 = P1 wins, 2 = P2 wins, 0 = no win */
static int check_win(void)
{
    char sym = (current_player == 1) ? 'X' : 'O';
    for (int i = 0; i < N; i++) {
        if (board[i][0] == sym && board[i][1] == sym && board[i][2] == sym) return current_player;
        if (board[0][i] == sym && board[1][i] == sym && board[2][i] == sym) return current_player;
    }
    if (board[0][0] == sym && board[1][1] == sym && board[2][2] == sym) return current_player;
    if (board[0][2] == sym && board[1][1] == sym && board[2][0] == sym) return current_player;
    return 0;
}

static int is_full(void)
{
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            if (board[i][j] >= '1' && board[i][j] <= '9') return 0;
    return 1;
}

/* Get move 1-9 from current player; return 0 if quit */
static int get_move(void)
{
    char line[16];
    for (;;) {
        if (current_player == 1)
            os_puts("Player 1 (X), enter position 1-9 (or Esc to quit): ");
        else
            os_puts("Player 2 (O), enter position 1-9 (or Esc to quit): ");
        os_readline(line, sizeof(line));
        if (line[0] == OS_KEY_ESC) return 0;
        if (line[0] >= '1' && line[0] <= '9') {
            int pos = line[0] - '0';
            int r = (pos - 1) / N;
            int c = (pos - 1) % N;
            if (board[r][c] >= '1' && board[r][c] <= '9')
                return pos;
        }
        os_puts("Invalid position. Use 1-9.\n");
    }
}

void run_tictactoe(void)
{
    if (!game_in_progress) {
        os_puts("Player 1 = X, Player 2 = O. Positions 1-9.\n");
        board_init();
        current_player = 1;
        game_in_progress = 1;
    } else {
        os_puts("(resumed)\n");
    }

    for (;;) {
        for (;;) {
            board_draw();
            int pos = get_move();
            if (pos == 0) {
                os_puts("Leaving Tic-Tac-Toe. Game state saved.\n");
                return;   /* game_in_progress stays 1 so we resume next time */
            }
            int r = (pos - 1) / N;
            int c = (pos - 1) % N;
            board[r][c] = (current_player == 1) ? 'X' : 'O';

            int win = check_win();
            if (win) {
                board_draw();
                if (win == 1) os_puts("Player 1 (X) wins!\n");
                else os_puts("Player 2 (O) wins!\n");
                break;
            }
            if (is_full()) {
                board_draw();
                os_puts("Draw!\n");
                break;
            }
            current_player = (current_player == 1) ? 2 : 1;
        }

        os_puts("Play again? (y/n, Esc = no): ");
        char line[8];
        os_readline(line, sizeof(line));
        if (line[0] == OS_KEY_ESC || (line[0] != 'y' && line[0] != 'Y')) {
            game_in_progress = 0;
            os_puts("Leaving Tic-Tac-Toe.\n");
            return;
        }
        board_init();
        current_player = 1;
    }
}
