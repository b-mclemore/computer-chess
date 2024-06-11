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

// A helper to get a random U64
static U64 random_u64() {
  U64 u1, u2, u3, u4;
  u1 = (U64)(rand()) & 0xFFFF; u2 = (U64)(rand()) & 0xFFFF;
  u3 = (U64)(rand()) & 0xFFFF; u4 = (U64)(rand()) & 0xFFFF;
  return u1 | (u2 << 16) | (u3 << 32) | (u4 << 48);
}

// A helper to get a random U64 with a low number of nonzero bits
U64 random_u64_fewbits() {
  return random_u64() & random_u64() & random_u64();
}

const int BitTable[64] = {
  63, 30, 3, 32, 25, 41, 22, 33, 15, 50, 42, 13, 11, 53, 19, 34, 61, 29, 2,
  51, 21, 43, 45, 10, 18, 47, 1, 54, 9, 57, 0, 35, 62, 31, 40, 4, 49, 5, 52,
  26, 60, 6, 23, 44, 46, 27, 56, 16, 7, 39, 48, 24, 59, 14, 12, 55, 38, 28,
  58, 20, 37, 17, 36, 8
};

int pop_1st_bit(U64 *bb) {
  U64 b = *bb ^ (*bb - 1);
  unsigned int fold = (unsigned) ((b & 0xffffffff) ^ (b >> 32));
  *bb &= (*bb - 1);
  return BitTable[(fold * 0x783a9b23) >> 26];
}

U64 index_to_uint64(int index, int bits, U64 m) {
  int j;
  U64 result = 0ULL;
  for(int i = 0; i < bits; i++) {
    j = pop_1st_bit(&m);
    if(index & (1 << i)) result |= (1ULL << j);
  }
  return result;
}

// Helper to index into lookup table. Needs occupancy to already be masked
static int get_magic_key(U64 occupancy, U64 magic, int numBits) {
  return (int)((occupancy * magic) >> (64 - numBits));
}

static U64 find_magic(square sq, int numBits, int doRooks, U64 masks[64]) {
	U64 mask = masks[sq];
	U64 b[4096], a[4096], magic;
	int i, j, k, n, fail;

	U64 *used = doRooks? rookAttacksTable[sq] : bishopAttacksTable[sq];

	// Find number of flipped bits
	n = __builtin_popcountll(mask);

	for (i = 0; i < (1 << n); i++) {
		b[i] = index_to_uint64(i, n, mask);
		a[i] = doRooks? rookAttacks(1ULL << sq, ~b[i]) : bishopAttacks(1ULL << sq, ~b[i]);
	}
	// Try out up to a million different random keys until we find one
	for(k = 0; k < 100000000; k++) {
		magic = random_u64_fewbits();
		if(__builtin_popcountll((mask * magic) & 0xFF00000000000000ULL) < 6) continue;
		for(i = 0; i < 4096; i++) used[i] = 0ULL;
		for(i = 0, fail = 0; !fail && i < (1 << n); i++) {
			j = get_magic_key(b[i], magic, numBits);
			if(used[j] == 0ULL) used[j] = a[i];
			else if(used[j] != a[i]) fail = 1;
		}
		if(!fail) return magic;
	}
	printf("***Failed***\n");
	return 0ULL;
}

static const int RBits[64] = {
  12, 11, 11, 11, 11, 11, 11, 12,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  12, 11, 11, 11, 11, 11, 11, 12
};

static const int BBits[64] = {
  6, 5, 5, 5, 5, 5, 5, 6,
  5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 7, 7, 7, 7, 5, 5,
  5, 5, 7, 9, 9, 7, 5, 5,
  5, 5, 7, 9, 9, 7, 5, 5,
  5, 5, 7, 7, 7, 7, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5,
  6, 5, 5, 5, 5, 5, 5, 6
};

void init_magic_bitboards() {
	init_rook_masks(rookMasks);
	init_bishop_masks(bishopMasks);
	for (square sq = h1; sq <= a8; sq++) {
		rookMagics[sq] = find_magic(sq, RBits[sq], 1, rookMasks);
	}
	for (square sq = h1; sq <= a8; sq++) {
		bishopMagics[sq] = find_magic(sq, BBits[sq], 0, bishopMasks);
	}
}

U64 magicRookAttacks(square rook_sq, U64 occupancy) {
    return rookAttacksTable[rook_sq][get_magic_key(occupancy & rookMasks[rook_sq], rookMagics[rook_sq], RBits[rook_sq])];
}

U64 magicBishopAttacks(square bishop_sq, U64 occupancy) {
	return bishopAttacksTable[bishop_sq][get_magic_key(occupancy & bishopMasks[bishop_sq], bishopMagics[bishop_sq], BBits[bishop_sq])];
}

U64 magicQueenAttacks(square queen_sq, U64 occupancy) {
	return (magicBishopAttacks(queen_sq, occupancy) | magicRookAttacks(queen_sq, occupancy));
}