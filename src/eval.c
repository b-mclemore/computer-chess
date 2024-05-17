#include "chess.h"
#include <ctype.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif
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

// Helper to get a random number in a range
// Assumes 0 <= max <= RAND_MAX
// Returns in the closed interval [0, max]
long random_at_most(long max) {
  unsigned long
    // max <= RAND_MAX < ULONG_MAX, so this is okay.
    num_bins = (unsigned long) max + 1,
    num_rand = (unsigned long) RAND_MAX + 1,
    bin_size = num_rand / num_bins,
    defect   = num_rand % num_bins;

  long x;
  do {
		#ifdef _WIN32
			x = rand();
		#else
   		x = random();
		#endif
  }
  // This is carefully written not to overflow
  while (num_rand - defect <= (unsigned long)x);

  // Truncated division is intentional
  return x/bin_size;
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
		// Get random noise (between -5 and 5)
		int noise = random_at_most(20) - 10;
		return scaling_factor * (pawn_eval + 3 * knight_eval + 3 * bishop_eval +
														 5 * rook_eval + 9 * queen_eval + 100 * king_eval) + noise;
}