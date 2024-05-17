all: aldan aldanuci

aldan: aldan.c
	gcc -O2 -Wall -Wextra bitboards.c search.c eval.c interface.c aldan.c -o aldan

aldanuci: aldanuci.c
	x86_64-w64-mingw32-gcc -O2 -Wall -Wextra bitboards.c search.c eval.c interface.c aldanuci.c -o aldanuci.exe

clean:
	rm -f aldan aldanuci.exe