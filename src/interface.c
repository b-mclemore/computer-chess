#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "chess.h"

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
           COMMAND LINE INTERFACE
-------------------------------------------
===========================================

The main function: the input parser.
We return 0 for failure, -1 for no new board, and 1 for a new board.
*/

// max buffer size
#define INPUT_BUFFER 10000
int parse_input() {
    // reset buffers
    setvbuf(stdin, NULL, _IOFBF, BUFSIZ);
    setvbuf(stdout, NULL, _IOFBF, BUFSIZ);
    // define buffer
    char input[INPUT_BUFFER];
	// zero out
	memset(input, 0, sizeof(input));
	fflush(stdout);
	if (!fgets(input, INPUT_BUFFER, stdin)) {
        // failed to read
		printf("Failed to read input\n");
		return 0;
	}
	// quit program
    else if (!strncmp(input, "-quit", 4)) {
        printf("Quitting program...\n");
		return 0;
    }
	// print available commands
	else if (!strncmp(input, "-help", 4)) {
		printf("To make a legal move, use algebraic notation: ");
		printf("For example, Nf4\n");
		printf("\nUtilities:\n");
		printf("-setup [FEN]\t:\tstarts a new game from a given FEN string\n\t\t\t(WARNING: a malformed FEN will still restart the game)\n");
		printf("-quit\t\t:\tquits out of the program\n");
		printf("-help\t\t:\tprints this message\n");
		printf("\nDebugging flags:\n");
		printf("-cb\t\t:\tprints the current board\n");
		printf("-ab\t\t:\tprints all bitboards\n");
		printf("-ex\t\t:\tlists the extras: whose move, castling rights,\n\t\t\ten-passant square, and number of moves\n");
		printf("-moves\t\t:\tlists possible moves, ordered by evaluation (unimplemented)\n");
		return -1;
	}
    // print board for debugging
    else if (!strncmp(input, "-cb", 3)) {
		return 1;
	}
	// print all bitboards for debugging
	else if (!strncmp(input, "-ab", 3)) {
		print_all_bitboards();
		return -1;
	}
	// print all extras for debugging
	else if (!strncmp(input, "-ex", 3)) {
		print_extras();
		return -1;
	}
	// set up board
	else if (!strncmp(input, "-setup", 6)) {
		if (strlen(input) < 8) {
			// check to see if we can parse more
			printf("No FEN was given. Try '-setup 8/8/2k2q2/8/1R4K1/2RRRR2/8/8 w - - 0 1'\n");
			init_board();
		} else {
			char *fen = input + 7;
			if (parse_fen(fen)) {
				printf("Not a valid FEN string\n");
				init_board();
			}
		}
		return 1;
	}
	// default
    else {
		printf("Not a valid command\n");
		return -1;
	}
	return 0;
}