#define _CRT_SECURE_NO_WARNINGS
#include "chess.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

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
which maximizes its own evaluated position while minimizing the evaluation of the
best move for the opponent.

*/

// Combined minimax (negaMax)
int negaMax(game_state *gs, int mg_table[12][64], int eg_table[12][64], int alpha, int beta, int depth) {
	// If at leaf node (max depth), return evaluation
    if (depth == 0) return evaluate(gs, mg_table, eg_table);
    int max = -9999999;
	int score;
	moves move_list[256];
    game_state *save_file = MALLOC(1, game_state);
	generateAllMoves(move_list, gs);
	// For every move, find the optimum
    for (int i = 0; i < move_list->count; i++)  {
		int move = move_list->moves[i];
		// First, save position
		saveGamestate(gs, save_file);
		// Next, make move
		makeMove(move, gs);
		if (!checkCheck(gs)) {
			// Make sure legal before continuing
			score = -negaMax(gs, mg_table, eg_table, -beta, -alpha, depth - 1);
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
		undoPreviousMove(gs, save_file);
    }
	// Free save file
	free(save_file);
	// Subtract one: makes engine prefer faster checkmates
    return max;
}

// Find best move via negaMax
int findBestMove(game_state *gs, int mg_table[12][64], int eg_table[12][64], int depth, int *best_score) {
	// Copied negaMax, but here we return a BEST MOVE int, not a scoring
	// int at the end
	int max = -9999999;
	int alpha = -9999999;
	int beta = -alpha;
	int score;
	moves move_list[256];
    game_state *save_file = MALLOC(1, game_state);
	generateLegalMoves(move_list, gs);
	int best_move = move_list->moves[0];
	// For every move, find the optimum
    for (int i = 0; i < move_list->count; i++)  {
		int move = move_list->moves[i];
		// First, save position
		saveGamestate(gs, save_file);
		// Next, make move
		makeMove(move, gs);
        score = -negaMax(gs, mg_table, eg_table, -beta, -alpha, depth - 1);
		/*
		square source_sq = decodeSource(move_list->moves[i]);
        square dest_sq = decodeDest(move_list->moves[i]);
		printf("\t%s -> %s\t\t:\t%i\n",boardStringMap[source_sq], boardStringMap[dest_sq], score);
		*/
		// Undo move
		undoPreviousMove(gs, save_file);
        if(score > max) {
            max = score;
			best_move = move;
		}
    }
	// Free save file
	free(save_file);
	*best_score = max;
    return best_move;
}

// Iteratively deepens: moves 1 ply at a time, finding the best move at each step.
// Useful for two cases: first, it early returns if mate is found, meaning we select
// the fastest mate, and secondly, it ensures a move is found in a given amount of time,
// even if the search hasn't finished
int iterativelyDeepen(game_state *gs, int mg_table[12][64], int eg_table[12][64], int turn_time_ms) {
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
	return best_move;
}

// Finds best move and returns a long-algebraic string version
void computerMakeMove(char output[5], game_state *gs, int mg_table[12][64], int eg_table[12][64], int depth) {
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