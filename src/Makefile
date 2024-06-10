all: aldan aldanuci aldanprofile

aldan: aldan.c
	gcc -O2 -Wall -Wextra bitboards.c search.c eval.c interface.c aldan.c magic.c transposition.c -o aldan

aldanprofile: aldan.c
	gcc -O0 -Wall -Wextra bitboards.c search.c eval.c interface.c aldan.c magic.c transposition.c -o aldanprofile -pg

aldanuci: aldanuci.c
	x86_64-w64-mingw32-gcc -O2 -Wall -Wextra bitboards.c search.c eval.c interface.c aldanuci.c magic.c transposition.c -o aldanuci.exe

clean:
	rm -f aldan aldanuci.exe aldanprofile