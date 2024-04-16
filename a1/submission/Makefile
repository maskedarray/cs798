all: hello_world.o

hello_world.o: hello_world.cpp
	g++ -std=c++11 -pthread -o hello_world.o hello_world.cpp

clean:
	rm -f hello_world.o