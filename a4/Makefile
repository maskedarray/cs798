GPP = g++-9
FLAGS = -O3 -g
FLAGS += -std=c++2a
FLAGS += -fopenmp
LDFLAGS = -lpthread

all: benchmark benchmark_debug

.PHONY: benchmark
benchmark:
	$(GPP) $(FLAGS) -o $@.out $@.cpp $(LDFLAGS) 

.PHONY: benchmark_debug
benchmark_debug:
	$(GPP) $(FLAGS) -o $@.out benchmark.cpp -DTRACE=if\(1\) $(LDFLAGS)

clean:
	rm -f *.out 