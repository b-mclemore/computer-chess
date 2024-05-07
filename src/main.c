#include <stdlib.h>
#include "bitboards.c"
#include "interface.c"
#include "chess.h"

// Main driver code - initializes board and runs parser
int main() {
	// Set up board
    init_board();
	print_board();
	printf("For all available commands, type '-help'\n");
	printf("To make a legal move, use algebraic notation: ");
	printf("For example, Nf4\n\n>");
	int flag;
	while((flag = parse_input())) {
		if (flag == 1) print_board();
		printf("\n> ");
	}
}