#define _CRT_SECURE_NO_WARNINGS
#include "chess.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
              UCI COMPLIANCE
-------------------------------------------
===========================================


In order to allow the engine to play other engines, we need to make it conform
to Universal Chess Interface (UCI) standards, a writeup of which can be found
at:

https://gist.github.com/DOBRO/2592c6dad754ba67e6dcaec8c90165bf

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

/*

We need to define two helper functions. The first is a position parser. By UCI
standards, if we get the string:

position [fen <fenstring> | startpos ] moves <move1> .... <movei>

We need to read past "position", then either set up a position or initialize the
board as normal, then do the moves listed

*/

void parse_position(char *pos, game_state *gs) {
    // First, read to 'position '
    pos += strlen("position ");
    if (!(strncmp("fen", pos, 3))) {
        // Parse position
        if (parse_fen(gs, pos)) {
            // Couldn't parse FEN, just exit (bad behavior)
            exit(1);
        }
    } else {
        // Use initial position
        init_board(gs);
    }
    // Now, read in moves (taken from VICE)
    char *current_char = strstr(pos, "moves");
	// Unneeded here, used for command-line printing, but necessary for
	// parsing move
	last_move temp;
	char buffer[6];
    // moves available
    if (current_char != NULL) {
        // shift pointer to the right where next token begins
        current_char += 6;
        // loop over moves within a move string
        while(*current_char) {
			// Check whether moves exist
			if (strlen(current_char) < 4) {
				break;
			}
            // parse next move
			strncpy(buffer, current_char, 5);
            if (parse_move(buffer, gs, &temp) == -1) {
				// if no more moves
				break;
			}
            // move current character mointer to the end of current move
            while (*current_char && *current_char != ' ') current_char++;
            // go to the next move
            current_char++;
        }  
	} 
}

/*

The second is "go", which by UCI standards can be followed by many flags, all
listed below. 

*/

void parse_go(char *go, game_state *gs) {
    // No flags implemented yet
	go[0] = ' ';// <- Prevent unused warning
    int best_move = iterativelyDeepen(gs, 1000);
    square source_sq = decodeSource(best_move);
    square dest_sq = decodeDest(best_move);
	piece promoteTo = decodePromote(best_move);
    printf("bestmove %s", boardStringMap[source_sq]);
	printf("%s", boardStringMap[dest_sq]);
	if (promoteTo != pawn) {
		printf("%s", pieceStringMap[promoteTo]);
	}
    printf("\n");
}

// Print move: takes a move and prints as long-algebraic notation. Used for
// taking the engine's choice of move and outputting to GUI
void print_move(int move) {
    if (decodeCapture(move)) {
        printf("%s%s%s\n", boardStringMap[decodeSource(move)],
                           boardStringMap[decodeDest(move)],
                           pieceStringMap[decodePromote(move)]);
    }
    else {
        printf("%s%s\n", boardStringMap[decodeSource(move)],
                           boardStringMap[decodeDest(move)]);
    }
}

// main UCI loop
// Technically, by UCI standards we should ignore garbage preceding a command
// and ignore any unnecessary whitespace, but we'll assume that commands are
// always well-formed for now
#define INPUT_BUFFER 10000
void uci_loop(game_state *gs) {
    // reset buffers
    setvbuf(stdin, NULL, _IOFBF, BUFSIZ);
    setvbuf(stdout, NULL, _IOFBF, BUFSIZ);
    char input[2000];

    // GUI always begins with "uci" command, so print id & uciok
    // after Понедельник начинается в субботу
    printf("id name Алдан-3\n");
    printf("id name Ben McLemore\n");
    printf("uciok\n");

	// Set to initial board
	init_board(gs);

    // main loop : continue until broken
    while (1) {
        // reset input
        memset(input, 0, sizeof(input));
        fflush(stdout);
        // get user / GUI input
        if (!fgets(input, 2000, stdin))
            // failure to read
            continue;

        // no input
        if (input[0] == '\n')
            continue;

        // isready - used to synchronize engine w/ GUI.
        // used either as a heartbeat check or after the GUI sends
        // time-intensive commands always requires response "readyok"
        if (strncmp(input, "isready", 7) == 0) {
            printf("readyok\n");
            continue;
        }

        // register - used to register an engine or to tell the engine that
        // registration will be done later.
        // for now, always returns "later"
        else if (strncmp(input, "register", 8) == 0) {
            printf("later\n");
			continue;
		}

        // ucinewgame - set up new game board
        else if (strncmp(input, "ucinewgame", 10) == 0) {
            init_board(gs);
			continue;
		}

        // position - see above, sets up a position before evaluating
        else if (strncmp(input, "position", 8) == 0) {
            parse_position(input, gs);
			continue;
		}

        // go - see above, begins evaluation based on given flags
        else if (strncmp(input, "go", 2) == 0) {
            parse_go(input, gs);
			continue;
		}

        // quit - exit as soon as possible
        else if (strncmp(input, "quit", 4) == 0)
            break;

        // parse UCI "uci" command
        else if (strncmp(input, "uci", 3) == 0) {
            // print engine info
            printf("id name Алдан-3\n");
            printf("id name Ben McLemore\n");
            printf("uciok\n");
			continue;
        }

		// debugging: print board
		else if (strncmp(input, "debug", 5) == 0) {
			last_move temp;
			temp.dest_sq = 0;
			temp.orig_sq = 0;
            print_board(gs, &temp);
			for (int i = 0; i < 64; i++) {
				printf("%s ", boardStringMap[i]);
			}
			printf("\n");
			continue;
		}
    }
}

// Main driver code - initializes board and runs uci parser
// The game memory also lives here as a game_state struct
int main() {
    // Init game memory
    game_state *gs = MALLOC(1, game_state);
    uci_loop(gs);
	free(gs);
}