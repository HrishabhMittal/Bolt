CXX = g++
CXXFLAGS = -std=c++23 -Os --static -flto -march=native
INCLUDES = -Ibvm/include
all:
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) src/main.cpp -o build/boltc
