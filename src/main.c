#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include "chess.h"  

// Main driver code - initializes board and runs parser
// The game memory also lives here as a game_state struct
// TODO: figure out how to compile separate command-line and UCI executables
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
            int best_move = iterativelyDeepen(gs, 6);
            makeMove(best_move, gs);
            // Add to highlight for previous move
            lm->orig_sq = decodeSource(best_move);
            lm->dest_sq = decodeDest(best_move);
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
                fflush(stdout);
                if (checkGameover(ms, gs)) {
                    break;
                }
            }
        }
        printf("\n> ");
    }
    free(gs);
    free(lm);
    free(ms);
}