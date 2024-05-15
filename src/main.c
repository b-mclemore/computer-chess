#include "chess.h"
#include "interface.c"
#include <stdlib.h>

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
        if (flag == 1) {
            print_board(gs, lm);
            generateLegalMoves(ms, gs);
            // If there are no legal moves, the game is over
            if (ms->count == 0) {
                printf("Game over! ");
                // Flip turn to make the check function look at the next player's king
                gs->whose_turn = 1 - gs->whose_turn;
                if (checkCheck(gs)) {
                    printf("%s has been checkmated.\n\n", 1 - gs->whose_turn ? "Black" : "White");
                } else {
                    printf("The game is a stalemate.\n\n");
                }
                break;
            }
        }
        printf("\n> ");
    }
    free(gs);
    free(lm);
    free(ms);
}