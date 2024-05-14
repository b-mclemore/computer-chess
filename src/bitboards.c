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

The struct for these bitboards lives in "game_state", which is defined in
chess.h We pass a pointer to a game_state to each of the following functions in
order to pass information about the bitboards, whose names appear below

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

Next, we want to use bitboard logic to reason about the current game, for
example knight or rook moves, as well as more complex structures like passed
pawns or a protected king.

The set of legal moves is required to play the game, while the complex
structures are needed to evaluate a position: for example, if white is down a
pawn but has a passed pawn, this is a better position than without a passed
pawn.

We'll want these functions to run extremely quick, since when we evaluate a
position we need to look multiple moves in the future, so the number of times we
use the reasoning functions will grow exponentially. Where possible, I've tried
to use vector intrinsics (SIMD functions) to speed up the program.

The logically most simple move (though not necessarily for a human) is the
knight moves, which we calculate here. We take in the knight bitboard and return
all possible (though not necessarily legal) knight moves. You'll notice that we
need to define a few consts here to assis with the computation. For example,
when we shift to the left before moving down to look at a south-west knight
move, we need to make sure we haven't rolled over to the next row.

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

Next, we add pawn moves, which are almost as simple, except for the case that a
pawn can move twice from its opening square. Pawn attacks have two branches: the
left and right attacks. Since we assume that black is always on top (opposite
white), we need to have separate calculators for black and white.

*/

U64 wpLeftAttacks(U64 pawn_bb) { return (pawn_bb << 9) & notA; }

U64 wpRightAttacks(U64 pawn_bb) { return (pawn_bb << 7) & notH; }

U64 wpAttacks(U64 pawn_bb) {
    return (wpLeftAttacks(pawn_bb) | wpRightAttacks(pawn_bb));
}

U64 bpLeftAttacks(U64 pawn_bb) { return (pawn_bb >> 7) & notA; }

U64 bpRightAttacks(U64 pawn_bb) { return (pawn_bb >> 9) & notH; }

U64 bpAttacks(U64 pawn_bb) {
    return (bpLeftAttacks(pawn_bb) | bpRightAttacks(pawn_bb));
}

U64 wpSinglePushes(U64 pawn_bb) { return (pawn_bb << 8); }

U64 wpDoublePushes(U64 pawn_bb) {
    // Needs to originate from 2nd rank
    C64 rank2 = (C64)0x000000000000FF00;
    return ((pawn_bb & rank2) << 16);
}

U64 wpPushes(U64 pawn_bb) {
    return (wpSinglePushes(pawn_bb) | wpDoublePushes(pawn_bb));
}

U64 bpSinglePushes(U64 pawn_bb) { return (pawn_bb >> 8); }

U64 bpDoublePushes(U64 pawn_bb) {
    // Needs to originate from 7th rank
    C64 rank7 = (C64)0x00FF000000000000;
    return ((pawn_bb & rank7) >> 16);
}

U64 bpPushes(U64 pawn_bb) {
    return (bpSinglePushes(pawn_bb) | bpDoublePushes(pawn_bb));
}

/*

Now, king moves: for now we'll ignore checks and just generate all moves for the
king, which are easy enough to calculate. We simply move once in every
lateral/diagonal direction and prevent wrap-around

*/

// King moves: just once in any direction
U64 kingAttacks(U64 king_bb) {
    // The forward and backward moves are the same as single pawn pushes and
    // diagonal attacks
    U64 back = (bpSinglePushes(king_bb) | bpAttacks(king_bb));
    U64 forw = (wpSinglePushes(king_bb) | wpAttacks(king_bb));
    U64 left = ((king_bb & notA) << 1);
    U64 rght = ((king_bb & notH) >> 1);
    return (back | forw | left | rght);
}

/*

More difficult are the sliding pieces: the bishops, rooks, and queens. Luckily,
a queen is simply a "bishop-rook". That is, its moves are the union of moves a
rook and bishop could make from the same square, which simplifies things. We
split the functions into eight directions: four lateral, and four diagonal.

The real difficulty in moving sliding pieces is their obstructions. To find
legal moves for the non- sliders, we simply bitwise & the opposite color piece
boards to find captures, but here we need to halt a sliding piece when it
captures; no hopping is allowed.

To do so, we use a "fill" algorithm. Essentially, we take two bitboards: the
sliding piece bitboard and the all colors bitboard. Then we implement a "fill"
for each lateral/diagonal. For example, for the backward fill, we shift >> 8 to
move down a square, iterating until we hit another piece. Then we shift once
more to represent capturing this piece. We want to avoid branching, since these
will be called extremely frequently.

For now, we use the "dumb7fill" method of moving one step at a time in the given
direction, which requires a bitboard of the empty squares (~ all_bb)

Importantly, this ignores whether we capture an opposite colored piece, but we
will do a check later to ensure that do not allow "friendly-fire"

*/

// Takes bitboard of lateral sliders and returns all backward (towards rank 1)
// rays
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

// Takes bitboard of lateral sliders and returns all backward (towards rank 8)
// rays
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
    // We consider the A file to be filled to prevent overflow
    empt &= notA;
    // Move up to six squares before encountering a piece
    ray |= lateralSliders_bb = (lateralSliders_bb << 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb << 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb << 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb << 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb << 1) & empt;
    ray |= (lateralSliders_bb << 1) & empt;
    // Where a piece was encountered, move another square
    return (ray << 1) & notA;
}

// Takes bitboard of lateral sliders and returns all right (towards H file) rays
static U64 rghtRay(U64 lateralSliders_bb, U64 empt) {
    U64 ray = lateralSliders_bb;
    // We consider the H file to be filled to prevent overflow
    empt &= notH;
    // Move up to six squares before encountering a piece
    ray |= lateralSliders_bb = (lateralSliders_bb >> 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb >> 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb >> 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb >> 1) & empt;
    ray |= lateralSliders_bb = (lateralSliders_bb >> 1) & empt;
    ray |= (lateralSliders_bb >> 1) & empt;
    // Where a piece was encountered, move another square
    return (ray >> 1) & notH;
}

// Takes a bitboard of lateral sliders and returns all rays
U64 rookAttacks(U64 rook_bb, U64 empt) {
    U64 forw = forwRay(rook_bb, empt);
    U64 back = backRay(rook_bb, empt);
    U64 left = leftRay(rook_bb, empt);
    U64 rght = rghtRay(rook_bb, empt);
    return (forw | back | left | rght);
}

// Takes a bitboard of diagonal sliders and returns all northwest (towards A8)
// rays
static U64 nwRay(U64 diagonalSliders_bb, U64 empt) {
    U64 ray = diagonalSliders_bb;
    // We consider the H file to be filled to prevent overflow
    empt &= notH;
    // Move up to six squares before encountering a piece
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 7) & empt;
    ray |= (diagonalSliders_bb << 7) & empt;
    // Where a piece was encountered, move another square
    return (ray << 7) & notH;
}

// Takes a bitboard of diagonal sliders and returns all southwest (towards A1)
// rays
static U64 swRay(U64 diagonalSliders_bb, U64 empt) {
    U64 ray = diagonalSliders_bb;
    // We consider the H file to be filled to prevent overflow
    empt &= notH;
    // Move up to six squares before encountering a piece
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 9) & empt;
    ray |= (diagonalSliders_bb >> 9) & empt;
    // Where a piece was encountered, move another square
    return (ray >> 9) & notH;
}

// Takes a bitboard of diagonal sliders and returns all northeast (towards H8)
// rays
static U64 neRay(U64 diagonalSliders_bb, U64 empt) {
    U64 ray = diagonalSliders_bb;
    // We consider the A file to be filled to prevent overflow
    empt &= notA;
    // Move up to six squares before encountering a piece
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 9) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb << 9) & empt;
    ray |= (diagonalSliders_bb << 9) & empt;
    // Where a piece was encountered, move another square
    return (ray << 9) & notA;
}

// Takes a bitboard of diagonal sliders and returns all southeast (towards H1)
// rays
static U64 seRay(U64 diagonalSliders_bb, U64 empt) {
    U64 ray = diagonalSliders_bb;
    // We consider the A file to be filled to prevent overflow
    empt &= notA;
    // Move up to six squares before encountering a piece
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 7) & empt;
    ray |= diagonalSliders_bb = (diagonalSliders_bb >> 7) & empt;
    ray |= (diagonalSliders_bb >> 7) & empt;
    // Where a piece was encountered, move another square
    return (ray >> 7) & notA;
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

One final "capturing" move to cover: en passant. This is very simple: we take a bitboard of
pawns, the en-passant square, and the current color, checking whether an opposite colored pawn
can capture a current-colored pawn if placed on the en-passant square

*/

// Takes a bitboard of pawns, the current color, and the en-passant square
U64 enPassantAttacks(U64 pawn_bb, int color, U64 enPassantSq) {
    // Sadly, I am using a conditional here
    // Find if there are any pawns which could capture
    U64 attacksFromSq;
    if (color) {
        attacksFromSq = wpAttacks(enPassantSq);
    } else {
        attacksFromSq = bpAttacks(enPassantSq);
    }
    // Return enPassantSq
    if (attacksFromSq) {
        return enPassantSq;
    } else {
        return 0;
    }
}

/*

Now we're ready to produce all "pseudo-legal" moves. We take a game_state and return encodings of
every legal move in a 4-byte int (only need 3 bytes)
          binary move bits                               hexadecimal constants
    
    0000 0000 0000 0000 0011 1111    source square       0x3f
    0000 0000 0000 1111 1100 0000    target square       0xfc0
    0000 0000 1111 0000 0000 0000    piece               0xf000
    0000 1111 0000 0000 0000 0000    promoted piece      0xf0000
    0001 0000 0000 0000 0000 0000    capture flag        0x100000
    0010 0000 0000 0000 0000 0000    double push flag    0x200000
    0100 0000 0000 0000 0000 0000    enpassant flag      0x400000
    1000 0000 0000 0000 0000 0000    castling flag       0x800000

Moves are generated by taking every piece in every bitboard and finding every square it attacks, then encoding
the move and appending to the movelist

*/

// Converts bb w/ 1 piece to its square (counts trailing zeros w/ GCC builtin)
int bbToSq(U64 bb) {
    return __builtin_ctzl(bb);
}

// Encodes information about move as an int
int encodeMove(U64 source_bb, U64 dest_bb, piece piec, piece promoteTo, int captureFlag, int doubleFlag, int enPassantFlag, int castleFlag) {
    // Grab source and dest SQUARES from bitboards
    int source_sq = bbToSq(source_bb);
    int dest_sq = bbToSq(dest_bb);
    // Force flags to be 0 or 1
    int captureBit = !!captureFlag;
    int doubleBit = !!doubleFlag;
    int enPassantBit = !!enPassantFlag;
    int castleBit = !!castleFlag;
    // Encode
    return (source_sq | (dest_sq << 6) | (piec << 12) | (promoteTo << 16) | (captureBit << 20) | (doubleBit << 21) | (enPassantBit << 22) | (castleBit << 23));
}

// Decodes information about moves:

// Decode moveint to source bitboard
square decodeSource(int move) {
    return (move & 0b111111);
}

// Decode moveint to dest bitboard
square decodeDest(int move) {
    return ((move >> 6) & 0b111111);
}

// Decode moveint to piece enum
piece decodePiece(int move) {
    return ((move >> 12) & 0b1111);
}

// Decode moveint to promotion piece enum
piece decodePromote(int move) {
    return ((move >> 16) & 0b1111);
}

// Decode moveint to capture flag
int decodeCapture(int move) {
    return (move >> 20) & 0b1;
}

// Decode moveint to double pawn push flag
int decodeDouble(int move) {
    return ((move >> 21) & 0b1);
}

// Decode moveint to en passant flag
int decodeEnPassant(int move) {
    return ((move >> 22) & 0b1);
}

// Decode moveint to castling flag
int decodeCastle(int move) {
    return ((move >> 23) & 0b1);
}

// Add a move to the movelist
void addMove(moves *move_list, int move) {  
    move_list->moves[move_list->count] = move;
    move_list->count++;
}

// Generate all moves (lots of branching)
void generateAllMoves(moves *moveList, game_state *gs) {
    // Reset move count (no need to reset list)
    moveList->count = 0;
    int move;
    // Bitboard holding current pieces, bitboard holding ONLY ONE current piece, and 
    // bitboard containing its legal moves/attacks (if any), bitboard containing ONE attack
    U64 piece_bb, source_bb, attacks_bb, currAttack_bb;
    int color = gs->whose_turn;
    int foe = 1 - color;
    // Emptiness (non-occupancy) bitboard for sliding attacks
    U64 empt = ~gs->all_bb;
    // Mask to remove attacks on the same color
    U64 friendlyFireMask = ~gs->color_bb[color];
    // Flags for encoding later
    int captureFlag, doubleFlag, enPassantFlag, castleFlag;
    piece promoteTo;
    for (piece piec = pawn; piec <= king; piec++) {
        // Get all pieces
        piece_bb = gs->piece_bb[2 * piec + color];
        // Iter thru current pieces:
        while (piece_bb) {
            // Get LSB
            source_bb = piece_bb & -piece_bb;
            // Remove LSB from piece bitboard
            piece_bb = piece_bb & (piece_bb - 1);
            // Get all attacks
            switch (piec) {
                case pawn:
                    // TODO: promotion
                    if (color) {
                        attacks_bb = ((bpPushes(source_bb) & empt) | (bpAttacks(source_bb) & gs->color_bb[foe]) | enPassantAttacks(source_bb, color, ((U64)1 << gs->en_passant)));
                    } else {
                        attacks_bb = ((wpPushes(source_bb) & empt) | (wpAttacks(source_bb) & gs->color_bb[foe]) | enPassantAttacks(source_bb, color, ((U64)1 << gs->en_passant)));
                    }
                    break;
                case knight:
                    attacks_bb = knightAttacks(source_bb);
                    break;
                case bishop:
                    attacks_bb = bishopAttacks(source_bb, empt);
                    break;
                case rook:
                    attacks_bb = rookAttacks(source_bb, empt);
                    break;
                case queen:
                    attacks_bb = queenAttacks(source_bb, empt);
                    break;
                case king:
                    // TODO: castling
                    attacks_bb = kingAttacks(source_bb);
                    break;
            }
            // Turn off friendly-fire
            attacks_bb &= friendlyFireMask;
            // Iter thru possible squares:
            while (attacks_bb) {
                // Reset flags (TODO: should make these checks for real)
                castleFlag = 0;
                promoteTo = pawn;
                // Get current attack (LSB)
                currAttack_bb = attacks_bb & -attacks_bb;
                // Remove LSB from piece bitboard
                attacks_bb = attacks_bb & (attacks_bb - 1);
                // Check whether this is an attack
                captureFlag = (currAttack_bb & gs->color_bb[foe]);
                // Check whether we've double-moved a pawn
                doubleFlag = (((currAttack_bb << 16 & source_bb) || (currAttack_bb >> 16 & source_bb)) & (!piec));
                // Check whether we've en-passant captures
                enPassantFlag = (gs->en_passant & currAttack_bb);
                // Add the move
                move = encodeMove(source_bb, currAttack_bb, piec, promoteTo, captureFlag, doubleFlag, enPassantFlag, castleFlag);
                addMove(moveList, move);
            }
        }
    }
}

// Make a move
void makeMove(int move, game_state *gs) {
    U64 source_bb = (U64)1 << decodeSource(move);
    U64 dest_bb = (U64)1 << decodeDest(move);
    piece piec = decodePiece(move);
    piece promoteTo = decodePromote(move);
    int captureFlag = decodeCapture(move);
    int doubleFlag = decodeCapture(move);
    // int enPassantFlag = decodeEnPassant(move);
    int castleFlag = decodeCastle(move);
    int color = gs->whose_turn;
    int foe = 1 - color;
    // Move in pieceboard
    gs->piece_bb[2 * piec + color] &= (~source_bb);
    gs->piece_bb[2 * piec + color] |= dest_bb;
    // Move in own color
    gs->color_bb[color] &= (~source_bb);
    gs->color_bb[color] |= dest_bb;
    // Move in overall
    gs->all_bb &= (~source_bb);
    gs->all_bb |= dest_bb;
    // If capturing, update other color
    if (captureFlag) {
        // Remove other color
        gs->color_bb[foe] &= (~dest_bb);
        // Remove from every pieceboard (kings should not be valid)
        gs->piece_bb[(0 * 2) + foe] &= (~dest_bb);
        gs->piece_bb[(1 * 2) + foe] &= (~dest_bb);
        gs->piece_bb[(2 * 2) + foe] &= (~dest_bb);
        gs->piece_bb[(3 * 2) + foe] &= (~dest_bb);
        gs->piece_bb[(4 * 2) + foe] &= (~dest_bb);
    }
    // If double pushing, update en-passant square
    if (doubleFlag) {
        if (color) {
            gs->en_passant = dest_bb << 8;
        } else {
            gs->en_passant = source_bb >> 8;
        }
    }
    // If castling, update castling array
    if (castleFlag) {
        // TODO
    }
    // If promoting, update piece
    if (promoteTo) {
        // TODO
    }
    // Now, update game state. (TODO: Change extras. Just updates move count and
    // whose_turn now)
    if (gs->whose_turn) {
        gs->moves += 1;
    }
    gs->whose_turn = foe;
}

/*

Lastly, we implement legality checking. The evaluation and search will work by generating all
moves, but only considering those which are legal. This is a lot easier than only generating
legal moves, since we have to make legality checks in the cases of pins anyways. Since pins are
rare, it's in some ways better to avoid checking them. Another benefit is that since we don't
look at many moves due to alpha-beta pruning, we avoid doing any legality checks on them
unnecessarily.

The legality checker really only needs to do one thing: place a "super-piece" on the king's
square and check if it hits anything. That means our legality checker only does "check" checking,
not castling or en-passant or anything else, which should be handled in move generation.

This "super-piece" can move like any other piece. If it encounters any piece, we see whether this
piece could attack the king.

This means that we'll need to "test" moves as well as "take back" moves, so we have two functions:
- "King attacked" checker
- "Take back" move

*/

// Checks whether the opposite colored king is in check. (If so, the previous move was illegal)
U64 checkCheck(game_state *gs) {
    // Find king
    int color = gs->whose_turn;
    int foe = 1 - color;
    U64 king_bb = gs->piece_bb[2 * king + foe];
    // Find empty bitboard for occlusion
    U64 empt = ~gs->all_bb;
    // Make all attacks (intersection of attacks w/ king)
    U64 pawn_attacks;
    if (color) {
        pawn_attacks = wpAttacks(gs->piece_bb[2 * pawn + color]);
    } else {
        pawn_attacks = bpAttacks(gs->piece_bb[2 * pawn + color]);
    }
    U64 knight_attacks = knightAttacks(gs->piece_bb[2 * knight + color]);
    U64 bishop_attacks = bishopAttacks(gs->piece_bb[2 * bishop + color], empt);
    U64 rook_attacks = rookAttacks(gs->piece_bb[2 * rook + color], empt);
    U64 queen_attacks = queenAttacks(gs->piece_bb[2 * queen + color], empt);
    U64 king_attacks = kingAttacks(gs->piece_bb[2 * king + color]);
    return (pawn_attacks | knight_attacks | bishop_attacks | rook_attacks | queen_attacks | king_attacks) & king_bb;
}

// Saves memory to allow for move take-back
void saveGamestate(game_state* gs, game_state *copy_address) {
    copy_address->all_bb = gs->all_bb;
    memcpy(copy_address->color_bb, gs->color_bb, sizeof(U64) * 2);
    memcpy(copy_address->piece_bb, gs->piece_bb, sizeof(U64) * 12);
    copy_address->whose_turn = gs->whose_turn;
    memcpy(copy_address->castling, gs->castling, sizeof(int) * 4);
    copy_address->en_passant = gs->en_passant;
    copy_address->halfmove_counter = gs->halfmove_counter;
    copy_address->moves = gs->moves;
}

// Restores game-state from memory 
void undoPreviousMove(game_state *gs, game_state *copy_address) {
    gs->all_bb = copy_address->all_bb;
    memcpy(gs->color_bb, copy_address->color_bb, sizeof(U64) * 2);
    memcpy(gs->piece_bb, copy_address->piece_bb, sizeof(U64) * 12);
    gs->whose_turn = copy_address->whose_turn;
    memcpy(gs->castling, copy_address->castling, sizeof(int) * 4);
    gs->en_passant = copy_address->en_passant;
    gs->halfmove_counter = copy_address->halfmove_counter;
    gs->moves = copy_address->moves;
}

// Generates all LEGAL moves by pruning
void generateLegalMoves(moves *move_list, game_state *gs) {
    // Init 
    move_list->count = 0;
    // First, generate pseudo-legal moves
    moves *pseudo_legal = MALLOC(1, moves);
    generateAllMoves(pseudo_legal, gs);
    // Then save memory and try all out
    game_state *save_file = MALLOC(1, game_state);
    saveGamestate(gs, save_file);
    for (int i = 0; i < pseudo_legal->count; i++) {
        // Try out move
        makeMove(pseudo_legal->moves[i], gs);
        // If it works, add to movelist
        if (!checkCheck(gs)) {
            addMove(move_list, pseudo_legal->moves[i]);
        }
        // Undo before trying next
        undoPreviousMove(gs, save_file);

    }
    free(save_file);
    free(pseudo_legal);
}