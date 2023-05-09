all: mehlisp

mehlisp: mehlisp.c
	cc mehlisp.c -o mehlisp -Wall -Os -lm -static -std=c17

test: mehlisp test.in test.ans
	./mehlisp < test.in > test.out
	diff test.out test.ans

clean:
	rm -f test.out mehlisp

.PHONY: all test clean
