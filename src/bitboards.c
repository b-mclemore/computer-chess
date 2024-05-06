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
\       	  Computer Chess	          /
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

Computer chess hobbyists like to define such a 64-bit board as a U64, and we need 12 piece
bitboards: a pair for pawns, knights, bishops, rooks, queens, and kings. We'll also want
2 color bitboards, and a bitboard to track every piece.
*/

#define U64 unsigned long long
U64 piece_bb[12];
U64 color_bb[2];
U64 all_bb;

// It will be useful to have a bitboard printer as a debugging tool, which we'll define here.
// It's pretty naive, just iterating through the bits and printing 1 or 0.

void print_bitboard(U64 bitboard) {
	// start position is a 1 in the far left bit
	U64 start = (U64)1 << 63;
	// we choose to make rows the "slow" axis, columns the "fast" axis
	for (int row = 0; row < 8; row++) {
		// print num for row
		printf("%i  ", row + 1);
		for (int col = 0; col < 8; col++) {
			start & bitboard ? printf("[]") : printf("  ");
			start >>= 1;
		}
		printf("\n");
	}
	printf("\n");
	// print letter for cols
	printf("   A B C D E F G H \n");
}

// Initialize board: these are the magic hexadecimal representations of the initial
// piece positions. The bitboard printer above can be used to check them out.


/*

FEN is a useful "human-readable" notation for giving the engine a particular position.
For example, the initial position is given below, as well as an empty board and a
position following white's failure to find the "computer move" in the Ulvestad variation.

https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation

*/

#define INIT_POS "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define EMPTY_POS "8/8/8/8/8/8/8/8 b - - "
#define TEST_POS "r1b1kb1r/p1p2ppp/2n2n2/1B2p1N1/2P5/8/PP1P1PqP/RNBQK2R w KQkq - 0 1"

/*

To map between bitboards and FEN, we ...

*/

/*

Finally, we introduce a "GUI" (not really). Unicode has kindly given us every chess piece,
so we can use the command line to play, but Windows can't render the black pawn piece correctly.
Therefore we'll need to paint the white pieces black, and use a different square color. I've chosen
green and tan, to match the chess.com color scheme.

*/

// Black background is dark green, white is tan
#define bbg "\x1b[42m"
#define wbg "\x1b[43m"
// White text and black text
#define wtxt "\x1b[97m"
#define btxt "\x1b[30m"
// Reset
#define reset_txt "\x1b[0m"

// Most fonts have unicode pieces doublewide, so we add a space so that they don't get chopped in the command line
char *unicode_pieces[12] = {"♙ ", "♘ ", "♗ ", "♖ ", "♕ ", "♔ ","♙ ", "♘ ", "♗ ", "♖ ", "♕ ", "♔ "};

void print_board() {
	// start position is a 1 in the far left bit
	U64 start = (U64)1 << 63;
	// we choose to make rows the "slow" axis, columns the "fast" axis
	for (int row = 0; row < 8; row++) {
		// print num for row
		printf("%i  ", row + 1);
		for (int col = 0; col < 8; col++) {
			// For now, just simply print white pieces in top, black in bottom,
			// pieces chosen arbitrarily
			char *txt = wtxt;
			char *bg = wbg;
			if ((row + col) % 2) {
				bg = bbg;
			}
			if (row > 3) {
				txt = btxt;
			}
			// Formatting
			printf("%s", bg);
			printf("%s", txt);
			// Piece
			printf("%s", unicode_pieces[(row + col) % 12]);
			// Reset
			printf(reset_txt);
			start >>= 1;
		}
		printf("\n");
	}
	printf("\n");
	// print letter for cols
	printf("   A B C D E F G H \n");
}

int main() { 
	// test printing board
	print_board();
 }