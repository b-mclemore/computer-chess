# computer-chess

### Introduction

A straightforward, documented chess engine with a command-line interface. A UCI compliant interface is included to allow for playing with more serious GUI's like Arena.

### Code overview

In `src`:
* **bitboards.c**: Framework for representing the board as a [bitboard](https://www.chessprogramming.org/Bitboards) as well as move generation.
* **magic.c** A simple utility to use lookup tables to generate sliding piece moves in bitboard format, using [magic bitboards](https://www.chessprogramming.org/Magic_Bitboards)
* **interface.c**: The command-line interface and debugging functions. 
* **search.c**: Code to search through a tree of legal moves.
* **transposition.c** Code to keep track of transpositions while moving through the search tree, using Zobrist hashing.
* **eval.c**: Code to evaluate a given position, necessary for the search. Includes piece-square tables.
* **aldan.c** The command-line loop (and main function) for command-line play.
* **aldanuci.c**: The UCI-compliant interface for Windows.

In `tuning`:
* **texel.ipynb**: Notebook for [texel tuning](https://www.chessprogramming.org/Texel%27s_Tuning_Method) (Under construction)
