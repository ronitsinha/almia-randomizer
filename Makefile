CXX = g++
CXXFLAGS = -Wall -Wextra -Wpedantic -Wshadow

all: main

main: clean
	$(CXX) ${CXXFLAGS} util.cpp pokedatastructure.cpp \
		romprocessor.cpp main.cpp -o range_randomizer

clean:
	rm -f range_randomizer