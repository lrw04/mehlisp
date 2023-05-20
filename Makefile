all: mehlisp

mehlisp: mehlisp.cpp
	c++ mehlisp.cpp -o mehlisp -Wall -Os -static -std=c++17

test: mehlisp test.in test.ans
	./mehlisp < test.in > test.out
	diff test.out test.ans

clean:
	rm -f test.out mehlisp

.PHONY: all test clean
