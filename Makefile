all:
	mkdir -p build
	g++ -std=c++23 src/main.cpp -o build/boltc -g -Ibvm/include
