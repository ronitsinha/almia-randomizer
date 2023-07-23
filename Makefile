CXX = g++
CXXFLAGS = -Wall -Wextra -Wpedantic -Wshadow
OUTPUT = range_randomizer

all: main

main: clean
	$(CXX) ${CXXFLAGS} util.cpp pokedatastructure.cpp \
		romprocessor.cpp main.cpp -o ${OUTPUT}

clean:
	rm -f range_randomizer *.exe