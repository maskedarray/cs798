#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <time.h>
using namespace std;

#define MAX_THREADS 256

struct  alignas(64) padded_subcounter {
    // char padding[64 - sizeof(atomic<int64_t>)];
    atomic<int64_t> v;
    
};

struct  globals {
    char padding0[64];
    int numThreads;
    atomic<bool> start;    
    alignas(64);
    atomic<bool> done;

    padded_subcounter subcounters[MAX_THREADS];
    
    // char padding3[64];
    
    globals(int _numThreads) {
        numThreads = _numThreads;
        start = false;
        done = false;
    }
};

globals * g;

void threadFunc(int tid) {
    // wait for all threads to be started before letting any thread do "real" work
    while (!g->start.load(std::memory_order_relaxed)) { /* busy wait */ }

    // increment my subcounter until the experiment is done
    while (true) {
        g->subcounters[tid].v.fetch_add(1, memory_order_relaxed);
        if (g->done.load(memory_order_relaxed)) break;
    }
    // printf("Thread: %d Count: %d\n", tid, g->subcounters[tid].v.load());
}

int main(int argc, char ** argv) {
    // read command line args
    if (argc != 3) {
        cout<<"USAGE: "<<argv[0]<<" SECONDS_TO_RUN NUMBER_OF_THREADS"<<endl;
        return 1;
    }
    int secondsToRun = atoi(argv[1]);
    int numThreads = atoi(argv[2]);
    
    // initialize globals
    g = new globals(numThreads);
    
    // create and start threads
    vector<thread *> threads;
    for (int i=0;i<g->numThreads;++i) {
        threads.push_back(new thread(threadFunc, i));
    }
    
    // have threads perform increments for a fixed time then stop

    g->start = true;
    this_thread::sleep_for(chrono::seconds(secondsToRun));
    g->done = true;
    
    // join threads
    for (int i=0;i<g->numThreads;++i) {
        threads[i]->join();
        delete threads[i]; // free memory
    }
    
    // print: increments performed per second
    int64_t sum = 0;
    for (int i=0;i<g->numThreads;++i) {
        sum += g->subcounters[i].v;
    }
    cout<<"throughput (increments per second)="<<(sum / secondsToRun)<<endl;
    
    delete g; // free memory allocated for globals (and call destructor)
    return 0;
}
