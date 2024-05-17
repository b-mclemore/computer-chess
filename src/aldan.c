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

const char *boardStringMap[64] = {
    "h1", "g1", "f1", "e1", "d1", "c1", "b1", "a1",
    "h2", "g2", "f2", "e2", "d2", "c2", "b2", "a2",
    "h3", "g3", "f3", "e3", "d3", "c3", "b3", "a3",
    "h4", "g4", "f4", "e4", "d4", "c4", "b4", "a4",
    "h5", "g5", "f5", "e5", "d5", "c5", "b5", "a5",
    "h6", "g6", "f6", "e6", "d6", "c6", "b6", "a6",
    "h7", "g7", "f7", "e7", "d7", "c7", "b7", "a7",
    "h8", "g8", "f8", "e8", "d8", "c8", "b8", "a8",
};
// For taking an index (piece enum) and getting a piece
const char *pieceStringMap[6] = {
    "p", "n", "b", "r", "q", "k"
};

// Main driver code - initializes board and runs parser
// The game memory also lives here as a game_state struct
int main() {
    // Init game memory
    game_state *gs = MALLOC(1, game_state);
    // Init lastmove
    last_move *lm = MALLOC(1, last_move);
    // Init movelist (for mate)
    moves *ms = MALLOC(1, moves);
    lm->dest_sq = -1;
    lm->orig_sq = -1;
    // Set up board
    init_board(gs);
    print_board(gs, lm);
    printf("For all available commands, type '-help'\n");
    printf("To make a legal move, use long algebraic notation: ");
    printf("For example, e2e4 for the e4 opening.\n\n>");
    int flag;
    generateLegalMoves(ms, gs);
    while ((flag = parse_input(gs, lm))) {
        // Reload board if input requires
        if (flag >= 1) {
            print_board(gs, lm);
            // Check whether game is over
            if (checkGameover(ms, gs)) {
                break;
            }
        } 
        if (flag == 2) {
            printf("\n");
            // Make computer move
            int start_time = get_time_ms();
            int best_move = iterativelyDeepen(gs, 1000);
            int end_time = get_time_ms();
            makeMove(best_move, gs);
            // Add to highlight for previous move
            lm->orig_sq = decodeSource(best_move);
            lm->dest_sq = decodeDest(best_move);
            printf("Thought for %g seconds\n", ((float)end_time - (float)start_time)/1000);
            print_board(gs, lm);
            if (checkGameover(ms, gs)) {
                break;
            }
        }
        if (flag == 3) {
            while (1) {
                printf("\n");
                // Make computer move
                int start_time = get_time_ms();
                int best_move = iterativelyDeepen(gs, 1000);
                int end_time = get_time_ms();
                makeMove(best_move, gs);
                // Add to highlight for previous move
                lm->orig_sq = decodeSource(best_move);
                lm->dest_sq = decodeDest(best_move);
                printf("Thought for %g seconds\n", ((float)end_time - (float)start_time)/1000);
                print_board(gs, lm);
                print_extras(gs);
                generateLegalMoves(ms, gs);
                printf("Legal moves:\n");
                printMoves(ms);
                print_board(gs, lm);
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