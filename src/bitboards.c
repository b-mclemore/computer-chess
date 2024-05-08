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

/*

Next, we want to use bitboard logic to reason about the current game, for example knight or rook
moves, as well as more complex structures like passed pawns or a protected king.

The set of legal moves is required to play the game, while the complex structures are needed to
evaluate a position: for example, if white is down a pawn but has a passed pawn, this is a better
position than without a passed pawn.

We'll want these functions to run extremely quick, since when we evaluate a position we need to
look multiple moves in the future, so the number of times we use the reasoning functions will
grow exponentially. Where possible, I've tried to use vector intrinsics (SIMD functions) to
speed up the program.

The logically most simple move (though not necessarily for a human) is the knight moves, which
we calculate here. We take in the knight bitboard and return all possible (though not necessarily
legal) knight moves. You'll notice that we need to define a few consts here to assis with the
computation. For example, when we shift to the left before moving down to look at a south-west
knight move, we need to make sure we haven't rolled over to the next row.

*/

C64 notA = 0xfefefefefefefefe;
C64 notAB = 0xfcfcfcfcfcfcfcfc;
C64 notH = 0x7f7f7f7f7f7f7f7f;
C64 notGH = 0x3f3f3f3f3f3f3f3f;

// Takes knight bb and returns knight attacks
U64 knightAttacks(U64 knight_bb) {
    // (move once L/R and twice U/D, or twice L/R once U/D)
    // generate L/R once and L/R twice
    U64 l1 = (knight_bb << 1) & notA;
    U64 l2 = (knight_bb << 2) & notAB;
    U64 r1 = (knight_bb >> 1) & notH;
    U64 r2 = (knight_bb >> 2) & notGH;
    // pack together and move once or twice U/D
    U64 h1 = l1 | r1;
    U64 h2 = l2 | r2;
    return (h1 >> 16) | (h2 >> 8) | (h1 << 16) | (h2 << 8);
}

/* 

Next, we add pawn moves, which are almost as simple, except for the case that a pawn can move twice
from its opening square. Pawn attacks have two branches: the left and right attacks. Since we assume
that black is always on top (opposite white), we need to have separate calculators for black and white.

*/

U64 wpLeftAttacks(U64 pawn_bb) {
    return (pawn_bb << 9) & notA;
}

U64 wpRightAttacks(U64 pawn_bb) {
    return (pawn_bb << 7) & notH;
}

U64 wpAttacks(U64 pawn_bb) {
    return (wpLeftAttacks(pawn_bb) | wpRightAttacks(pawn_bb));
}

U64 bpLeftAttacks(U64 pawn_bb) {
    return (pawn_bb >> 7) & notA;
}

U64 bpRightAttacks(U64 pawn_bb) {
    return (pawn_bb >> 9) & notH;
}

U64 bpAttacks(U64 pawn_bb) {
    return (bpLeftAttacks(pawn_bb) | bpRightAttacks(pawn_bb));
}

U64 wpSinglePushes(U64 pawn_bb) {
    return (pawn_bb << 8);
}

U64 wpDoublePushes(U64 pawn_bb) {
    // Needs to originate from 2nd rank
    C64 rank2 = (C64)0x000000000000FF00;
    return ((pawn_bb & rank2) << 16);
}

U64 wpPushes(U64 pawn_bb) {
    return (wpSinglePushes(pawn_bb) | wpDoublePushes(pawn_bb));
}

U64 bpSinglePushes(U64 pawn_bb) {
    return (pawn_bb >> 8);
}

U64 bpDoublePushes(U64 pawn_bb) {
    // Needs to originate from 7th rank
    C64 rank7 = (C64)0x00FF000000000000;
    return ((pawn_bb & rank7) >> 16);
}

U64 bpPushes(U64 pawn_bb) {
    return (bpSinglePushes(pawn_bb) | bpDoublePushes(pawn_bb));
}

/*

Now, king moves: for now we'll ignore checks and just generate all moves for the king, 
which are easy enough to calculate. We simply move once in every lateral/diagonal direction
and prevent wrap-around

*/

// King moves: just once in any direction
U64 kingAttacks(U64 king_bb) {
    // The forward and backward moves are the same as single pawn pushes and diagonal attacks
    U64 back = (bpSinglePushes(king_bb) | bpAttacks(king_bb));
    U64 forw = (wpSinglePushes(king_bb) | wpAttacks(king_bb));
    U64 left = ((king_bb & notA) << 1);
    U64 rght = ((king_bb & notH) >> 1);
    return (back | forw | left | rght);
}

/*

More difficult are the sliding pieces: the bishops, rooks, and queens. Luckily, a queen is simply a
"bishop-rook". That is, its moves are the union of moves a rook and bishop could make from the same
square, which simplifies things. We split the functions into eight directions: four lateral, and
four diagonal.

*/

/*

The real difficulty in moving sliding pieces is their obstructions. To find legal moves for the non-
sliders, we simply bitwise & the opposite color piece boards to find captures, but here we need
to halt a sliding piece when it captures; no hopping is allowed.

To do so, we need to use some bit tricks. 

*/