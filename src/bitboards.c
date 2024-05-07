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

The struct for these bitboards lives in "game_state", which is defined in chess.h
We pass a pointer to a game_state to each of the following functions in order to pass
information about the bitboards, whose names appear below

U64 piece_bb[12]; 	(The pairs of boards for each piece)
U64 color_bb[2];	(The pair of color boards)
U64 all_bb;			(The board for all pieces)

Besides the bitboards, we also want to keep track of five more things:
- Whose turn it is
- Castling rights (kingside/queenside, per color)
- En passant captures (if the previous move was a 2-square pawn move, then the
intermediate square)
- The number of halfmoves (single player moves) since a capture or pawn advance,
for the 50 move rule
- The number of total moves

These all also appear in game_state as the following:
- whose_turn
- castling[4]
- en_passant
- halfmove_counter
- moves

*/

// Initialize board: these are the representations of the initial piece
// positions. The bitboard printer above can be used to check them out.
void init_board(game_state *gs) {
    // We define pieces bitboards in the following order:
    // white then black (so we can %2 later), in "point" order, bishops
    // considered higher points than knights pawns
    gs->piece_bb[0] = (U64)0b11111111 << 8;
    gs->piece_bb[1] = (U64)0b11111111 << 48;
    // knights
    gs->piece_bb[2] = (U64)0b01000010;
    gs->piece_bb[3] = (U64)0b01000010 << 56;
    // bishops
    gs->piece_bb[4] = (U64)0b00100100;
    gs->piece_bb[5] = (U64)0b00100100 << 56;
    // rooks
    gs->piece_bb[6] = (U64)0b10000001;
    gs->piece_bb[7] = (U64)0b10000001 << 56;
    // queens
    gs->piece_bb[8] = (U64)0b00010000;
    gs->piece_bb[9] = (U64)0b00010000 << 56;
    // kings
    gs->piece_bb[10] = (U64)0b00001000;
    gs->piece_bb[11] = (U64)0b00001000 << 56;

    // white pieces
    gs->color_bb[0] = (U64)0b1111111111111111;
    // black pieces
    gs->color_bb[1] = (U64)0b1111111111111111 << 48;

    // all pieces
    gs->all_bb = gs->color_bb[0] ^ gs->color_bb[1];

	// extras
	gs->whose_turn = 0;
	gs->en_passant = 0;
	gs->halfmove_counter = 0;
	gs->moves = 0;
	for (int i = 0; i < 4; i++) {
        gs->castling[i] = 1;
    }

}

// Set the bitboards to 0 before entering FEN information
static void clear_bitboards(game_state *gs) {
    for (int i = 0; i < 12; i++) {
        gs->piece_bb[i] = (U64)0;
    }
    gs->color_bb[0] = (U64)0;
    gs->color_bb[1] = (U64)0;
    gs->all_bb = (U64)0;
	gs->whose_turn = 0;
	gs->en_passant = 0;
	gs->halfmove_counter = 0;
	gs->moves = 0;
	for (int i = 0; i < 4; i++) {
        gs->castling[i] = 0;
    }
}