all: mehlisp

mehlisp: mehlisp.cpp
	zig c++ mehlisp.cpp -o mehlisp -Wall -Os -static -std=c++17
	zig c++ -target x86_64-linux-musl mehlisp.cpp -o mehlisp.linux -Wall -Os -static -std=c++17
	zig c++ -target x86_64-windows mehlisp.cpp -o mehlisp.exe -Wall -Os -static -std=c++17
	zig c++ -target x86_64-macos mehlisp.cpp -o mehlisp.macos -Wall -Os -static -std=c++17
	zig c++ -target aarch64-macos mehlisp.cpp -o mehlisp.macos.as -Wall -Os -static -std=c++17

test: mehlisp test.in test.ans
	./mehlisp < test.in > test.out
	diff test.out test.ans

clean:
	rm -f test.out mehlisp

.PHONY: all test clean
