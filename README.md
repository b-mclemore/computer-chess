# computer-chess

### Introduction

A straightforward, documented chess engine with a command-line interface. A UCI compliant interface is included to allow for playing with more serious GUI's like Arena.

I've chosen to calculate moves on-the-fly (no lookup tables), and I've tried to avoid conditional branching when computing legal moves.

### Code overview

In `src`:
* **bitboards.c**: Framework for representing the board as a [bitboard](https://www.chessprogramming.org/Bitboards) as well as move generation.
* **interface.c**: The command-line interface and debugging functions.
* **aldanuci.c**: The UCI-compliant interface. 
* **search.c**: Code to search through a tree of legal moves.
* **eval.c**: Code to evaluate a given position, necessary for the search.

In `tuning`:
* **texel.ipynb**: Notebook for [texel tuning](https://www.chessprogramming.org/Texel%27s_Tuning_Method) (Under construction)
* **cnn.ipynb**: Notebook to train a convolutional neural network to evaluate a 16-channel bitboard with a 3x3 kernel. The trained model's weights will then be used in `eval.c`. (Under construction)
