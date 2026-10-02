all: shapes

shapes: floatingShape.cpp
	g++ -Wall -O3 -g -std=c++17 floatingShape.cpp -o shapes -lX11

clean:
	rm -f shapes