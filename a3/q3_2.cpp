#include <iostream>
#include <atomic>
#include <thread>
using namespace std;

#define TOTAL_INCREMENTS 100000000

__thread int tid;

class mutex_t {
private:
    atomic<bool> wants_to_enter[2];
    atomic<int> turn;
public:
    mutex_t(): turn(1) {
        wants_to_enter[0].store(false);
        wants_to_enter[1].store(false);  
    
    void lock() {
        wants_to_enter[tid].store(true);
        while(wants_to_enter[1 - tid].load() == true ) {
            if(turn.load() != tid){
                wants_to_enter[tid].store(false);
                while(turn.load() != tid){}
                wants_to_enter[tid].store(true);
            }
        } 
    }
    void unlock() {
        turn.store(1 - tid);
        wants_to_enter[tid].store(false);
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

int counts[2];
void threadFunc(int _tid) {
    tid = _tid;
    for (int i = 0; i < TOTAL_INCREMENTS / 2; ++i) {
        c.increment();
    }
}

int main(void) {
    // create and start threads
    counts[0]=0;
    counts[1]=0;
    thread t0(threadFunc, 0);
    thread t1(threadFunc, 1);
    t0.join();
    t1.join();
    cout<<c.get()<<endl;
    return 0;
}
