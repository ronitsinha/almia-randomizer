all: test

test: clean
	g++ -Wall -Werror -Wpedantic util.cpp lcs.cpp pokedatastructure.cpp \
		pokeid.cpp test.cpp -o test.out

clean:
	rm -f *.out