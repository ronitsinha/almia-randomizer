all: test

test: clean
	g++ util.cpp lcs.cpp pokedatastructure.cpp \
		pokeid.cpp test.cpp -o test.out

clean:
	rm -f *.out