CXX = g++
# CXXFLAGS = -std=c++23 -Os --static -flto -march=native
CXXFLAGS = -std=c++23 -g
INCLUDES = -Ibvm/include
all:
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) src/main.cpp -o build/boltc
