CFLAGS=-std=c11 -g -static	#コンパイル時のオプション

9cc: 9cc.c

test: 9cc	#9ccを前提に以下のコマンドを行え
	./test.sh

clean:
	rm -f 9cc *.o *~ tmp*

.PHONY: test clean