#ifndef COUNTERS_IMPL_H
#define COUNTERS_IMPL_H

#include <atomic>
#include <mutex>
#include <pthread.h>

#define MAX_THREADS 256

class CounterNaive {
private:
    char padding0[64];
    int64_t v;
    char padding1[64];
public:
    CounterNaive(int _numThreads) : v(0) {}
    int64_t inc(int tid) {
        return v++;
    }
    int64_t read() {
        return v;
    }
};

#define _USE_MUTEX      // Experimental results showed higher efficiency with mutex -> probably of way mutex is different from pthread spinlock, has more information
class CounterLocked {
private:
    #ifdef _USE_MUTEX
    std::mutex m;
    #else
    pthread_spinlock_t plock;
    #endif
    char padding[64];
    int64_t v;
    
public:
    CounterLocked(int _numThreads) { 
        v = 0;
        #ifndef _USE_MUTEX 
        pthread_spin_init(&plock, PTHREAD_PROCESS_PRIVATE); 
        #endif
    }
    int64_t inc(int tid) {
        #ifdef _USE_MUTEX
        m.lock();
        ++v;
        m.unlock();
        #else
        pthread_spin_lock(&plock);
        ++v;
        pthread_spin_unlock(&plock);
        #endif

        return v;
    }
    int64_t read() {
        
        #ifdef _USE_MUTEX
        m.lock();
        int64_t temp = v;
        m.unlock();
        #else
        pthread_spin_lock(&plock);
        int64_t temp = v;
        pthread_spin_unlock(&plock);
        #endif
        return temp;
    }
};

class CounterFetchAndAdd {
private:
    std::atomic<int64_t> v;
public:
    CounterFetchAndAdd(int _numThreads) { v = 0;}
    int64_t inc(int tid) {
        return v.fetch_add(1);
    }
    int64_t read() {
        return v;
    }
};


class CounterApproximate {
private:
    std::atomic<int64_t> globalCount;
    // int64_t localCount;                 // TODO: fix this
    int64_t threshold;
public:
    CounterApproximate(int _numThreads) {
        globalCount = 0;
        threshold   = 10000*_numThreads;
    }
    int64_t inc(int tid) {
        static __thread int64_t localCount = 0;
        if(++localCount >= threshold){
            globalCount.fetch_add(localCount);
            localCount = 0;
            return localCount;
        }   else {
            return localCount;
        }
    }
    int64_t read() {
        return globalCount;
    }
};

// pthread spinlock gives better performance than mutex, reason is there is one lock per thread. We do not necessarily need functionality provided by mutex
class CounterShardedLocked {
private:
    struct sh_locked {
        // std::mutex m;
        pthread_spinlock_t plock;
        char padding0[64];
        int64_t data;
        char padding1[64];
        // char padding[64-sizeof(int64_t)-sizeof(std::mutex)];
    };
    sh_locked sh[MAX_THREADS];
    int __numThreads;
public:
    CounterShardedLocked(int _numThreads) {
        for(int i=0; i<_numThreads; i++){
            sh[i].data = 0;
            pthread_spin_init(&sh[i].plock, PTHREAD_PROCESS_PRIVATE); 
        }
        
        __numThreads = _numThreads;
    }
    int64_t inc(int tid) {
        pthread_spin_lock(&sh[tid].plock);
        sh[tid].data++;
        pthread_spin_unlock(&sh[tid].plock);
        return 0;
    }
    int64_t read() {
        int64_t thread_local sum=0;
        for(int i=0; i<__numThreads; i++){
            // sh[i].m.lock();
            pthread_spin_lock(&sh[i].plock);
            sum+= sh[i].data;
        }
        for(int i=0; i<__numThreads; i++){
            // sh[i].m.unlock();
            pthread_spin_unlock(&sh[i].plock);
        }
        return sum;
    }
};


class CounterShardedWaitfree {
private:
    struct sh_lf {
        std::atomic<int64_t> data;
        char padding1[64];
    };
    sh_lf sh[MAX_THREADS];
    int __numThreads;
public:
    CounterShardedWaitfree(int _numThreads) {
        for(int i=0; i<_numThreads; i++){
            sh[i].data.store(0, std::memory_order_relaxed);
        }
        __numThreads = _numThreads;
    }
    int64_t inc(int tid) {
        sh[tid].data.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }
    int64_t read() {
        int64_t sum=0;
        for (int i=0; i<__numThreads; ++i){
            sum+=sh[i].data.load(std::memory_order_relaxed);
        }
        return sum;
    }
};

#endif
