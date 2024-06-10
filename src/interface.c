#define _CRT_SECURE_NO_WARNINGS
#include "chess.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timeb.h>
#ifdef _WIN32
#include <windows.h>
#endif

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
           COMMAND LINE INTERFACE
-------------------------------------------
===========================================

*/

const char *boardStringMap[64] = {
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
const char *pieceStringMap[6] = {
    "p", "n", "b", "r", "q", "k"
};

// It will be useful to have a bitboard printer as a debugging tool, which we'll
// define here. It's pretty naive, just iterating through the bits and printing
// 1 or 0.
// We also want to color the board, which we do by printing the codes defined in
// the header
void print_bitboard(U64 bitboard, int color) {
    char *txt = wtxt;
    if (color)
        txt = btxt;
    // start position is a 1 in the far left bit
    U64 start = (U64)1 << 63;
    // we choose to make rows the "slow" axis, columns the "fast" axis
    for (int row = 0; row < 8; row++) {
        // print num for row
        printf("%i  ", 8 - row);
        for (int col = 0; col < 8; col++) {
            // Set background color
            char *bg = wbg;
            if ((row + col) % 2) {
                bg = bbg;
            }
            printf("%s%s", bg, txt);
            start &bitboard ? printf("[]") : printf("  ");
            printf("%s", reset_txt);
            start >>= 1;
        }
        printf("\n");
    }
    printf("\n");
    // print letter for cols
    printf("   A B C D E F G H \n\n");
}

// Helper to print the extras (non-bitboard gamestate information)
void print_extras(game_state *gs) {
    if (gs->whose_turn) {
        printf("Black to play\n");
    } else {
        printf("White to play\n");
    }
    if (gs->castling & 0b1000) {
        printf("White may castle kingside\n");
    }
    if (gs->castling & 0b0100) {
        printf("White may castle queenside\n");
    }
    if (gs->castling & 0b0010) {
        printf("Black may castle kingside\n");
    }
    if (gs->castling & 0b0001) {
        printf("Black may castle queenside\n");
    }
    if (gs->en_passant) {
        printf("The en-passant square is at %s\n", boardStringMap[(bbToSq(gs->en_passant))]);
    }
    printf("There have been %i halfmoves since the last pawn move or capture\n",
           gs->halfmove_counter);
    printf("There have been %i total moves this game\n", gs->moves);
}

/*

FEN is a useful "human-readable" notation for giving the engine a particular
position. For example, the initial position is given in the header as INIT_POS,
as well as an empty board and a position following white's failure to find the
"computer move" in the Ulvestad variation.

https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation

To map between bitboards and FEN, we simply parse through the FEN string and
update each bitboard, given a few rules about FEN strings. Once a space is
encountered, we parse the extras. In the header is also a string of the piece
characters, which we'll use to access the piece bitboards.

*/

char *castle_map = "KQkq";

// Parses only the string after the squares in a FEN string
// (Warning: very ugly)
static int parse_extras(game_state *gs, char *inp, long unsigned int idx) {
    // First, copy the fen to a bigger buffer to avoid faults
    char fen[400];
    strcpy(fen, inp);
    // Next, our idx should move past the space
    idx++;
    // Find whose turn it is
    if (fen[idx] == 'w') {
        gs->whose_turn = 0;
    } else if (fen[idx] == 'b') {
        gs->whose_turn = 1;
    } else {
        return 1;
    }
    idx += 1;
    if (fen[idx] != ' ')
        return 1;
    idx += 1;
    // Now check for - or k/q chars
    if (fen[idx] == '-') {
        idx += 1;
        if (fen[idx] != ' ')
            return 1;
        idx += 1;
    } else if (fen[idx] == ' ') {
        return 1;
    } else {
        do {
            char *piece = strchr(castle_map, fen[idx]);
            if (piece) {
                // Locate idx in castlemap
                int index = (int)(piece - castle_map);
                gs->castling |= 1 << (3 - index);
            } else if (fen[idx] == ' ') {
                idx += 1;
                break;
            } else {
                return 1;
            }
            idx += 1;
        } while (1);
    }
    // Now get en-passant square
    if (fen[idx] == '-') {
        idx += 1;
        if (fen[idx] != ' ')
            return 1;
        idx += 1;
    } else if (fen[idx] == ' ') {
        return 1;
    } else {
        if (!isdigit(fen[idx])) {
            return 1;
        }
        // Extract the number
        while (isdigit(fen[idx])) {
            gs->en_passant = (gs->en_passant * 10) + (fen[idx] - '0');
            idx++;
        }
        if (fen[idx] != ' ') {
            return 1;
        } else {
            idx++;
        }
    }
    // Now get halfmoves
    if (!isdigit(fen[idx])) {
        return 1;
    }
    // Extract the number
    while (isdigit(fen[idx])) {
        gs->halfmove_counter = (gs->halfmove_counter * 10) + (fen[idx] - '0');
        idx++;
    }
    if (fen[idx] != ' ') {
        return 1;
    } else {
        idx++;
    }
    // Now get full moves (turns)
    if (!isdigit(fen[idx])) {
        return 1;
    }
    // Extract the number
    while (isdigit(fen[idx])) {
        gs->moves = (gs->moves * 10) + (fen[idx] - '0');
        idx++;
    }
    if (idx != strlen(fen) - 1) {
        return 1;
    }
    return 0;
}

// Parses an entire FEN
int parse_fen(game_state *gs, char *fen) {
    // first, clear the bitboards
    // this means that trying parse_fen is NOT rewindable!
    clear_bitboards(gs);
    // start position is a 1 in the far left bit
    U64 pos = (U64)1 << 63;
    for (long unsigned int idx = 0; idx < strlen(fen); idx++) {
        char ch = fen[idx];
        // if /, do nothing
        if (ch == '/') {
            continue;
        }
        // if number, move position by this many to the right
        else if ((0 < (ch - 48)) && ((ch - 48) <= 8)) {
            // make sure that we don't get multi-digit numbers
            if (idx != strlen(fen) - 1) {
                int num2 = fen[idx + 1] - 48;
                if ((0 < num2) && (num2 < 10)) {
                    return 1;
                }
            }
            // move by allowable num
            pos >>= (ch - 48);
            continue;
        }
        // if piece,find occurence of current character in piecemap and update
        char *piece = strchr(PIECE_MAP, ch);
        if (piece) {
            // Locate idx in piecemap
            int index = (int)(piece - PIECE_MAP);
            // Set position in piece, color, and overall bitboards
            gs->piece_bb[index] ^= pos;
            gs->color_bb[(index % 2)] ^= pos;
            gs->all_bb ^= pos;
            // if space, reached extras section. certain letters are reused, so
            // we need a new conditional branch
        } else if (ch == ' ') {
            return parse_extras(gs, fen, idx);
            // for any other character return 1 for error: bad FEN string
        } else {
            return 1;
        }
        pos >>= 1;
    }
    return 0;
}

/*

Finally, we introduce a "GUI" (sort of). Unicode has kindly given us every
chess piece, so we can use the command line to play, but Windows can't render
the black pawn piece correctly. Therefore (since I have a PC) we'll need to
paint the pieces black, and use a different square color. I've chosen green and
tan, to match the chess.com color scheme.

*/

// Most fonts have unicode pieces doublewide, so we add a space so that they
// don't get chopped in the command line
char *unicode_pieces[6] = {"♙ ", "♞ ", "♝ ", "♜ ", "♛ ", "♚ "};
char *ascii_pieces[6] = {"p ", "N ", "B ", "R ", "Q ", "K "};

void print_board(game_state *gs, last_move *lm, int useUnicode) {
    printf("\n");
    // start position is a 1 in the far left bit
    U64 pos = (U64)1 << 63;
    // we choose to make rows the "slow" axis, columns the "fast" axis
    for (int row = 0; row < 8; row++) {
        // print num for row
        printf("%i  ", 8 - row);
        for (int col = 0; col < 8; col++) {
            // Set background color
            char *bg = wbg;
            if ((row + col) % 2) {
                bg = bbg;
            }
            // If on the last move's squares, use a different color
            square curr_square = 63 - (8 * row + col);
            if ((curr_square == lm->orig_sq) || (curr_square == lm->dest_sq)) {
                bg = lmbg;
            }
            // if no piece, continue, else color the piece
            char *txt = wtxt;
            // No piece = print double-wide space (unicode pieces are doublewide)
            char *piece = "  ";
            if (pos & gs->all_bb) {
                if (pos & gs->color_bb[1]) {
                    txt = btxt;
                }
                // Find which piece
                for (int pc_idx = 0; pc_idx < 12; pc_idx++) {
                    if (pos & gs->piece_bb[pc_idx]) {
                        if (useUnicode) {
                            piece = unicode_pieces[pc_idx / 2];
                        } else {
                            piece = ascii_pieces[pc_idx / 2];
                        }
                        break;
                    }
                }
            }
            // Formatting followed by piece
            printf("%s%s%s%s", txt, bg, piece, reset_txt);
            // Iterate to next bitboard bit
            pos >>= 1;
        }
        printf("\n");
    }
    printf("\n");
    // print letter for cols
    printf("   A B C D E F G H \n\n");
    // Force board to be printed
    fflush(stdout);
}

// It will also be useful to print all bitboards from the command line
void print_all_bitboards(game_state *gs) {
    for (int i = 0; i < 12; i++) {
        char *color = "white";
        if (i % 2) {
            color = "black";
        }
        printf("The %s %s:\n\n", color, unicode_pieces[i / 2]);
        print_bitboard(gs->piece_bb[i], i % 2);
    }
    printf("All white pieces:\n\n");
    print_bitboard(gs->color_bb[0], 0);
    printf("All black pieces:\n\n");
    print_bitboard(gs->color_bb[1], 1);
    printf("All pieces:\n\n");
    print_bitboard(gs->all_bb, 0);
}

/*

In order to read a piece (if we want to view bitboards) we write a short helper
function here to match the piece.

*/

// Matches a piece to its //2 index in the gamestate bitboard list
int parse_piece(char piece) {
    if ((piece == 'p') || (piece == 'P'))
        return 0;
    else if ((piece == 'n') || (piece == 'N'))
        return 1;
    else if ((piece == 'b') || (piece == 'B'))
        return 2;
    else if ((piece == 'r') || (piece == 'R'))
        return 3;
    else if ((piece == 'q') || (piece == 'Q'))
        return 4;
    else if ((piece == 'k') || (piece == 'K'))
        return 5;
    else
        return -1;
}

// Matches a color to w->0, b->1
int parse_color(char color) {
    if ((color == 'w') || (color == 'W'))
        return 0;
    else if ((color == 'b') || (color == 'B'))
        return 1;
    else
        return -1;
}

// Helper to match a square string ("e2" например) to its
// bitboard for encoding purposes. Already assumes properly formatted
U64 strToSquare(int file, int rank) {
    return ((U64)1 << (8 * rank + (7 - file)));
}

// Helper to match a move  against a list of moves:
// simply iterate thru all moves in the list and check if we have a match
// This is very slow, but only used for human input to play against the engine,
// so time isn't a big issue
// Returns 1 if matches, 0 if not
int matchMove(int proposed_move, moves *moveList) {
    // Now can iterate thru all moves and check matches:
    for (int i = 0; i < moveList->count; i++) {
        int candidate_move = moveList->moves[i];
        if (candidate_move == proposed_move) {
            // Found match
            return 1;
        }
    }
    return 0;
}

// Helper to match a char to enum. Queen promotion is default
piece charToPiece(char input) {
    switch (input) {
        case 'N':
            return knight;
        case 'B':
            return bishop;
        case 'R':
            return rook;
        // No promotion
        case '\n':
            return pawn;
		case ' ':
			return pawn;
        default:
            return queen;
    }
}

// Debugging function to decode and print a move
void printMove(int move) {
    char *capture, *doubled, *enPassant, *castle;
    const char *source = boardStringMap[(decodeSource(move))];
    const char *dest = boardStringMap[(decodeDest(move))];
    const char *piec = pieceStringMap[decodePiece(move)];
    const char *promoteTo = pieceStringMap[decodePromote(move)];
    if (!strcmp(promoteTo, "p")) {
        promoteTo = " ";
    }
    if (decodeCapture(move)) {
        capture = "X";
    } else {
        capture = " ";
    }
    if (decodeDouble(move)) {
        doubled = "X";
    } else {
        doubled = " ";
    }
    if (decodeEnPassant(move)) {
        enPassant = "X";
    } else {
        enPassant = " ";
    }
    if (decodeCastle(move)) {
        castle = "X";
    } else {
        castle = " ";
    }
    printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t\n", source, dest, piec, promoteTo, capture, doubled, enPassant, castle);
}

// Debugging function to print all moves in a movelist
void printMoves(moves *moveList) {
    printf("\nSource\tDest\tPiece\tPromote\tCapture\tDouble\tEn pass\tCastle\n");
    for (int i = 0; i < moveList->count; i++) {
        printMove(moveList->moves[i]);
    }
}

/*

Now, a function to read "long" algebraic notation. This notation simply gives
the originating and target square: for example, e2e4 is the most popular opening
move. Therefore we need to do two things: check legality, and change the
bitboards. These already exist in bitboards.c, so really all we need to do is
parse the input.

*/

int parse_move(char *input, game_state *gs, last_move *lm) {
    // Recall that h8 = 0 and a1 = 1 << 63 (bitboard) or 63 (array index)
    // Therefore we treat rank as the slow axis and file as the fast, both with
    // size 8, so: first, make sure string has form
    // (letter)(digit)(letter)(digit), then move by 8 based on letters and 1
    // based on digits
    // Form check: set to uppercase, where char A = 65, so subtract 65. Then
    // must check every char in input to be in range (0, 8)
    input[0] = toupper(input[0]) - 65;
    input[2] = toupper(input[2]) - 65;
    input[1] -= 49;
    input[3] -= 49;
	if (input[4] != ' ') {
		input[4] = toupper(input[4]);
	}
    int form = 0;
    for (int i = 0; i < 4; i++) {
        form += ((input[i] < 0) || (7 < input[i]));
    }
    if (form) {
        printf("The move given was not recognized (squares do not exist).\n");
        return -1;
    }
    // Now, check legality: first, find move ...
    int color = gs->whose_turn;
    U64 source_bb = strToSquare(input[0], input[1]);
    U64 dest_bb = strToSquare(input[2], input[3]);
    piece promoteTo = pawn;
    promoteTo = charToPiece(input[4]);
    piece piec = pawn;
    for (int pc_idx = 2; pc_idx < 12; pc_idx++) {
        if (source_bb & gs->piece_bb[pc_idx]) {
            piec = pc_idx / 2;
            break;
        }
    }
    int captureFlag = !!(dest_bb & gs->color_bb[1 - color]);
    int doubleFlag = !!(((dest_bb << 16 & source_bb) || (dest_bb >> 16 & source_bb)) & (!piec));
    int enPassantFlag = !!((dest_bb & gs->en_passant) && (!piec));
    int castleFlag = (((dest_bb << 2 & source_bb) || (dest_bb >> 2 & source_bb)) && (piec == king));
    int move = encodeMove(source_bb, dest_bb, piec, promoteTo, captureFlag, doubleFlag, enPassantFlag, castleFlag);
    // ... next, generate all moves and see if this matches
    moves *move_list = MALLOC(1, moves);
    generateAllMoves(move_list, gs);
    if (matchMove(move, move_list)) {
        // Save status before making move
        game_state *save_file = MALLOC(1, game_state);
        saveGamestate(gs, save_file);
        // Make move ...
        makeMove(move, gs);
        // ... then check if this would put the king in check, and undo move if needed
        if (checkCheck(gs)) {
            printf("This move would have the king in check\n");
            undoPreviousMove(gs, save_file);
            free(save_file);
            return -1;
        }
    } else {
		square source_sq = decodeSource(move);
        square dest_sq = decodeDest(move);
		printf("\t%s -> %s\n",boardStringMap[source_sq], boardStringMap[dest_sq]);
        printf("The move given was not legal.\n");
        return -1;
    }
    // Update squares
    int piece_idx = (7 - input[0]) + 8 * input[1];
    int target_idx = (7 - input[2]) + 8 * input[3];
    lm->orig_sq = piece_idx;
    lm->dest_sq = target_idx;
    free(move_list);
    return 2;
}

// Helper for perft: get time in milliseconds
int get_time_ms() {
    #ifdef WIN64
        return GetTickCount();
    #else
		struct timeb time_value;
		ftime(&time_value);
		return time_value.time * 1000 + time_value.millitm;
	#endif
}

// Helper to print perft counts
void printPerft(int depth, game_state *gs, int per_move_flag) {
    U64 depth_count;
    depth += 1;
    for (int i = 1; i < depth; i++) {
        int start_ms = get_time_ms();
        if ((i == depth - 1) && per_move_flag) {
            printf("\nMoves for depth %i:\n", i);
        }
        depth_count = perft(i, gs, (i == depth - 1 ? per_move_flag : 0));
        int end_ms = get_time_ms();
        printf("Depth %i\t:\t%llu moves\t:\t%i ms\n", i, depth_count, end_ms - start_ms);
    }
}

// Helper to parse depth (should be string of digits)
int parseDepth(char *input) {
    unsigned long int idx = 0;
    int depth = 0;
    if (!isdigit(input[idx])) {
        return -1;
    }
    // Extract the number
    while (isdigit(input[idx])) {
        depth = (depth * 10) + (input[idx] - '0');
        idx++;
    }
    if (idx != strlen(input) - 1) {
        return -1;
    }
    return depth;
}

// Helper to check whether the current game has ended (no legal moves)
int checkGameover(moves *ms, game_state *gs) {
    // If there is insufficient material, the game is over
    // For now, only lone kings are insufficient material
    int material_flag = 1;
    for (piece piec = pawn; piec < king; piec++) {
        if (gs->color_bb[2 * piec] || gs->color_bb[2 * piec + 1]) {
            material_flag = 0;
            break;
        }
    }
    if (material_flag) {
        printf("The game is a draw by insufficient material.\n\n");
        return 1;
    }
    generateLegalMoves(ms, gs);
    // If there are no legal moves, the game is over
    if (ms->count == 0) {
        printf("Game over! ");
        // Flip turn to make the check function look at the next player's king
        gs->whose_turn = 1 - gs->whose_turn;
        if (checkCheck(gs)) {
            printf("%s has been checkmated.\n\n", 1 - gs->whose_turn ? "Black" : "White");
        } else {
            printf("The game is a stalemate.\n\n");
        }
        return 1;
    } else {
        return 0;
    }
}

/*
The main function: the input parser.
We return 0 for failure, -1 for no new board, and 1 for a new board.
*/

// max buffer size
#define INPUT_BUFFER 10000
int parse_input(game_state *gs, last_move *lm, int mg_table[12][64], int eg_table[12][64]) {
    // reset buffers
    setvbuf(stdin, NULL, _IOFBF, BUFSIZ);
    setvbuf(stdout, NULL, _IOFBF, BUFSIZ);
    // define buffer
    char input[INPUT_BUFFER];
    // zero out
    memset(input, 0, sizeof(input));
    fflush(stdout);
    if (!fgets(input, INPUT_BUFFER, stdin)) {
        // failed to read
        printf("Failed to read input\n");
        return 0;
    }
	else if ((strlen(input) < 4)) {
        printf("The command was not recognized, try again.\n");
        return -1;
    }
    // quit program
    else if (!strncmp(input, "-quit", 4)) {
        printf("Quitting program...\n");
        return 0;
    }
    // print available commands
    else if (!strncmp(input, "-help", 4)) {
        printf("To make a legal move, use long algebraic notation: ");
        printf("For example, e2e4 for the e4 opening.\n");
        printf("\nUtilities:\n");
        printf("-setup [FEN]\t:\tstarts a new game from a given FEN "
               "string\n\t\t\t(WARNING: a malformed FEN will still restart the "
               "game)\n");
        printf("-quit\t\t:\tquits out of the program\n");
        printf("-help\t\t:\tprints this message\n");
        printf("\nDebugging flags:\n");
        printf("-cb\t\t:\tprints the current board\n");
        printf("-ab\t\t:\tprints all bitboards\n");
        printf("-ex\t\t:\tlists the extras: whose move, castling "
               "rights,\n\t\t\ten-passant square, and number of moves\n");
        printf("-movebb\t\t:\tshow move bitboards for a piece, "
               "semi-algebraically.\n"
               "\t\t\t(WN for white knight, BR for black rook, etc)\n");
        printf("-legalmoves\t:\tprint all legal moves in the current position\n");
        printf("-perft [depth]\t:\tprint the number of legal moves at a given depth\n");
        printf("-eval\t\t:\tgives evaluation score of current position");
        return -1;
    }
    // print board for debugging
    else if (!strncmp(input, "-cb", 3)) {
        return 1;
    }
    // print all bitboards for debugging
    else if (!strncmp(input, "-ab", 3)) {
        print_all_bitboards(gs);
        return -1;
    }
    // print all extras for debugging
    else if (!strncmp(input, "-ex", 3)) {
        print_extras(gs);
        return -1;
    }
    // set up board
    else if (!strncmp(input, "-setup", 6)) {
        if (strlen(input) < 8) {
            // check to see if we can parse more
            printf("No FEN was given. Try '-setup 8/8/2k2q2/8/1R4K1/2RRRR2/8/8 "
                   "b - - 12 34'\n");
            init_board(gs);
        } else {
            char *fen = input + 7;
            if (parse_fen(gs, fen)) {
                printf("Not a valid FEN string\n");
                init_board(gs);
            }
        }
        lm->dest_sq = -1;
        lm->orig_sq = -1;
        return 1;
    }
    // show move bitboards
    else if (!strncmp(input, "-movebb", 7)) {
        if (strlen(input) != 11) {
            // must be exactly two characters
            printf("Code must be two characters. Try WN for white knight or BR "
                   "for black rook.\n");
        } else {
            int color = parse_color(input[8]);
            int piece = parse_piece(input[9]);
            if (piece == -1) {
                printf("Not a valid piece. Try WN for white knight or BR for "
                       "black rook.\n");
                return -1;
            }
            if (color == -1) {
                printf("Not a valid color. Try WN for white knight or BR for "
                       "black rook.\n");
                return -1;
            }
            U64 piece_bb = gs->piece_bb[2 * piece + color];
            if (piece == 0) {
                if (color) {
                    print_bitboard(bpPushes(piece_bb, ~gs->all_bb) | bpAttacks(piece_bb),
                                   color);
                } else {
                    print_bitboard(wpPushes(piece_bb, ~gs->all_bb) | wpAttacks(piece_bb),
                                   color);
                }
            } else if (piece == 1) {
                print_bitboard(knightAttacks(piece_bb), color);
            } else if (piece == 2) {
                printf("Testing bishop on ce4\n");
                print_bitboard(magicBishopAttacks(e4, gs->all_bb), color);
            } else if (piece == 3) {
                U64 all_rook_attacks = 0;
                U64 rooks = gs->piece_bb[2 * rook + color];
                while (rooks) {
                    // Get LSB
                    U64 lsb = rooks & -rooks;
                    // Remove LSB
                    rooks = rooks & (rooks - 1);
                    all_rook_attacks |= magicRookAttacks(bbToSq(lsb), gs->all_bb);
                }
                print_bitboard(all_rook_attacks, color);
            } else if (piece == 4) {
                print_bitboard(queenAttacks(piece_bb, ~gs->all_bb), color);
            } else if (piece == 5) {
                print_bitboard(kingAttacks(piece_bb), color);
            } else {
                printf(
                    "We haven't implemented this piece's moves yet, sorry!\n");
            }
        }
        return -1;
    }
    // Show current legal moves
    else if (!strncmp(input, "-legalmoves", 11)) {
        moves *move_list = MALLOC(1, moves);
        generateLegalMoves(move_list, gs);
        printf("Legal moves:\n");
        printMoves(move_list);
        free(move_list);
        return -1;
    }
    // Show perft counts for given depth
    else if (!strncmp(input, "-perft", 6)) {
        int depth = parseDepth(input + 7);
        if (depth == -1) {
            printf("The depth was ill-formatted, please use an integer, for example \'-perft 3\'");
        } else {
            printPerft(depth, gs, 0);
        }
        return -1;
    }
    // Show perft counts for given depth, with per-move counts
    else if (!strncmp(input, "-perfm", 6)) {
        int depth = parseDepth(input + 7);
        if (depth == -1) {
            printf("The depth was ill-formatted, please use an integer, for example \'-perft 3\'");
        } else {
            printPerft(depth, gs, 1);
        }
        return -1;
    // Show evaluation of current board (no search)
    } else if (!strncmp(input, "-eval", 5)){
        printf("Board evaluation = %i\n", evaluate(gs, mg_table, eg_table));
        return -1;
    }
    // Make computer play itself
    else if (!strncmp(input, "-test", 5)) {
        return 3;
    }
    // default: try to make move
    else {
        return parse_move(input, gs, lm);
    }
    return 0;
}