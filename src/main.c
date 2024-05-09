#include "chess.h"
#include "interface.c"
#include <stdlib.h>

// Main driver code - initializes board and runs parser
// The game memory also lives here as a game_state struct
// TODO: figure out how to compile separate command-line and UCI executables
int main() {
    // Init game memory
    game_state *gs = MALLOC(1, game_state);
    // Set up board
    init_board(gs);
    print_board(gs);
    printf("For all available commands, type '-help'\n");
    printf("To make a legal move, use long algebraic notation: ");
    printf("For example, e2e4 for the e4 opening.\n\n>");
    int flag;
    while ((flag = parse_input(gs))) {
        if (flag == 1)
            print_board(gs);
        printf("\n> ");
    }
    free(gs);
}