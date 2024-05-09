#ifndef CHESS_H
#define CHESS_H
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
/* utility macro for malloc() (memory allocation on the heap) for N things of type T,
   and casting to T* type */
#define MALLOC(N, T) (T *)(malloc((N) * sizeof(T)))
// Game memory
typedef struct gameState_t {
    U64 piece_bb[12]; 	// The pairs of boards for each piece
	U64 color_bb[2];	// The pair of color boards
	U64 all_bb;			// The board for all pieces
	int whose_turn;		// 0 = white, 1 = black
	int castling[4];	// (0 or nonzero, in order WKQWbkbq)
	int en_passant;		/* Must be between 0 and 63, for how much to bit-shift a 1 to the left to get
						   the position bit-vector. This means that 0 is the bottom-right corner and 63
						   is the top left, moving left down the row before up a column */
	int halfmove_counter; // Counter for 50 move rule
	int moves;			// Number of moves in game
} game_state;


/*
===========================================
-------------------------------------------
                BITBOARDS
-------------------------------------------
===========================================
*/

#define U64 unsigned long long
// backwards, so that bit shifting >> from top right square moves right, and >> 8 times goes
// down. That is, 1 << 63 is the first square, a1, and 1 is the last square, h8
typedef enum {
	h8, h7, h6, h5, h4, h3, h2, h1,
	g8, g7, g6, g5, g4, g3, g2, g1,
	f8, f7, f6, f5, f4, f3, f2, f1,
	e8, e7, e6, e5, e4, e3, e2, e1,
	d8, d7, d6, d5, d4, d3, d2, d1,
	c8, c7, c6, c5, c4, c3, c2, c1,
	b8, b7, b6, b5, b4, b3, b2, b1,
	a8, a7, a6, a5, a4, a3, a2, a1,
} square;
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

#endif
