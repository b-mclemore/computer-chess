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
// Move list
typedef struct {
    int moves[256]; // List of moves (each int encodes a move)
    int count; // Number of moves in list
} moves;
// Game memory
typedef struct gameState_t {
    U64 piece_bb[12]; // The pairs of boards for each piece
    U64 color_bb[2];  // The pair of color boards
    U64 all_bb;       // The board for all pieces
    int whose_turn;   // 0 = white, 1 = black
    int castling;  // (0b0000 or nonzero, in order WKQWbkbq)
    U64 en_passant;  // If last move was double pawn push, keep track of en-passant
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
// Helper struct to keep track of moves so that they may be highlighted
typedef struct lastMove_t {
    square orig_sq;
    square dest_sq;
} last_move;
typedef enum {pawn, knight, bishop, rook, queen, king} piece;
extern void init_board(game_state *gs);
// https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation
#define INIT_POS "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define EMPTY_POS "8/8/8/8/8/8/8/8 b - - "
#define TEST_POS                                                               \
    "r1b1kb1r/p1p2ppp/2n2n2/1B2p1N1/2P5/8/PP1P1PqP/RNBQK2R w KQkq - 0 1"
#define PIECE_MAP "PpNnBbRrQqKk"
extern int parse_fen(game_state *gs, char *fen);
extern void print_board(game_state *gs, last_move *lm);
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
// Black background is dark green, white is tan
#define bbg "\x1b[42m"
#define wbg "\x1b[43m"
// Blue background to highlight the last move
#define lmbg "\x1b[46m"
// White text and black text
#define wtxt "\x1b[97m"
#define btxt "\x1b[30m"
// Reset
#define reset_txt "\x1b[0m"
extern int parse_input(game_state *gs, last_move *lm);
// For taking an index (square enum) and getting a string
char *boardStringMap[64] = {
    "h1", "g1", "f1", "e1", "d1", "c1", "b1", "a1",
    "h2", "g2", "f2", "e2", "d2", "c2", "b2", "a2",
    "h3", "g3", "f3", "e3", "d3", "c3", "b3", "a3",
    "h4", "g4", "f4", "e4", "d4", "c4", "b4", "a4",
    "h5", "g5", "f5", "e5", "d5", "c5", "b5", "a5",
    "h6", "g6", "f6", "e6", "d6", "c6", "b6", "a6",
    "h7", "g7", "f7", "e7", "d7", "c7", "b7", "a7",
    "h8", "g8", "f8", "e8", "d8", "c8", "b8", "a8",
};
// For taking an index (piece enum) and getting a piece
char *pieceStringMap[6] = {
    "p", "n", "b", "r", "q", "k"
};

#endif
