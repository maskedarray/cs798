#pragma once
#include <immintrin.h>
#include <atomic>
#include "util.h"
#include "tle.h"
using namespace std;

#define MAX_RETRIES 40

struct nodeA {
    char padding0[PADDING_BYTES];
    uint32_t key;
    // mutex m;
    char padding1[PADDING_BYTES];
};


class TLEHashTableExpand {
private:
    enum {
        TOMBSTONE = (int) 0x7FFFFFFF,       // largest possible key is this minus one
        EMPTY = (int) 0                     // smallest possible key is this plus one
    };

    char padding0[PADDING_BYTES];
    ElapsedTimer debugTimer;                // just for debugging
    int numThreads;
    volatile int * data;
    volatile int * old;
    int64_t capacity;
    int64_t oldCapacity;
    counter * approxInserts;                // only create ONCE in the constructor, then use set() to reset its value if needed
    counter * approxDeletes;                // only create ONCE in the constructor, then use set() to reset its value if needed
    char padding1[PADDING_BYTES];
    TryLock lock;
    atomic<bool> isExpanding;

    bool isExpandNeeded(const int tid, int64_t probeCount);
    void expand(const int tid, TLEGuard *g);
    int64_t getAccurateSize();

public:
    TLEHashTableExpand(const int _numThreads, const int64_t _capacity);
    ~TLEHashTableExpand();
    bool insertIfAbsent(const int tid, const int & key);
    bool erase(const int tid, const int & key);
    long getSumOfKeys();
    void printDebuggingDetails();

private:
    bool insertIfAbsentSequential(const int tid, const int & key, TLEGuard *g, bool disableExpansion);
    bool eraseSequential(const int tid, const int & key, TLEGuard *g);
};

// _capacity is the INITIAL size of the hash table (maximum number of elements it can contain WITHOUT expansion)
TLEHashTableExpand::TLEHashTableExpand(const int _numThreads, const int64_t _capacity) {
    numThreads = _numThreads;
    capacity = _capacity;
    data = new volatile int[capacity];
    for (int64_t i=0;i<capacity;++i) data[i] = EMPTY;
    approxInserts = new counter(numThreads);
    approxDeletes = new counter(numThreads);
    old = NULL;
    oldCapacity = 0;
    debugTimer.startTimer();
    isExpanding.store(0);
}

TLEHashTableExpand::~TLEHashTableExpand() {
    delete[] data;
    delete[] old;
    delete approxInserts;
    delete approxDeletes;
}

int64_t TLEHashTableExpand::getAccurateSize() {
    int64_t accurateInserts = approxInserts->getAccurate();
    int64_t accurateDeletes = approxDeletes->getAccurate();
    return accurateInserts - accurateDeletes;
}

bool TLEHashTableExpand::isExpandNeeded(const int tid, int64_t probeCount) {
    return ((approxInserts->get() > capacity/3) ||
            (probeCount > 100 && approxInserts->getAccurate() > capacity/3));
}

void TLEHashTableExpand::expand(const int tid, TLEGuard *g) {
    int64_t expansionStartTime = debugTimer.getElapsedMillis();

    // EXPANSION CODE HERE :)
    // g->explicit_fallback();
    // lock.acquire();

    int64_t accurateSize = approxInserts->getAccurate();
    accurateSize = accurateSize - approxDeletes->getAccurate();

    int64_t new_capacity = capacity *2;
    volatile int * new_data;
    new_data = new int[new_capacity];
    for(int64_t i=0; i<new_capacity; i++)   new_data[i]=0;
    old = data;
    data = new_data;
    oldCapacity = capacity;
    capacity = new_capacity;
    approxInserts->set(accurateSize);
    approxDeletes->set(0);
    
    for(int64_t i=0; i<oldCapacity; i++){
            // uint32_t h = murmur3(data[i]); 
            // for (uint32_t j = 0; j < new_capacity; j++){
            //     uint32_t index = (h + j) % new_capacity;
            //     if(new_data[index] == data[i]){
            //         assert(false);
            //     }
            //     else if(new_data[index] == EMPTY){ 
            //         new_data[index] = data[i];
            //         break;
            //     }
            // }
            int res = insertIfAbsentSequential(tid, (const int)old[i], g, true);
            if(!res)    printf("insert in expand returned false\n\n");
        
    }
    g->explicit_fallback();

    // lock.release();
    // suggested adjustment to counters at the end of expansion:
     

    auto expansionEndTime = debugTimer.getElapsedMillis();
    printf("tid=%d expansion at_ms=%ld duration_ms=%ld oldCapacity=%ld newCapacity=%ld\n", tid, expansionStartTime, (expansionEndTime - expansionStartTime), oldCapacity, capacity);
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool TLEHashTableExpand::insertIfAbsent(const int tid, const int & key) {
    bool result;
    volatile __attribute_used__ TLEGuard g(tid);
    result = insertIfAbsentSequential(tid, key, (TLEGuard*)&g, false);
    if(result) approxInserts->inc(tid);
    return result; 
}

// semantics: try to erase key. return true if successful, and false otherwise
bool TLEHashTableExpand::erase(const int tid, const int & key) {
    bool result;
    volatile __attribute_used__ TLEGuard g(tid);
    result = eraseSequential(tid, key, (TLEGuard*)&g);
    if(result) approxDeletes->inc(tid);
    return result;
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool TLEHashTableExpand::insertIfAbsentSequential(const int tid, const int & key, TLEGuard *g, bool disableExpansion = false) {
    
    uint32_t h = murmur3(key); 
    for (uint32_t i = 0; i < capacity; i++){
        if(!disableExpansion) { if(isExpandNeeded(tid, i))  expand(tid, g);}
        uint32_t index = (h + i) % capacity;
		// data[index].m.lock(); 
        if(data[index] == key){
            // data[index].m.unlock();
            if(!disableExpansion) g->explicit_commit();
            return false;
        }
        else if(data[index] == EMPTY){ 
            data[index] = key;
            if(!disableExpansion) g->explicit_commit();
            // data[index].m.unlock();
            return true;
        }
        // data[index].m.unlock();
    }
    return false;
}

// semantics: try to erase key. return true if successful, and false otherwise
bool TLEHashTableExpand::eraseSequential(const int tid, const int & key, TLEGuard *g) {
       uint32_t h = murmur3(key); 
    for (uint32_t i = 0; i < capacity; i++){
        if(isExpandNeeded(tid, i))  expand(tid, g);
        uint32_t index = (h + i) % capacity;
        // data[index].m.lock(); 
        if(data[index] == EMPTY){ 
            // data[index].m.unlock();
            g->explicit_commit();
            return false;
        }
        else if(data[index] == key){
            data[index] = TOMBSTONE;
            g->explicit_commit();
            return true;
        }
        
        // data[index].m.unlock();
    }
    return false;
}


// semantics: return the sum of all KEYS in the set
int64_t TLEHashTableExpand::getSumOfKeys() {
    int64_t sum = 0;
    #pragma omp parallel for reduction(+: sum)
    for (int64_t i=0;i<capacity;i++) {
        if (data[i] != TOMBSTONE && data[i] != EMPTY) {
            sum += data[i]; // note: this line is correct without fetch&add ONLY because of the #pragma omp reduction above!
        }
    }
    return sum;
}

void TLEHashTableExpand::printDebuggingDetails() {}
