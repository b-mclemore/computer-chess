# computer-chess

### Introduction

A straightforward, documented chess engine with a command-line interface. A UCI compliant interface is included to allow for playing with more serious GUI's like Arena.

I've chosen to calculate moves on-the-fly (no lookup tables), and I've tried to avoid conditional branching when computing legal moves.

### Code overview

In `src`:
* **bitboards.c**: Framework for representing the board as a [bitboard](https://www.chessprogramming.org/Bitboards) as well as move generation.
* **interface.c**: The command-line interface and debugging functions.
* **aldan.c** The command-line loop (and main function) for command-line play.
* **aldanuci.c**: The UCI-compliant interface for Windows. 
* **search.c**: Code to search through a tree of legal moves. Includes transpositions via Zobrist hashing.
* **eval.c**: Code to evaluate a given position, necessary for the search. Includes piece-square tables.

In `tuning`:
* **texel.ipynb**: Notebook for [texel tuning](https://www.chessprogramming.org/Texel%27s_Tuning_Method) (Under construction)
