OUTPUT=server
CFLAGS= -g -std=c99
LFLAGS=-lm -pthread

all:
	gcc $(CFLAGS) strBST.c strbuf.c userBST.c server.c -o server $(LFLAGS)

clean:
	rm -rf server