CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -g

all: text_demo

text_demo: main.cpp varlena.cpp varlena.h
	$(CXX) $(CXXFLAGS) -o text_demo main.cpp varlena.cpp

clean:
	rm -f text_demo *.o

.PHONY: all clean
