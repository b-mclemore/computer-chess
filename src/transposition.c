#include "chess.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
            TRANSPOSITION TABLES
-------------------------------------------
===========================================

If we were to traverse a search tree naively, we might encounter the same position
many times. For instance, moving a rook, then the opponent moves a pawn, then we move
a knight would be an equivalent position to moving the knight then the pawn then the rook.
Such equivalent positions by different moves are known as "transpositions". A naive search
might encounter the same position without knowing it, potentially re-evaluating the entire
position at some cost.

To avoid recomputing the evaluation, we can instead store the evaluation of the board in a
hash table indexed by the current position. That way, if we encounter the same position,
we can do a hash lookup rather than re-evaluate the position.

A typical way to implement such a hash table in chess engines is via "Zobrist hashing"
(https://en.wikipedia.org/wiki/Zobrist_hashing)
Simply put, we first generate a random (injective) mapping of positions (12 colored pieces * 64 
squares) ---> random bitstrings. Then, when we want to hash into the table, we simply take the
bitstrings for every piece on the board, XOR all of them, and use the result as our key.

The question then is how long to make our bitstring. Certainly longer bitstrings will mean less
frequent collisions, but many positions are absurdly rare (9 queens) or outright impossible (9 pawns),
meaning our domain is actually rather sparse compared to the full set of piece combinations.
So clearly some balance is needed to be found, but for now we'll opt to simply use U64 bitboard
length bitstrings, and fix this later if collisions are found to be a real problem. Besides, on
modern computers memory shouldn't be of enormous as an issue.

*/

// Initialize here


/*

One great benefit of Zobrist hashing is that the key is efficiently updatable (the ZUE to NNUE family...)
meaning that we can quickly traverse a search tree while simultaneously hashing. This arises from the
property of XOR: to XOR _in_ a piece to the key is equivalent to XORing it _out_. That means, for instance,
that a rook capturing a pawn would update the key in three simple steps:
- XOR the rook's source square
- XOR the pawn's square
- XOR the rook's destination square

*/

