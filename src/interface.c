#include "chess.h"
#include "bitboards.c"
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
           COMMAND LINE INTERFACE
-------------------------------------------
===========================================

*/

// It will be useful to have a bitboard printer as a debugging tool, which we'll
// define here. It's pretty naive, just iterating through the bits and printing
// 1 or 0.
// We also want to color the board, which we do by printing the codes below

// Black background is dark green, white is tan
#define bbg "\x1b[42m"
#define wbg "\x1b[43m"
// White text and black text
#define wtxt "\x1b[97m"
#define btxt "\x1b[30m"
// Reset
#define reset_txt "\x1b[0m"
static void print_bitboard(U64 bitboard, int color) {
    char *txt = wtxt;
    if (color) txt = btxt;
    // start position is a 1 in the far left bit
    U64 start = (U64)1 << 63;
    // we choose to make rows the "slow" axis, columns the "fast" axis
    for (int row = 0; row < 8; row++) {
        // print num for row
        printf("%i  ", 8 - row);
        for (int col = 0; col < 8; col++) {
            // Set background color
            char *bg = wbg;
            if ((row + col) % 2) {
                bg = bbg;
            }
            printf("%s%s",bg,txt);
            start &bitboard ? printf("[]") : printf("  ");
            printf("%s", reset_txt);
            start >>= 1;
        }
        printf("\n");
    }
    printf("\n");
    // print letter for cols
    printf("   A B C D E F G H \n\n");
}

// Helper to print the extras (non-bitboard gamestate information)
void print_extras(game_state *gs) {
    if (gs->whose_turn) {
        printf("Black to play\n");
    } else {
        printf("White to play\n");
    }
    if (gs->castling[0]) {
        printf("White may castle kingside\n");
    }
    if (gs->castling[1]) {
        printf("White may castle queenside\n");
    }
    if (gs->castling[2]) {
        printf("Black may castle kingside\n");
    }
    if (gs->castling[3]) {
        printf("Black may castle queenside\n");
    }
    if (gs->en_passant > -1) {
        printf("The en-passant square is at %i\n", gs->en_passant);
    }
    printf("There have been %i halfmoves since the last pawn move or capture\n",
           gs->halfmove_counter);
    printf("There have been %i total moves this game\n", gs->moves);
}

/*

FEN is a useful "human-readable" notation for giving the engine a particular
position. For example, the initial position is given in the header as INIT_POS,
as well as an empty board and a position following white's failure to find the
"computer move" in the Ulvestad variation.

https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation

To map between bitboards and FEN, we simply parse through the FEN string and
update each bitboard, given a few rules about FEN strings. Once a space is
encountered, we parse the extras. In the header is also a string of the piece
characters, which we'll use to access the piece bitboards.

*/

char *castle_map = "KQkq";

// Parses only the string after the squares in a FEN string
// (Warning: very ugly)
static int parse_extras(game_state *gs, char *inp, int idx) {
    // First, copy the fen to a bigger buffer to avoid faults
    char fen[400];
    strcpy(fen, inp);
    // Next, our idx should move past the space
    idx++;
    // Find whose turn it is
    if (fen[idx] == 'w') {
        gs->whose_turn = 0;
    } else if (fen[idx] == 'b') {
        gs->whose_turn = 1;
    } else {
        return 1;
    }
    idx += 1;
    if (fen[idx] != ' ')
        return 1;
    idx += 1;
    // Now check for - or k/q chars
    if (fen[idx] == '-') {
        idx += 1;
        if (fen[idx] != ' ')
            return 1;
        idx += 1;
    } else if (fen[idx] == ' ') {
        return 1;
    } else {
        do {
            char *piece = strchr(castle_map, fen[idx]);
            if (piece) {
                // Locate idx in castlemap
                int index = (int)(piece - castle_map);
                gs->castling[index] = 1;
            } else if (fen[idx] == ' ') {
                idx += 1;
                break;
            } else {
                return 1;
            }
            idx += 1;
        } while (1);
    }
    // Now get en-passant square
    if (fen[idx] == '-') {
        idx += 1;
        if (fen[idx] != ' ')
            return 1;
        idx += 1;
    } else if (fen[idx] == ' ') {
        return 1;
    } else {
        if (!isdigit(fen[idx])) {
            return 1;
        }
        // Extract the number
        while (isdigit(fen[idx])) {
            gs->en_passant = (gs->en_passant * 10) + (fen[idx] - '0');
            idx++;
        }
        if (fen[idx] != ' ') {
            return 1;
        } else {
            idx++;
        }
    }
    // Now get halfmoves
    if (!isdigit(fen[idx])) {
        return 1;
    }
    // Extract the number
    while (isdigit(fen[idx])) {
        gs->halfmove_counter = (gs->halfmove_counter * 10) + (fen[idx] - '0');
        idx++;
    }
    if (fen[idx] != ' ') {
        return 1;
    } else {
        idx++;
    }
    // Now get full moves (turns)
    if (!isdigit(fen[idx])) {
        return 1;
    }
    // Extract the number
    while (isdigit(fen[idx])) {
        gs->moves = (gs->moves * 10) + (fen[idx] - '0');
        idx++;
    }
    if (idx != strlen(fen) - 1) {
        return 1;
    }
    return 0;
}

// Parses an entire FEN
int parse_fen(game_state *gs, char *fen) {
    // first, clear the bitboards
    // this means that trying parse_fen is NOT rewindable!
    clear_bitboards(gs);
    // start position is a 1 in the far left bit
    U64 pos = (U64)1 << 63;
    for (int idx = 0; idx < strlen(fen); idx++) {
        char ch = fen[idx];
        // if /, do nothing
        if (ch == '/') {
            continue;
        }
        // if number, move position by this many to the right
        else if ((0 < (ch - 48)) && ((ch - 48) <= 8)) {
            // make sure that we don't get multi-digit numbers
            if (idx != strlen(fen) - 1) {
                int num2 = fen[idx + 1] - 48;
                if ((0 < num2) && (num2 < 10)) {
                    return 1;
                }
            }
            // move by allowable num
            pos >>= (ch - 48);
            continue;
        }
        // if piece,find occurence of current character in piecemap and update
        char *piece = strchr(PIECE_MAP, ch);
        if (piece) {
            // Locate idx in piecemap
            int index = (int)(piece - PIECE_MAP);
            // Set position in piece, color, and overall bitboards
            gs->piece_bb[index] ^= pos;
            gs->color_bb[(index % 2)] ^= pos;
            gs->all_bb ^= pos;
            // if space, reached extras section. certain letters are reused, so
            // we need a new conditional branch
        } else if (ch == ' ') {
            return parse_extras(gs, fen, idx);
            // for any other character return 1 for error: bad FEN string
        } else {
            return 1;
        }
        pos >>= 1;
    }
    return 0;
}

/*

Finally, we introduce a "GUI" (sort of). Unicode has kindly given us every
chess piece, so we can use the command line to play, but Windows can't render
the black pawn piece correctly. Therefore (since I have a PC) we'll need to
paint the pieces black, and use a different square color. I've chosen green and
tan, to match the chess.com color scheme.

*/

// Most fonts have unicode pieces doublewide, so we add a space so that they
// don't get chopped in the command line
char *unicode_pieces[12] = {"♙ ", "♞ ", "♝ ", "♜ ", "♛ ", "♚ "};

void print_board(game_state *gs) {
    printf("\n");
    // start position is a 1 in the far left bit
    U64 pos = (U64)1 << 63;
    // we choose to make rows the "slow" axis, columns the "fast" axis
    for (int row = 0; row < 8; row++) {
        // print num for row
        printf("%i  ", 8 - row);
        for (int col = 0; col < 8; col++) {
            // Set background color
            char *bg = wbg;
            if ((row + col) % 2) {
                bg = bbg;
            }
            // if no piece, continue, else color the piece
            char *txt = wtxt;
            // If no piece, print double-wide space
            char *piece = "  ";
            if (pos & gs->all_bb) {
                if (pos & gs->color_bb[1]) {
                    txt = btxt;
                }
                // Find which piece
                for (int pc_idx = 0; pc_idx < 12; pc_idx++) {
                    if (pos & gs->piece_bb[pc_idx]) {
                        piece = unicode_pieces[pc_idx / 2];
                        break;
                    }
                }
            }
            // Formatting followed by piece
            printf("%s%s%s%s", txt, bg, piece, reset_txt);
            // Iterate to next bitboard bit
            pos >>= 1;
        }
        printf("\n");
    }
    printf("\n");
    // print letter for cols
    printf("   A B C D E F G H \n\n");
}

// It will also be useful to print all bitboards from the command line
void print_all_bitboards(game_state *gs) {
    for (int i = 0; i < 12; i++) {
        char *color = "white";
        if (i % 2) {
            color = "black";
        }
        printf("The %s %s:\n\n", color, unicode_pieces[i / 2]);
        print_bitboard(gs->piece_bb[i], i%2);
    }
    printf("All white pieces:\n\n");
    print_bitboard(gs->color_bb[0], 0);
    printf("All black pieces:\n\n");
    print_bitboard(gs->color_bb[1], 1);
    printf("All pieces:\n\n");
    print_bitboard(gs->all_bb, 0);
}

/*

In order to accept a move, we need to be able to read long algebraic notation (which matches
UCI standards). For example, e2e4 is the most common opening move.

*/

// Long algebraic notation parser

/*

In order to read a piece (if we want to view bitboards) we write a short helper function
here to match the piece.

*/

// Matches a piece to its //2 index in the gamestate bitboard list
int parse_piece(char piece) {
    if ((piece == 'p') || (piece == 'P')) return 0;
    else if ((piece == 'n') || (piece == 'N')) return 1;
    else if ((piece == 'b') || (piece == 'B')) return 2;
    else if ((piece == 'r') || (piece == 'R')) return 3;
    else if ((piece == 'q') || (piece == 'Q')) return 4;
    else if ((piece == 'k') || (piece == 'K')) return 5;
    else return -1;
}

// Matches a color to w->0, b->1
int parse_color(char color) {
    if ((color == 'w') || (color == 'W')) return 0;
    else if ((color == 'b') || (color == 'B')) return 1;
    else return -1;
}

/*
The main function: the input parser.
We return 0 for failure, -1 for no new board, and 1 for a new board.
*/

// max buffer size
#define INPUT_BUFFER 10000
int parse_input(game_state *gs) {
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
        printf("-setup [FEN]\t:\tstarts a new game from a given FEN "
               "string\n\t\t\t(WARNING: a malformed FEN will still restart the "
               "game)\n");
        printf("-quit\t\t:\tquits out of the program\n");
        printf("-help\t\t:\tprints this message\n");
        printf("\nDebugging flags:\n");
        printf("-cb\t\t:\tprints the current board\n");
        printf("-ab\t\t:\tprints all bitboards\n");
        printf("-ex\t\t:\tlists the extras: whose move, castling "
               "rights,\n\t\t\ten-passant square, and number of moves\n");
        printf("-moves\t\t:\tshow move bitboards for a piece, semi-algebraically.\n"
               "\t\t\t(WN for white knight, BR for black rook, etc)\n");
        return -1;
    }
    // print board for debugging
    else if (!strncmp(input, "-cb", 3)) {
        return 1;
    }
    // print all bitboards for debugging
    else if (!strncmp(input, "-ab", 3)) {
        print_all_bitboards(gs);
        return -1;
    }
    // print all extras for debugging
    else if (!strncmp(input, "-ex", 3)) {
        print_extras(gs);
        return -1;
    }
    // set up board
    else if (!strncmp(input, "-setup", 6)) {
        if (strlen(input) < 8) {
            // check to see if we can parse more
            printf("No FEN was given. Try '-setup 8/8/2k2q2/8/1R4K1/2RRRR2/8/8 "
                   "b - - 12 34'\n");
            init_board(gs);
        } else {
            char *fen = input + 7;
            if (parse_fen(gs, fen)) {
                printf("Not a valid FEN string\n");
                init_board(gs);
            }
        }
        return 1;
    }
    // show move bitboards
    else if (!strncmp(input, "-moves", 6)) {
        if (strlen(input) != 10) {
            // must be exactly two characters
            printf("Code must be two characters. Try WN for white knight or BR for black rook.\n");
        } else {
            int color = parse_color(input[7]);
            int piece = parse_piece(input[8]);
            if (piece == -1) {
                printf("Not a valid piece. Try WN for white knight or BR for black rook.\n");
            }
            if (color == -1) {
                printf("Not a valid color. Try WN for white knight or BR for black rook.\n");
            }
            U64 piece_bb = gs->piece_bb[2 * piece + color];
            if (piece == 0) {
                if (color) {
                    print_bitboard(bpPushes(piece_bb) | bpAttacks(piece_bb), color);
                } else {
                    print_bitboard(wpPushes(piece_bb) | wpAttacks(piece_bb), color);
                }
            } else if (piece == 1) {
                print_bitboard(knightAttacks(piece_bb), color);
            } else if (piece == 5) {
                print_bitboard(kingAttacks(piece_bb), color);
            } else {
                printf("We haven't implemented this piece's moves yet, sorry!\n");
            }
        }
        return -1;
    }
    // default
    else {
        printf("Not a valid command\n");
        return -1;
    }
    return 0;
}