all: mehlisp

mehlisp: mehlisp.c
	cc mehlisp.c -o mehlisp -Wall -Os -lm

test: mehlisp test.in test.ans
	./mehlisp < test.in > test.out
	diff test.out test.ans

.PHONY: all test
