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
								EVALUATION
-------------------------------------------
===========================================

*/

// Helper to count bits in a bitboard
int num_bits(U64 bb) {
		int count = 0;
		while (bb) {
				count++;
				bb &= bb - 1; // reset LS1B
		}
		return count;
}

// The main function: evaluating a board. Includes many helper functions to take
// into account different evaluation methods
int evaluate(game_state *gs) {
		// For now, very naive piece-based evaluation
		int color = gs->whose_turn;
		int foe = 1 - color;
		int pawn_eval =
				num_bits(gs->piece_bb[2 * pawn + color]) - num_bits(gs->piece_bb[2 * pawn + foe]);
		int knight_eval =
				num_bits(gs->piece_bb[2 * knight + color]) - num_bits(gs->piece_bb[2 * knight + foe]);
		int bishop_eval =
				num_bits(gs->piece_bb[2 * bishop + color]) - num_bits(gs->piece_bb[2 * bishop + foe]);
		int rook_eval =
				num_bits(gs->piece_bb[2 * rook + color]) - num_bits(gs->piece_bb[2 * rook + foe]);
		int queen_eval =
				num_bits(gs->piece_bb[2 * queen + color]) - num_bits(gs->piece_bb[2 * queen + foe]);
		int king_eval =
				num_bits(gs->piece_bb[2 * king + color]) - num_bits(gs->piece_bb[2 * king + foe]);
		int scaling_factor = 100;
		return scaling_factor * (pawn_eval + 3 * knight_eval + 3 * bishop_eval +
														 5 * rook_eval + 9 * queen_eval + 100 * king_eval);
}