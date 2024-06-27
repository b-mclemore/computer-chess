#define _CRT_SECURE_NO_WARNINGS
#include "chess.h"
#include <ctype.h>
#include <limits.h>
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
               SEARCH TREE
-------------------------------------------
===========================================

To begin with, we'll use a simply minimax algorithm: the engine picks the move
which maximizes its own evaluated position while minimizing the evaluation of
the best move for the opponent.

*/
#define EXACT 0
#define ALPHA 1
#define BETA 2
int alphaBeta(game_state *gs, int mg_table[12][64], int eg_table[12][64],
              int alpha, int beta, int depth, U64 hash) {
    int score;
    // First, probe the hash table to see if we have already evaluated
    // to the required depth
    if (get_eval(hash, &score, depth, alpha, beta) == 0) {
        // If we did, immediately exit
        return score;
    }
    // Otherwise, calculate by hand
    if (depth == 0) {
        // For depth 0 (no move), do not update hash key
        score = evaluate(gs, mg_table, eg_table);
        update_hash_table(hash, score, 0, EXACT, NULLMOVE);
        return score;
    }
    moves move_list[256];
    game_state save_file;
    generateAllMoves(move_list, gs);
    int currentFlag = ALPHA;
    U64 currentHash;
    int noMovesFlag = 1;
    // Check moves and extract scores
    for (int i = 0; i < move_list->count; i++) {
        int move = move_list->moves[i];
        // First, save position
        saveGamestate(gs, &save_file);
        // Next, make move
        makeMove(move, gs);
        // Make sure legal before continuing
        if (!checkCheck(gs)) {
            noMovesFlag = 0;
            // Update hash before looking at new move
            currentHash = update_hash(move, hash);
            score = -alphaBeta(gs, mg_table, eg_table, -beta, -alpha, depth - 1,
                               currentHash);
            /*
            printf("Depth %i\n", depth);
            square source_sq = decodeSource(move_list->moves[i]);
            square dest_sq = decodeDest(move_list->moves[i]);
            printf("\t%s -> %s\t\t:\t%i\n", boardStringMap[source_sq],
                   boardStringMap[dest_sq], score);
            */
            if (score >= beta) {
                update_hash_table(hash, beta, depth, BETA, NULLMOVE);
                return beta;
            }
            if (score > alpha) {
                currentFlag = EXACT;
                alpha = score;
            }
        }
        // Undo move
        undoPreviousMove(gs, &save_file);
    }
    update_hash_table(hash, score, depth, currentFlag, NULLMOVE);
    // Check for stalemate: if we don't, then it is treated as checkmate (score
    // = alpha)
    if (noMovesFlag) {
        // Either checkmate or stalemate
        gs->whose_turn = 1 - gs->whose_turn;
        if (!checkCheck(gs)) {
            alpha = 0;
        }
        gs->whose_turn = 1 - gs->whose_turn;
    }
    return alpha;
}

/*
// Combined minimax (negaMax)
int negaMax(game_state *gs, int mg_table[12][64], int eg_table[12][64],
            int alpha, int beta, int depth, U64 hash) {
    // If at leaf node (max depth), return evaluation
    if (depth == 0)
        return evaluate(gs, mg_table, eg_table);
    int max = -9999999;
    int score;
    moves move_list[256];
    game_state save_file;
    generateAllMoves(move_list, gs);
    // For every move, find the optimum
    for (int i = 0; i < move_list->count; i++) {
        int move = move_list->moves[i];
        // First, save position
        saveGamestate(gs, &save_file);
        // Next, make move
        makeMove(move, gs);
        // Make sure legal before continuing
        if (!checkCheck(gs)) {
            // Check whether hash table holds an evaluation of sufficient depth
            U64 currentHash = update_hash(move, hash);
            if (get_eval(currentHash, &score, depth)) {
                // Otherwise, calculate by hand...
                score = -negaMax(gs, mg_table, eg_table, -beta, -alpha,
                                 depth - 1, currentHash);
                // ... and update hashtable
                // TODO: fix this
                // update_hash_table(currentHash, score, depth);
            }
            if (score > max) {
                max = score;
            }
            if (score > alpha) {
                alpha = score;
            }
            if (alpha >= beta) {
                break; // Prune
            }
        }
        // Undo move
        undoPreviousMove(gs, &save_file);
    }
    return max;
}
*/

// Find best move via negaMax (or alphaBeta)
int findBestMove(game_state *gs, int mg_table[12][64], int eg_table[12][64],
                 int depth, int *best_score) {
    int max = -9999999;
    int alpha = -9999999;
    int beta = -alpha;
    int score;
    moves move_list[256];
    game_state save_file;
    generateLegalMoves(move_list, gs);
    int best_move = move_list->moves[0];
    // Initialize hash here. Probably it would be (barely) faster to hold this
    // in the main GUI loop and update after we make a move, so that we don't
    // have to keep initializing on each turn, but I'm lazy and this is dwarfed
    // by the actual search's compute time
    U64 hash = current_pos_hash(gs);
    // For every move, find the optimum
    for (int i = 0; i < move_list->count; i++) {
        int move = move_list->moves[i];
        // First, save position
        saveGamestate(gs, &save_file);
        // Next, make move
        makeMove(move, gs);
        // Check whether hash table holds an evaluation of sufficient depth
        U64 currentHash = update_hash(move, hash);
        if (get_eval(currentHash, &score, depth, alpha, beta) != 0) {
            // Otherwise, calculate by hand
            score = -alphaBeta(gs, mg_table, eg_table, -beta, -alpha, depth - 1,
                               currentHash);
        }
        ///*
        square source_sq = decodeSource(move_list->moves[i]);
        square dest_sq = decodeDest(move_list->moves[i]);
        printf("\t%s -> %s\t\t:\t%i\n", boardStringMap[source_sq],
               boardStringMap[dest_sq], score);
        //*/

        // Undo move
        undoPreviousMove(gs, &save_file);
        if (score > max) {
            max = score;
            best_move = move;
        }
    }
    *best_score = max;
    return best_move;
}

// Iteratively deepens: moves 1 ply at a time, finding the best move at each
// step. Useful for two cases: first, it early returns if mate is found, meaning
// we select the fastest mate, and secondly, it ensures a move is found in a
// given amount of time, even if the search hasn't finished
int iterativelyDeepen(game_state *gs, int mg_table[12][64],
                      int eg_table[12][64], int turn_time_ms) {
    int ply = 1;
    int start_time = get_time_ms();
    int score;
    // Requires that at least one move is found at 1 ply
    int best_move = 0;
    while (1) {
        int curr_time = get_time_ms();
        // Early return for out of time
        if (curr_time - start_time > turn_time_ms) {
            break;
        }
        best_move = findBestMove(gs, mg_table, eg_table, ply, &score);
        // Deepen for next search
        ++ply;
        // Early return for checkmate
        if (score >= 9999999) {
            return best_move;
        }
    }
    printf("Looked %i moves ahead\n", ply);
    return best_move;
}

// Finds best move and returns a long-algebraic string version
void computerMakeMove(char output[5], game_state *gs, int mg_table[12][64],
                      int eg_table[12][64], int depth) {
    int score;
    int best_move = findBestMove(gs, mg_table, eg_table, depth, &score);
    square source_sq = decodeSource(best_move);
    square dest_sq = decodeDest(best_move);
    piece promoteTo = decodePromote(best_move);
    strcpy(output, boardStringMap[source_sq]);
    strcpy(output + 2, boardStringMap[dest_sq]);
    if (promoteTo != pawn) {
        strcpy(output + 4, pieceStringMap[promoteTo]);
    }
}

// Debugging functions
// Here we set up a basic position (two kings and two pawns)
// where one pawn is hanging. The best move should be to capture
// the pawn, and we search 1 move deep
void db_simple_pos() {
    // Set up game
    // Init game memory
    game_state *gs = MALLOC(1, game_state);
    // Set up board
    parse_fen(gs, "k7/8/8/5p2/4P3/6K1/8/8 w - - 0 1");
    // Set up magic bitboards
    init_magic_bitboards();
    // Set up piece-square tables
    int mg_table[12][64];
    int eg_table[12][64];
    init_tables(mg_table, eg_table);
    // Set up hash tables
    init_zobrist_tables();
    init_hash_table();
    // Search 1 deep
    int score;
    int best_move = findBestMove(gs, mg_table, eg_table, 1, &score);
    char output[5];
    square source_sq = decodeSource(best_move);
    square dest_sq = decodeDest(best_move);
    piece promoteTo = decodePromote(best_move);
    strcpy(output, boardStringMap[source_sq]);
    strcpy(output + 2, boardStringMap[dest_sq]);
    if (promoteTo != pawn) {
        strcpy(output + 4, pieceStringMap[promoteTo]);
    }
    printf("Found the best move to be %s, with score %i\n", output, score);
}

// Here the position is more complicated. White can fork the king
// and rook. We search 3 moves deep.
void db_fork_pos() {
    // Set up game
    // Init game memory
    game_state *gs = MALLOC(1, game_state);
    // Set up board
    parse_fen(gs, "8/8/1k3r2/8/8/4N1K1/8/8 w - - 0 1");
    // Set up magic bitboards
    init_magic_bitboards();
    // Set up piece-square tables
    int mg_table[12][64];
    int eg_table[12][64];
    init_tables(mg_table, eg_table);
    // Set up hash tables
    init_zobrist_tables();
    init_hash_table();
    // Search 1 deep
    int score;
    int best_move = findBestMove(gs, mg_table, eg_table, 3, &score);
    char output[5];
    square source_sq = decodeSource(best_move);
    square dest_sq = decodeDest(best_move);
    piece promoteTo = decodePromote(best_move);
    strcpy(output, boardStringMap[source_sq]);
    strcpy(output + 2, boardStringMap[dest_sq]);
    if (promoteTo != pawn) {
        strcpy(output + 4, pieceStringMap[promoteTo]);
    }
    printf("Found the best move to be %s, with score %i\n", output, score);
}