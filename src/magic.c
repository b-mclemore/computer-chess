#include "chess.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

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
             MAGIC BITBOARDS
-------------------------------------------
===========================================

Here we introduce magic bitboards for sliding move generation.
Essentially, we use a pre-initialized lookup table and some "magic"
to calculate sliding piece moves, requiring a mask and magic.

The motivation behind magic bitboards is to just use a lookup table keyed
by the sliding piece's current square and any blocking squares along its
sliding directions to find the squares which the sliding piece can move to,
rather than generate each legal square "on-the-fly" while looking at legal
moves.

However, this would involve A LOT of data, since there are so many possible
key-value pairs. Instead, we use a kind of hash table indexed by "magic". The
math and details are a little too much to go into here, but this website
(https://www.chessprogramming.org/Magic_Bitboards)
explains magic bitboards in great detail. All that we need to understand is that
our hash key is a combination of the current square, the "occupancy" squares (pieces
which would block the sliding piece) "masked" so that we only look at those along the
sliding directions, and a bit of "magic" to make hash collisions useful - that is, 
when keys collide, they only collide if the resulting legal moves would be the same.

*/

// Masks and magics
U64 rookMasks[64];
U64 rookMagics[64];
U64 rookAttacksTable[64][4096];

U64 bishopMasks[64];
U64 bishopMagics[64];
U64 bishopAttacksTable[64][4096];

U64 edgeMask = 0b0000000001111110011111100111111001111110011111100111111000000000;
U64 rank1 	 = 0b0000000000000000000000000000000000000000000000000000000011111111;
U64 rank8 	 = 0b1111111100000000000000000000000000000000000000000000000000000000;
U64 fileA 	 = 0b1000000010000000100000001000000010000000100000001000000010000000;
U64 fileH 	 = 0b0000000100000001000000010000000100000001000000010000000100000001;
U64 empty 	 = 0b0000000000000000000000000000000000000000000000000000000000000000;

/*

Initializing the masks is easy enough. We simply generate the sliding piece's moves
"naively" (via our original Dumb7Fill ray method) and mask out the edges, then add
this to the mask array.

*/

U64 get_edge_mask(square sq) {
	return ((rank1 | rank8) & ~(rank1 << 8 * (sq / 8))) | ((fileA | fileH) & ~(fileH << (sq % 8)));
}

static void init_rook_masks(U64 masks[const 64]) {
	for (square sq = h1; sq <= a8; sq++) {
		U64 edgeMask = get_edge_mask(sq);
		U64 rookMoves = rookAttacks((U64)1 << sq, ~empty);
		masks[sq] = rookMoves & ~edgeMask;
	}
}

static void init_bishop_masks(U64 masks[const 64]) {
	for (square sq = h1; sq <= a8; sq++) {
		U64 edgeMask = get_edge_mask(sq);
		U64 bishopMoves = bishopAttacks((U64)1 << sq, ~empty);
		masks[sq] = bishopMoves & ~edgeMask;
	}
}

/*

Initializing the magic numbers is more involved. We need to brute-force search for every
magic constant, which can take some time.

*/

#define OCCUPANCY_VARIATIONS 4096
#define TRUE 1
#define FALSE 0

int findMagicNumber(int square, U64 masks[const 64], U64 *magic) {
    U64 occupancies[OCCUPANCY_VARIATIONS];
    U64 attacks[OCCUPANCY_VARIATIONS];
    U64 usedAttacks[OCCUPANCY_VARIATIONS] = {0};
    U64 mask = masks[square];
    int relevantBits = __builtin_popcountll(mask);
    int occupancyCount = 1 << relevantBits;

    // Generate all possible occupancies
    for (int i = 0; i < occupancyCount; ++i) {
        occupancies[i] = 0ULL;
        int bitIndex = 0;
        for (int bit = 0; bit < 64; ++bit) {
            if (mask & (1ULL << bit)) {
                if (i & (1ULL << bitIndex)) {
                    occupancies[i] |= (1ULL << bit);
                }
                ++bitIndex;
            }
        }
        attacks[i] = rookAttacks(square, ~occupancies[i]);
    }

    // Random number generator
    srand(time(NULL));

    // Try finding a magic number
    for (int k = 0; k < 100000000; ++k) {
        *magic = ((U64)rand() << 32) | rand();

        // Check for magic number validity
        for (int i = 0; i < OCCUPANCY_VARIATIONS; i++) usedAttacks[i] = 0ULL;
        int fail = FALSE;
        for (int i = 0; i < occupancyCount; i++) {
            U64 index = (occupancies[i] * (*magic)) >> (64 - relevantBits);
            if (usedAttacks[index] == 0ULL) {
                usedAttacks[index] = attacks[i];
            } else if (usedAttacks[index] != attacks[i]) {
                fail = TRUE;
                break;
            }
        }
        if (!fail) return TRUE;
    }
    return FALSE;
}

static void init_rook_magics(U64 masks[const 64], U64 magics[const 64]) {
    for (square sq = h1; sq <= a8; sq++) {
        U64 magicNum;
        if (findMagicNumber(sq, masks, &magicNum)) {
            magics[sq] = magicNum;
            printf("Magic number for square %s: 0x%016llx\n", boardStringMap[sq], magicNum);
        } else {
            fprintf(stderr, "Failed to find magic number for square %s\n", boardStringMap[sq]);
        }
    }
}

static void init_rook_tables(U64 masks[const 64], U64 magics[const 64], U64 attacks[64][4096]) {
	for (square sq = h1; sq <= a8; sq++) {
        int relevantBits = __builtin_popcountll(masks[sq]);
        int occupancyCount = 1 << relevantBits;
        
        for (int i = 0; i < occupancyCount; ++i) {
            U64 occupancy = 0ULL;
            int bitIndex = 0;
            for (int bit = 0; bit < 64; ++bit) {
                if (masks[sq] & (1ULL << bit)) {
                    if (i & (1 << bitIndex)) {
                        occupancy |= (1ULL << bit);
                    }
                    ++bitIndex;
                }
            }
            U64 index = (occupancy * magics[sq]) >> (64 - relevantBits);
            attacks[sq][index] = rookAttacks(1ULL << sq, ~occupancy);
        }
    }
}

static void init_bishop_magics(U64 masks[const 64], U64 magics[const 64]) {

}

void init_magic_bitboards() {
	init_rook_masks(rookMasks);
	init_bishop_masks(bishopMasks);
	init_rook_magics(rookMasks, rookMagics);
	init_bishop_magics(bishopMasks, bishopMagics);
	init_rook_tables(rookMasks, rookMagics, rookAttacksTable);
}

U64 magicRookAttacks(square rook_sq, U64 occupancy) {
    U64 relevantOccupancy = occupancy & rookMasks[rook_sq];
    relevantOccupancy *= rookMagics[rook_sq];
    relevantOccupancy >>= (64 - __builtin_popcountll(rookMasks[rook_sq]));
    return rookAttacksTable[rook_sq][relevantOccupancy];
}

U64 magicBishopAttacks(square bishop_sq, U64 all_bb) {
	return bishopAttacksTable[bishop_sq][(((all_bb | bishopMasks[bishop_sq]) ^ 7) * bishopMagics[bishop_sq]) >> 55];
}