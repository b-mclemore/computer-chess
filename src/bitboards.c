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
We'll write h1 = 0 and a1 = 1 << 63, an enum of which can be seen in chess.h

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
	gs->moves = 1;
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

The real difficulty in moving sliding pieces is their obstructions. To find legal moves for the non-
sliders, we simply bitwise & the opposite color piece boards to find captures, but here we need
to halt a sliding piece when it captures; no hopping is allowed.

To do so, we use a "fill" algorithm. Essentially, we take two bitboards: the sliding piece bitboard
and the all colors bitboard. Then we implement a "fill" for each lateral/diagonal. For example,
for the backward fill, we shift >> 8 to move down a square, iterating until we hit another piece.
Then we shift once more to represent capturing this piece. We want to avoid branching, since these
will be called extremely frequently. 

For now, we use the "dumb7fill" method of moving one step at a time in the given direction, which
requires a bitboard of the empty squares (~ all_bb)

Importantly, this ignores whether we capture an opposite colored piece, but we will do a check later
to ensure that do not allow "friendly-fire"

*/

// Takes bitboard of lateral sliders and returns all backward (towards rank 1) rays
static U64 backRay(U64 lateralSliders_bb, U64 empt) {
    U64 ray = lateralSliders_bb;
    // Move up to six squares before encountering a piece
    ray |= lateralSliders_bb = (lateralSliders_bb >> 8) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb >> 8) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb >> 8) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb >> 8) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb >> 8) & empt;
    ray |= (lateralSliders_bb >> 8) & empt;
    // Where a piece was encountered, move another square
    return (ray >> 8);
}

// Takes bitboard of lateral sliders and returns all backward (towards rank 8) rays
static U64 forwRay(U64 lateralSliders_bb, U64 empt) {
    U64 ray = lateralSliders_bb;
    // Move up to six squares before encountering a piece
    ray |= lateralSliders_bb = (lateralSliders_bb << 8) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb << 8) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb << 8) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb << 8) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb << 8) & empt;
    ray |= (lateralSliders_bb << 8) & empt;
    // Where a piece was encountered, move another square
    return (ray << 8);
}

// Takes bitboard of lateral sliders and returns all left (towards A file) rays
static U64 leftRay(U64 lateralSliders_bb, U64 empt) {
    U64 ray = lateralSliders_bb;
    // We consider the H file to be filled to prevent overflow
    empt &= notH;
    // Move up to six squares before encountering a piece
    ray |= lateralSliders_bb = (lateralSliders_bb << 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb << 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb << 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb << 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb << 1) & empt;
    ray |= (lateralSliders_bb << 1) & empt;
    // Where a piece was encountered, move another square
    return (ray << 1);
}

// Takes bitboard of lateral sliders and returns all right (towards H file) rays
static U64 rghtRay(U64 lateralSliders_bb, U64 empt) {
    U64 ray = lateralSliders_bb;
    // We consider the A file to be filled to prevent overflow
    empt &= notA;
    // Move up to six squares before encountering a piece
    ray |= lateralSliders_bb = (lateralSliders_bb >> 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb >> 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb >> 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb >> 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb >> 1) & empt;
    ray |= (lateralSliders_bb >> 1) & empt;
    // Where a piece was encountered, move another square
    return (ray >> 1);
}

// Takes a bitboard of lateral sliders and returns all rays
U64 rookAttacks(U64 rook_bb, U64 empt) {
    U64 forw = forwRay(rook_bb, empt);
    U64 back = backRay(rook_bb, empt);
    U64 left = leftRay(rook_bb, empt);
    U64 rght = rghtRay(rook_bb, empt);
    return (forw | back | left | rght);
}

// Takes a bitboard of diagonal sliders and returns all northwest (towards A8) rays
static U64 nwRay(U64 diagonalSliders_bb, U64 empt) {
    U64 ray = diagonalSliders_bb;
    // We consider the A file to be filled to prevent overflow
    empt &= notA;
    // Move up to six squares before encountering a piece
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 7) & empt;
    ray |= (diagonalSliders_bb << 7) & empt;
    // Where a piece was encountered, move another square
    return (ray << 7);
}

// Takes a bitboard of diagonal sliders and returns all southwest (towards A1) rays
static U64 swRay(U64 diagonalSliders_bb, U64 empt) {
    U64 ray = diagonalSliders_bb;
    // We consider the A file to be filled to prevent overflow
    empt &= notA;
    // Move up to six squares before encountering a piece
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 9) & empt;
    ray |= (diagonalSliders_bb >> 9) & empt;
    // Where a piece was encountered, move another square
    return (ray >> 9);
}

// Takes a bitboard of diagonal sliders and returns all northeast (towards H8) rays
static U64 neRay(U64 diagonalSliders_bb, U64 empt) {
    U64 ray = diagonalSliders_bb;
    // We consider the H file to be filled to prevent overflow
    empt &= notH;
    // Move up to six squares before encountering a piece
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 9) & empt;
    ray |= (diagonalSliders_bb << 9) & empt;
    // Where a piece was encountered, move another square
    return (ray << 9);
}

// Takes a bitboard of diagonal sliders and returns all southeast (towards H1) rays
static U64 seRay(U64 diagonalSliders_bb, U64 empt) {
    U64 ray = diagonalSliders_bb;
    // We consider the H file to be filled to prevent overflow
    empt &= notH;
    // Move up to six squares before encountering a piece
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 7) & empt;
    ray |= (diagonalSliders_bb >> 7) & empt;
    // Where a piece was encountered, move another square
    return (ray >> 7);
}

// Takes a bitboard of diagonal sliders and returns all rays
U64 bishopAttacks(U64 bishop_bb, U64 empt) {
    U64 ne = neRay(bishop_bb, empt);
    U64 se = seRay(bishop_bb, empt);
    U64 nw = nwRay(bishop_bb, empt);
    U64 sw = swRay(bishop_bb, empt);
    return (ne | se | nw | sw);
}

// Takes a bitboard of lateral & diagonal sliders and returns all rays
U64 queenAttacks(U64 queen_bb, U64 empt) {
    return (bishopAttacks(queen_bb, empt) | rookAttacks(queen_bb, empt));
}

/*

Now we're ready to begin making moves, but there are a few caveats: checks, pins, and
castling. First, we'll define a function which makes a move based on whether it's a legal
move/capture (no "friendly fire"), and worry about the caveats in a bit

*/

// Helper to find which piece is on the current square
int findPiece(square piece_int, game_state *gs) {
    U64 pos = (U64)1 << piece_int;
    int color = gs->whose_turn;
    for (int pc_idx = 0; pc_idx < 6; pc_idx++) {
        if (pos & gs->piece_bb[2 * pc_idx + color]) {
            return pc_idx;
        }
    }
    return -1;
}

// Check that the move exists (matches piece and color)
int checkExists(int piece, square piece_int, square target_sq, game_state *gs) {
    int color = gs->whose_turn;
    U64 piece_sq = (U64)1 << piece_int;
    return !!(gs->piece_bb[2 * piece + color] & piece_sq);
}

// Make the move: always assumes that such a move is valid
void makeMove(square piece_int, square target_int, game_state *gs) {
    int piece = findPiece(piece_int, gs);
    U64 piece_sq = (U64)1 << piece_int;
    U64 target_sq = (U64)1 << target_int;
    int color = gs->whose_turn;
    // Move in pieceboard
    gs->piece_bb[2 * piece + color] &= (~ piece_sq);
    gs->piece_bb[2 * piece + color] |= target_sq;
    // Move in own color
    gs->color_bb[color] &= (~ piece_sq);
    gs->color_bb[color] |= target_sq;
    // Move in overall
    gs->all_bb &= (~ piece_sq);
    gs->all_bb |= target_sq;
    // Remove other color
    gs->color_bb[1 - color] &= (~ target_sq);
    // Remove from every pieceboard (kings should not be valid)
    gs->piece_bb[(0 * 2) + 1 - color] &= (~ target_sq);
    gs->piece_bb[(1 * 2) + 1 - color] &= (~ target_sq);
    gs->piece_bb[(2 * 2) + 1 - color] &= (~ target_sq);
    gs->piece_bb[(3 * 2) + 1 - color] &= (~ target_sq);
    gs->piece_bb[(4 * 2) + 1 - color] &= (~ target_sq);
}

/*

Now, the caveats: checks, pins and castling. 
Checks:
    If the king is in check, we remove the check: move the king or block the check. This
    kind of test will also be useful for checkmate, as if there are no legal moves while
    in check, we immediately end the game

Pins:
    If a move is pinned (that is, moving it would put the king in check), we do not permit
    the piece to move

Castling:
    If the king and the given rook (king- or queen-side) have not moved, and the king is not
    in check nor will be in check during any step of castling, the king may move two squares
    towards the king and the castle may move to the intermediate square which the king jumped
    over. This is a tough one!

*/

int checkLegality(square piece_int, square target_int, game_state *gs) {
    // First, make sure that piece exists
    int piece = findPiece(piece_int, gs);
    int color = gs->whose_turn;
    U64 piece_sq = (U64)1 << piece_int;
    U64 target_sq = (U64)1 << target_int;
    if (!checkExists(piece, piece_int, target_int, gs)) {
        return 0;
    }
    // Next, prevent friendly fire
    if (target_sq & gs->color_bb[color]) {
        return 0;
    }
    // Next, check for valid attack
    switch (piece) {
        case 0:
            if (target_sq & gs->color_bb[1 - color]) {
                if (color) { // Black pawn attack
                    if (!(target_sq & bpAttacks(piece_sq))) {
                        return 0;
                    }
                } else { // White pawn attack
                    if (!(target_sq & wpAttacks(piece_sq))) {
                        return 0;
                    }
                }
            } else {
                if (color) { // Black pawn push
                    if (!(target_sq & bpPushes(piece_sq))) {
                        return 0;
                    }
                } else { // White pawn push
                    if (!(target_sq & wpPushes(piece_sq))) {
                        return 0;
                    }
                }
            }
            break;
        case 1:
            // Knight move
            if (!(target_sq & knightAttacks(piece_sq))) {
                return 0;
            }
            break;
        case 2:
            // Bishop move
            if (!(target_sq & bishopAttacks(piece_sq, ~gs->all_bb))) {
                return 0;
            }
            break;
        case 3:
            // Rook move
            if (!(target_sq & rookAttacks(piece_sq, ~gs->all_bb))) {
                return 0;
            }
            break;
        case 4:
            // Queen move
            if (!(target_sq & queenAttacks(piece_sq, ~gs->all_bb))) {
                return 0;
            }
            break;
        case 5:
            // King move
            if (!(target_sq & kingAttacks(piece_sq))) {
                return 0;
            }
            break;
    }
    // Else, passed all checks
    return 1;
}