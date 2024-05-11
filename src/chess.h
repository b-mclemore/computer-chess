#ifndef CHESS_H
#define CHESS_H
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

*/

/*
===========================================
-------------------------------------------
                GAME STATE
-------------------------------------------
===========================================
*/

#define U64 unsigned long long
#define C64 const U64
/* utility macro for malloc() (memory allocation on the heap) for N things of
   type T, and casting to T* type */
#define MALLOC(N, T) (T *)(malloc((N) * sizeof(T)))
// Game memory
typedef struct gameState_t {
    U64 piece_bb[12]; // The pairs of boards for each piece
    U64 color_bb[2];  // The pair of color boards
    U64 all_bb;       // The board for all pieces
    int whose_turn;   // 0 = white, 1 = black
    int castling[4];  // (0 or nonzero, in order WKQWbkbq)
    int en_passant;   /* Must be between 0 and 63, for how much to bit-shift a 1
                         to the left to get   the position bit-vector. This means
                         that 0 is the bottom-right corner and 63   is the top left,
                         moving left down the row before up a column */
    int halfmove_counter; // Counter for 50 move rule
    int moves;            // Number of moves in game
} game_state;

/*
===========================================
-------------------------------------------
                BITBOARDS
-------------------------------------------
===========================================
*/

#define U64 unsigned long long
// backwards, so that bit shifting >> from top right square moves right, and >>
// 8 times goes down. That is, 1 << 63 is the first square, a1, and 1 is the
// last square, h8
typedef enum {
	h1, g1, f1, e1, d1, c1, b1, a1,
    h2, g2, f2, e2, d2, c2, b2, a2,
    h3, g3, f3, e3, d3, c3, b3, a3,
    h4, g4, f4, e4, d4, c4, b4, a4,
    h5, g5, f5, e5, d5, c5, b5, a5,
    h6, g6, f6, e6, d6, c6, b6, a6,
    h7, g7, f7, e7, d7, c7, b7, a7,
    h8, g8, f8, e8, d8, c8, b8, a8
} square;
typedef enum {pawn, knight, bishop, rook, queen, king} piece;
extern void init_board(game_state *gs);
// https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation
#define INIT_POS "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define EMPTY_POS "8/8/8/8/8/8/8/8 b - - "
#define TEST_POS                                                               \
    "r1b1kb1r/p1p2ppp/2n2n2/1B2p1N1/2P5/8/PP1P1PqP/RNBQK2R w KQkq - 0 1"
#define PIECE_MAP "PpNnBbRrQqKk"
extern int parse_fen(game_state *gs, char *fen);
extern void print_board(game_state *gs);
extern void print_all_bitboards(game_state *gs);
extern void print_extras(game_state *gs);
extern U64 knightAttacks(U64 knight_bb);
extern U64 kingAttacks(U64 king_bb);
extern U64 rookAttacks(U64 rook_bb, U64 all_bb);

/*
===========================================
-------------------------------------------
           COMMAND LINE INTERFACE
-------------------------------------------
===========================================
*/
extern int parse_input(game_state *gs);
// Helper struct to keep track of moves so that they may be highlighted
typedef struct lastMove_t {
    square orig_sq;
    square dest_sq;
} last_move;

#endif
