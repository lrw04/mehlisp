all: mehlisp

mehlisp: mehlisp.cpp
	c++ mehlisp.cpp -o mehlisp -Wall -g -static -std=c++17

test: mehlisp test.lisp test.ans
	./mehlisp stdlib.lisp test.lisp > test.out
	diff -s test.out test.ans

clean:
	rm -f test.out mehlisp

.PHONY: all test clean
