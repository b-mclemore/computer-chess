#define _CRT_SECURE_NO_WARNINGS
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include "chess.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/timeb.h>
#include <stdlib.h>

/*
  _______________________________________
 /                                       \
/   _   _   _                 _   _   _   \
|  | |_| |_| |   _   _   _   | |_| |_| |  |
|   \   _   /   | |_| |_| |   \   _   /   |
|    | | | |     \       /     | | | |    |
|    | |_| |______|     |______| |_| |    |
|    |              ___              |    |
|    |  _    _    (     )    _    _  |    |
|    | | |  |_|  (       )  |_|  | | |    |
|    | |_|       |       |       |_| |    |
|   /            |_______|            \   |
|  |___________________________________|  |
\             Computer Chess              /
 \_______________________________________/

===========================================
-------------------------------------------
           		MAIN FILE
-------------------------------------------
===========================================

*/

// Main driver code - initializes board and runs parser
// The game memory also lives here as a game_state struct
int main() {
    // Init game memory
    game_state *gs = MALLOC(1, game_state);
    // Init lastmove
    last_move *lm = MALLOC(1, last_move);
    // Init movelist (for mate)
    moves *ms = MALLOC(1, moves);
    // Use unicode characters to print
    int do_unicode = 1;
    lm->dest_sq = -1;
    lm->orig_sq = -1;
    // Set up board
    init_board(gs);
    // Set up magic bitboards
    init_magic_bitboards();
    // Set up piece-square tables
    int mg_table[12][64];
    int eg_table[12][64];
    init_tables(mg_table, eg_table);
    print_board(gs, lm, do_unicode);
    printf("For all available commands, type '-help'\n");
    printf("To make a legal move, use long algebraic notation: ");
    printf("For example, e2e4 for the e4 opening.\n\n>");
    int flag;
    while ((flag = parse_input(gs, lm, mg_table, eg_table))) {
        // Reload board if input requires
        if (flag >= 1) {
            print_board(gs, lm, do_unicode);
            // Check whether game is over
            if (checkGameover(ms, gs)) {
                break;
            }
        } 
        if (flag == 2) {
            printf("\n");
            // Make computer move
            int start_time = get_time_ms();
            //int score;
            int best_move = iterativelyDeepen(gs, mg_table, eg_table, 1000);//findBestMove(gs, mg_table, eg_table, 7, &score);//
            int end_time = get_time_ms();
            makeMove(best_move, gs);
            // Add to highlight for previous move
            lm->orig_sq = decodeSource(best_move);
            lm->dest_sq = decodeDest(best_move);
            printf("Thought for %g seconds\n", ((float)end_time - (float)start_time)/1000);
            print_board(gs, lm, do_unicode);
            if (checkGameover(ms, gs)) {
                break;
            }
        }
        if (flag == 3) {
            while (1) {
                printf("\n");
                // Make computer move
                int start_time = get_time_ms();
                int best_move = iterativelyDeepen(gs, mg_table, eg_table, 1000);
                int end_time = get_time_ms();
                makeMove(best_move, gs);
                // Add to highlight for previous move
                lm->orig_sq = decodeSource(best_move);
                lm->dest_sq = decodeDest(best_move);
                printf("Thought for %g seconds\n", ((float)end_time - (float)start_time)/1000);
                print_board(gs, lm, do_unicode);
                fflush(stdout);
                if (checkGameover(ms, gs)) {
                    break;
                }
            }
        }
        printf("\n> ");
    }
    free(lm);
    free(ms);
    free(gs);
}