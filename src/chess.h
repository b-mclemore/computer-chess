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

/*
===========================================
-------------------------------------------
           COMMAND LINE INTERFACE
-------------------------------------------
===========================================
*/
extern int parse_input(game_state *gs);

#endif
