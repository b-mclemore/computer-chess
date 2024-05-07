#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
                BITBOARDS
-------------------------------------------
===========================================

The 8x8 chess board can conveniently be expressed as a collection of 64-bit
"bitboards". For example, the "white pawn" bitboard initially looks like:

  8  0 0 0 0 0 0 0 0
  7  0 0 0 0 0 0 0 0
  6  0 0 0 0 0 0 0 0
  5  0 0 0 0 0 0 0 0
  4  0 0 0 0 0 0 0 0
  3  0 0 0 0 0 0 0 0
  2  1 1 1 1 1 1 1 1
  1  0 0 0 0 0 0 0 0

     a b c d e f g h

Computer chess hobbyists like to define such a 64-bit board as a U64, and we
need 12 piece bitboards: a pair for pawns, knights, bishops, rooks, queens, and
kings. We'll also want 2 color bitboards, and a bitboard to track every piece.
*/

#define U64 unsigned long long
U64 piece_bb[12];
U64 color_bb[2];
U64 all_bb;

// It will be useful to have a bitboard printer as a debugging tool, which we'll
// define here. It's pretty naive, just iterating through the bits and printing
// 1 or 0.

void print_bitboard(U64 bitboard) {
    // start position is a 1 in the far left bit
    U64 start = (U64)1 << 63;
    // we choose to make rows the "slow" axis, columns the "fast" axis
    for (int row = 0; row < 8; row++) {
        // print num for row
        printf("%i  ", 8 - row);
        for (int col = 0; col < 8; col++) {
            start &bitboard ? printf("[]") : printf("  ");
            start >>= 1;
        }
        printf("\n");
    }
    printf("\n");
    // print letter for cols
    printf("   A B C D E F G H \n\n");
}

// Initialize board: these are the representations of the initial piece
// positions. The bitboard printer above can be used to check them out.
void init_board() {
    // We define pieces bitboards in the following order:
    // white then black (so we can %2 later), in "point" order, bishops
    // considered higher points than knights pawns
    piece_bb[0] = (U64)0b11111111 << 8;
    piece_bb[1] = (U64)0b11111111 << 48;
    // knights
    piece_bb[2] = (U64)0b01000010;
    piece_bb[3] = (U64)0b01000010 << 56;
    // bishops
    piece_bb[4] = (U64)0b00100100;
    piece_bb[5] = (U64)0b00100100 << 56;
    // rooks
    piece_bb[6] = (U64)0b10000001;
    piece_bb[7] = (U64)0b10000001 << 56;
    // queens
    piece_bb[8] = (U64)0b00010000;
    piece_bb[9] = (U64)0b00010000 << 56;
    // kings
    piece_bb[10] = (U64)0b00001000;
    piece_bb[11] = (U64)0b00001000 << 56;

    // white pieces
    color_bb[0] = (U64)0b1111111111111111;
    // black pieces
    color_bb[1] = (U64)0b1111111111111111 << 48;

    // all pieces
    all_bb = color_bb[0] ^ color_bb[1];
}

/*

FEN is a useful "human-readable" notation for giving the engine a particular
position. For example, the initial position is given below, as well as an empty
board and a position following white's failure to find the "computer move" in
the Ulvestad variation.

https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation

*/

#define INIT_POS "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define EMPTY_POS "8/8/8/8/8/8/8/8 b - - "
#define TEST_POS                                                               \
    "r1b1kb1r/p1p2ppp/2n2n2/1B2p1N1/2P5/8/PP1P1PqP/RNBQK2R w KQkq - 0 1"

/*

To map between bitboards and FEN, we simply parse through the FEN string and
update each bitboard, given a few rules about FEN strings. Once a space is
encountered, we end the parsing. Below is also a string of the piece characters,
which we'll use to access the piece bitboards.

Later, we'll want to implement castling and turn bitboards, so we'll need to come back
and add more functionality to the parser.

*/

#define PIECE_MAP "PpNnBbRrQqKk"

void clear_bitboards() {
	for (int i = 0; i < 12; i++) {
		piece_bb[i] = (U64)0;
	}
	color_bb[0] = (U64)0;
	color_bb[1] = (U64)0;
	all_bb = (U64)0;
}

int parse_fen(char *fen) {
	// first, clear the bitboards
	clear_bitboards();
	// start position is a 1 in the far left bit
    U64 pos = (U64)1 << 63;
	for (int idx = 0; idx < strlen(fen); idx++) {
		char ch = fen[idx];
		// if /, do nothing
		if (ch == '/') {
			continue;
		}
		// if number, move position by this many to the right
		int num = ch - 48;
		if ((0 < num) && (num <= 8)) {
			// make sure that we don't get multi-digit numbers
			if (idx != strlen(fen) - 1) {
				int num2 = fen[idx + 1] - 48;
				if ((0 < num2) && (num2 < 10)) {
					return 1;
				}
			}
			// move by allowable num
			pos >>= num;
			continue;
		} else if ((num == 0) || (num == 9)) {
			// not allowable
			return 1;
		}
		// if piece, find occurence of current character in piecemap and update
		char *piece = strchr(PIECE_MAP, ch);
		if (piece) {
			// Locate idx in piecemap
			int index = (int)(piece - PIECE_MAP);
			// Set position in piece, color, and overall bitboards
			piece_bb[index] ^= pos;
			color_bb[(index % 2)] ^= pos;
			all_bb ^= pos;
		}
		// for any other character return 1 for error: bad FEN string
		else {
			return 1;
		}
		pos >>= 1;
	}
	// only a valid FEN if we iterated thru 64 squares, return 1 for bad FEN string
	return (!!pos);
}

/*

Finally, we introduce a "GUI" (sort of). Unicode has kindly given us every
chess piece, so we can use the command line to play, but Windows can't render
the black pawn piece correctly. Therefore (since I have a PC) we'll need to paint
the pieces black, and use a different square color. I've chosen green and
tan, to match the chess.com color scheme.

*/

// Black background is dark green, white is tan
#define bbg "\x1b[42m"
#define wbg "\x1b[43m"
// White text and black text
#define wtxt "\x1b[97m"
#define btxt "\x1b[30m"
// Reset
#define reset_txt "\x1b[0m"

// Most fonts have unicode pieces doublewide, so we add a space so that they
// don't get chopped in the command line
char *unicode_pieces[12] = {"♙ ", "♞ ", "♝ ", "♜ ", "♛ ", "♚ "};

void print_board() {
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
			char *txt;
			char *piece;
			if (pos & all_bb) {
				if (pos & color_bb[0]) {
					txt = wtxt;
				} else if (pos & color_bb[1]) {
					txt = btxt;
				}
				// Find which piece
				for (int pc_idx = 0; pc_idx < 12; pc_idx++) {
					if (pos & piece_bb[pc_idx]) {
						piece = unicode_pieces[pc_idx / 2];
						break;
					}
				}
			} else {
				// No piece, print double-wide space
				piece = "  ";
			}
            // Formatting followed by piece
            printf("%s%s%s%s",txt,bg,piece,reset_txt);
			// Iterate to next bitboard bit
            pos >>= 1;
        }
        printf("\n");
    }
    printf("\n");
    // print letter for cols
    printf("   A B C D E F G H \n\n");
}

int main() {
    init_board();
    // test all bitboards
    for (int i = 0; i < 12; i++) {
        char *color = "white";
        if (i % 2) {
            color = "black";
        }
        printf("Testing the %s %s:\n\n", color, unicode_pieces[i / 2]);
        print_bitboard(piece_bb[i]);
    }
    printf("Testing white pieces:\n\n");
    print_bitboard(color_bb[0]);
    printf("Testing black pieces:\n\n");
    print_bitboard(color_bb[1]);
    printf("Testing all pieces:\n\n");
    print_bitboard(all_bb);
    // test printing board
    print_board();
	// try out test boards
	parse_fen(INIT_POS);
	print_board();
	parse_fen(TEST_POS);
	print_board();
}