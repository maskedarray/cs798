#include <iostream>
#include <thread>
// #include <chrono>
using namespace std;

#define TOTAL_INCREMENTS 100000000

__thread int tid;

class mutex_t {
private:
    volatile bool wants_to_enter[2];
    volatile int turn;
public:
    mutex_t(): turn(0) {
        wants_to_enter[0] = false;
        wants_to_enter[1] = false;  
    }
    void lock() {
        wants_to_enter[tid] = true;
        __sync_synchronize(); 
        while(wants_to_enter[1 - tid]) {
            // __sync_synchronize(); 
            if(turn != tid){
                wants_to_enter[tid] = false;
                __sync_synchronize(); 
                while(turn != tid){}
                wants_to_enter[tid] = true;
                __sync_synchronize(); 
            }
        } 
    }
    void unlock() {
        turn = 1 - tid;
        wants_to_enter[tid] = false;
        __sync_synchronize(); 
    }
};


class counter_locked {
private:
    mutex_t m;
    volatile int v;
public:
    counter_locked() {
        
    }

    void increment() {
        m.lock();
        v++;
        m.unlock();
    }

    int get() {
        m.lock();
        auto result = v;
        m.unlock();
        return result;
    }
};

counter_locked c;

void threadFunc(int _tid) {
    tid = _tid;
    // auto start = chrono::high_resolution_clock::now();
    for (int i = 0; i < TOTAL_INCREMENTS / 2; ++i) {
        c.increment();
    }
    // auto end = chrono::high_resolution_clock::now();
    // auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    // cout << "Thread " << tid << " took " << duration.count() << " milliseconds" << endl;
}

int main(void) {
    // create and start threads
    thread t0(threadFunc, 0);
    thread t1(threadFunc, 1);
    t0.join();
    t1.join();
    cout<<c.get()<<endl;
    return 0;
}
