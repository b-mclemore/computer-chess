#include "chess.h"
#include <ctype.h>
#include <math.h>
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
            TRANSPOSITION TABLES
-------------------------------------------
===========================================

If we were to traverse a search tree naively, we might encounter the same
position many times. For instance, moving a rook, then the opponent moves a
pawn, then we move a knight would be an equivalent position to moving the knight
then the pawn then the rook. Such equivalent positions by different moves are
known as "transpositions". A naive search might encounter the same position
without knowing it, potentially re-evaluating the entire position at some cost.

To avoid recomputing the evaluation, we can instead store the evaluation of the
board in a hash table indexed by the current position. That way, if we encounter
the same position, we can do a hash lookup rather than re-evaluate the position.

A typical way to implement such a hash table in chess engines is via "Zobrist
hashing" (https://en.wikipedia.org/wiki/Zobrist_hashing) Simply put, we first
generate a random (injective) mapping of positions (12 colored pieces * 64
squares) ---> random bitstrings. We also want to map the "extras": whose turn it
is, the en-passant square, and castling rights. Then, when we want to hash into
the table, we simply take the bitstrings for every piece on the board, XOR all
of them, and use the result as our key.

The question then is how long to make our bitstring. Certainly longer bitstrings
will mean less frequent collisions, but many positions are absurdly rare (9
queens) or outright impossible (9 pawns), meaning our domain is actually rather
sparse compared to the full set of piece combinations. So clearly some balance
is needed to be found, but for now we'll opt to simply use U64 bitboard length
bitstrings, and fix this later if collisions are found to be a real problem.
Besides, on modern computers memory shouldn't be of enormous as an issue.

*/

// For each colored piece, a code (bitstring) per square
static U64 pieceCodes[12][64];
// For each castling right (in FEN order KQkq)
#define kingsideWhite 0
#define queensideWhite 1
#define kingsideBlack 2
#define queensideBlack 3
static U64 castlingCodes[4];
// En passant squares (denoted by file)
static U64 enpassantCodes[8];
// Turn switching (xor at every move)
static U64 endTurnCode;

// Size of hash table (smaller than maximum U64, so need to modulo)
#define BIGNUMBER 0x400000

// A random bitstring generator, with input (seed) being the previous bitstring
static U64 random_bitstring(U64 x) {
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return x * 0x2545F4914F6CDD1DULL;
}

// Initialize the tables with the bitstring generator
void init_zobrist_tables() {
    // Init piecetable
    U64 prev = 1LLU;
    for (int i = 0; i < 12; i++) {
        for (square sq = h1; sq <= a8; sq++) {
            pieceCodes[i][sq] = random_bitstring(prev);
            prev = pieceCodes[i][sq];
        }
    }
    // Init extras
    for (int i = 0; i < 4; i++) {
        castlingCodes[i] = random_bitstring(prev);
        prev = castlingCodes[i];
    }
    for (int i = 0; i < 8; i++) {
        enpassantCodes[i] = random_bitstring(prev);
        prev = enpassantCodes[i];
    }
    endTurnCode = random_bitstring(prev);
}

// Debug the tables (make sure none are equal).
// for now, just checks piece-square
void debug_tables() {
    int collisions = 0;
    for (int i = 0; i < 12; i++) {
        for (square sq = h1; sq <= a8; sq++) {
            for (int j = 0; j < 12; j++) {
                for (square sq2 = h1; sq2 <= a8; sq2++) {
                    U64 key1 = pieceCodes[i][sq] % BIGNUMBER;
                    U64 key2 = pieceCodes[j][sq2] % BIGNUMBER;
                    if ((key1 == key2) && (i != j) && (sq != sq2)) {
                        ++collisions;
                    }
                }
            }
        }
    }
    if (collisions) {
        printf("WARNING: %i HASH COLLISIONS\n", collisions);
        for (int i = 0; i < 12; i++) {
            for (square sq = h1; sq <= a8; sq++) {
                printf("Key = 0x%llxULL\n", pieceCodes[i][sq]);
            }
        }
    } else {
        printf("No hash collisions detected\n");
    }
}

// Compute the hash for the current position (should avoid doing this except
// at the start)
U64 current_pos_hash(game_state *gs) {
    U64 hash = 0ULL;
    U64 bb;
    // XOR every square
    for (square sq = h1; sq <= a8; sq++) {
        bb = 1ULL << sq;
        // Only XOR if square is occupied
        if (bb & gs->all_bb) {
            // Find piece
            for (int i = 0; i < 12; i++) {
                if (bb & gs->piece_bb[i]) {
                    hash ^= pieceCodes[i][sq];
                }
            }
        }
    }
    return hash;
}

// Compute the hash for the start position
U64 start_hash() {
    game_state gs;
    init_board(&gs);
    return current_pos_hash(&gs);
}

/* transposition table (tt)
Contains the following:
- Hash key
    Since we must modulo by the number of
    elements in the array, this allows us to check whether
    we've collided
- Relative depth
- Evaluation
    This can come in three varieties: the actual "exact" score of a position,
    or the alpha/beta (maximum/minimum) score of a pruned subtree,
    requiring the use of...
- Flags */
#define EXACT 0
#define ALPHA 1
#define BETA 2
/*
- The best move in a position
    Which allows for quicker searches: we always search the best move in a
    position first
*/
typedef struct tt_t {
    U64 hash_key;
    int relativeDepth;
    int eval;
    int flag;
    int bestMove;
} tt;

tt hash_table[BIGNUMBER];

// clear TT (hash table)
void init_hash_table() {
    // loop over TT elements
    for (int index = 0; index < BIGNUMBER; index++) {
        // reset TT inner fields
        hash_table[index].hash_key = 0;
        hash_table[index].relativeDepth = -1;
        hash_table[index].eval = 0;
    }
}

/*

One great benefit of Zobrist hashing is that the key is efficiently updatable
(the ZUE to NNUE family...) meaning that we can quickly traverse a search tree
while simultaneously hashing. This arises from the property of XOR: to XOR _in_
a piece to the key is equivalent to XORing it _out_. That means, for instance,
that a rook capturing a pawn would update the key in three simple steps:
- XOR the rook's source square
- XOR the pawn's square
- XOR the rook's destination square

*/

// Updates hash key, taking an encoded move and the current hash, returning the
// updated hash
U64 update_hash(int move, U64 currentHash) {
    square source_sq = decodeSource(move);
    square dest_sq = decodeDest(move);
    piece piec = decodePiece(move);
    piece promoteTo = decodePromote(move);
    int captureFlag = decodeCapture(move);
    int doubleFlag = decodeDouble(move);
    int enPassantFlag = decodeEnPassant(move);
    int castleFlag = decodeCastle(move);
    int color = decodeTurn(move);
    int foe = 1 - color;
    piece capturedPiec = decodeCapturedPiece(move);
    // XOR using the piecetable keys and extra keys:
    U64 retHash = currentHash;
    // Source and destination
    retHash ^= pieceCodes[2 * piec + color][source_sq];
    retHash ^= pieceCodes[2 * piec + color][dest_sq];
    // Destination if capturing
    if (captureFlag)
        retHash ^= pieceCodes[2 * capturedPiec + foe][dest_sq];
    // Extras
    if (doubleFlag && color)
        retHash ^= enpassantCodes[(dest_sq - 8) % 8];
    if (doubleFlag && foe)
        retHash ^= enpassantCodes[(source_sq - 8) % 8];
    if (enPassantFlag)
        retHash ^= enpassantCodes[dest_sq];
    if (promoteTo != pawn) {
        retHash ^= pieceCodes[2 * promoteTo + color][dest_sq];
        retHash ^= pieceCodes[2 * pawn + color][dest_sq];
    }
    if (castleFlag) {
        for (int i = 0; i < 4; i++) {
            retHash ^= castlingCodes[i];
        }
    }
    // Check whether castling rights need to be updated:
    // If not castling, but moving king, forbid castling
    if (piec == king) {
        for (int i = 0; i < 4; i++) {
            retHash ^= castlingCodes[i];
        }
    }
    // If capturing on or moving from any of the corner squares, forbid castling
    // on this square
    if ((source_sq == h1) || (dest_sq == h1)) {
        retHash ^= castlingCodes[0];
    }
    if ((source_sq == a1) || (dest_sq == a1)) {
        retHash ^= castlingCodes[1];
    }
    if ((source_sq == h8) || (dest_sq == h8)) {
        retHash ^= castlingCodes[2];
    }
    if ((source_sq == a8) || (dest_sq == a8)) {
        retHash ^= castlingCodes[3];
    }
    // Switch turn
    retHash ^= endTurnCode;
    return retHash;
}

// Tries to get an eval out of the hash table. Returns 1 for failure (no eval or
// less depth), otherwise sets *eval to the value in the table
int get_eval(U64 hash, int *eval, int relativeDepth, int alpha, int beta) {
    int modHash = hash % BIGNUMBER;
    if (hash_table[modHash].hash_key == hash) {
        if (hash_table[modHash].relativeDepth >= relativeDepth) {
            // Correct key and depth, extract eval/alpha/beta
            if (hash_table[modHash].flag == EXACT) {
                *eval = hash_table[modHash].eval;
                return 0;
            }
            if ((hash_table[modHash].flag == ALPHA) &&
                (hash_table[modHash].eval <= alpha)) {
                *eval = alpha;
                return 0;
            }
            if ((hash_table[modHash].flag == BETA) &&
                (hash_table[modHash].eval >= beta)) {
                *eval = beta;
                return 0;
            }
        }
    }
    // For any failure, return 1 (manually evaluate)
    return 1;
}

// Updates hash table, taking a key and a value (the evaluation score)
void update_hash_table(U64 hash, int eval, int relativeDepth, int flag,
                       int bestMove) {
    int modHash = hash % BIGNUMBER;
    hash_table[modHash].hash_key = hash;
    hash_table[modHash].eval = eval;
    hash_table[modHash].relativeDepth = relativeDepth;
    hash_table[modHash].flag = flag;
    hash_table[modHash].bestMove = bestMove;
}

// Debugging functions: these functions print their results
// Takes a move, makes it, then undos the move, checking whether the hash
// remains the same
void debug_update(int move, char *label) {
    // Randomly init hash
    U64 initial_hash = random_bitstring(0);
    U64 updated_hash = update_hash(move, initial_hash);
    U64 undone_hash = update_hash(move, updated_hash);
    if (initial_hash != undone_hash) {
        printf("!!! FAILURE: %s update FAILED undo check\n", label);
    } else {
        printf("%s update passed undo check\n", label);
    }
}

// Uses the debug_update function to check every part of a move (moving,
// capturing, castling, extras)
void debug_all_updates() {
    // Label constructors
    char label[255];
    char *colors[2] = {"white ", "black "};
    char *pieces[6] = {"pawn", "knight", "bishop", "rook", "queen", "king"};

    // Test moving without capturing (each piece)
    for (int i = 0; i < 12; i++) {
        // Pseudorandom source/dest
        U64 source_bb = (U64)1 << ((i * 7) % 63);
        U64 dest_bb = (U64)1 << ((i * 13) % 63);
        // Set source, dest, piece, and turn flag (all others set to 0)
        int test_move = encodeMove(source_bb, dest_bb, (piece)(i / 2), 0, 0, 0,
                                   0, 0, (i % 2), 0);
        // Construct label
        strcpy(label, "");
        strcat(label, colors[i % 2]);
        strcat(label, pieces[i / 2]);
        strcat(label, " move");
        debug_update(test_move, label);
    }

    // Test capturing (each piece -> each piece)

    // Test promotion (each promotion)

    // Test doubling a pawn

    // Test en-passant capture

    // Test castling
}