#pragma once
#include <immintrin.h>
#include <atomic>
#include "util.h"
#include "tle.h"
using namespace std;

// #define ___USE_OPENMP___

class TLEHashTableExpand {
private:
    enum {
        TOMBSTONE = (int) 0x7FFFFFFF,       // largest possible key is this minus one
        EMPTY = (int) 0                     // smallest possible key is this plus one
    };

    char padding0[PADDING_BYTES];
    ElapsedTimer debugTimer;                // just for debugging
    int numThreads;
    #ifndef ___USE_OPENMP___
    volatile int * data;
    char padding2[PADDING_BYTES];
    volatile int * old;
    #else
    atomic<int> * data;
    char padding2[PADDING_BYTES];
    atomic<int> * old;
    #endif
    int64_t capacity;
    int64_t oldCapacity;
    counter * approxInserts;                // only create ONCE in the constructor, then use set() to reset its value if needed
    char padding3[PADDING_BYTES];
    counter * approxDeletes;                // only create ONCE in the constructor, then use set() to reset its value if needed
    char padding1[PADDING_BYTES];

    inline bool isExpandNeeded(const int tid, int64_t probeCount, int* new_capacity);
    inline void expand(const int tid, TLEGuard *g, int* new_capacity);
    inline int64_t getAccurateSize();

public:
    TLEHashTableExpand(const int _numThreads, const int64_t _capacity);
    ~TLEHashTableExpand();
    bool insertIfAbsent(const int tid, const int & key);
    bool erase(const int tid, const int & key);
    long getSumOfKeys();
    void printDebuggingDetails();

private:
    inline bool insertIfAbsentSequential(const int tid, const int & key, TLEGuard *g, bool disableExpansion);
    inline bool insertIfAbsentCAS(const int tid, const int & key, TLEGuard *g, bool disableExpansion);
    inline bool eraseSequential(const int tid, const int & key,TLEGuard *g);
};

// _capacity is the INITIAL size of the hash table (maximum number of elements it can contain WITHOUT expansion)
TLEHashTableExpand::TLEHashTableExpand(const int _numThreads, const int64_t _capacity) {
    numThreads = _numThreads;
    capacity = _capacity;
    #ifndef ___USE_OPENMP___
    data = new int[capacity];
    #else
    data = new atomic<int>[capacity];
    #endif
    for (int64_t i=0;i<capacity;++i) data[i] = EMPTY;
    approxInserts = new counter(numThreads);
    approxDeletes = new counter(numThreads);
    old = NULL;
    oldCapacity = 0;
    debugTimer.startTimer();
}

TLEHashTableExpand::~TLEHashTableExpand() {
    delete[] data;
    // delete[] old;
    delete approxInserts;
    delete approxDeletes;
}

inline int64_t TLEHashTableExpand::getAccurateSize() {
    int64_t accurateInserts = approxInserts->getAccurate();
    int64_t accurateDeletes = approxDeletes->getAccurate();
    return accurateInserts - accurateDeletes;
}

inline bool TLEHashTableExpand::isExpandNeeded(const int tid, int64_t probeCount, int *new_capacity) {
    if ((approxInserts->get() > capacity/3)){
        *new_capacity = abs(capacity - approxDeletes->getAccurate())*2;
        return true;
    } else if (probeCount > 500){
        *new_capacity = capacity;
    }
    else if (probeCount > 100 && approxInserts->getAccurate() > capacity/3) {
        *new_capacity = abs(capacity - approxDeletes->getAccurate())*2;
        return true;
    } else {
        return false;
    }
}

inline void TLEHashTableExpand::expand(const int tid, TLEGuard *g, int *new_capacity) {
    int64_t expansionStartTime = debugTimer.getElapsedMillis();

    // EXPANSION CODE HERE :)

    int64_t accurateSize = approxInserts->getAccurate();
    accurateSize = accurateSize - approxDeletes->getAccurate();

    // int64_t new_capacity = capacity *2;
    #ifndef ___USE_OPENMP___
    volatile int * new_data;
    new_data = new int[*new_capacity];
    #else
    atomic<int> * new_data;
    new_data = new atomic<int>[*new_capacity];
    #endif
    
    
    
    old = data;
    data = new_data;
    oldCapacity = capacity;
    capacity = *new_capacity;

    #ifndef ___USE_OPENMP___
    for(int64_t i=0; i<*new_capacity; i++)   new_data[i]=0;
    #else
    #pragma omp parallel for num_threads(16) schedule(static)
    for(int64_t i=0; i<*new_capacity; i++)   new_data[i]=0;
    #endif
    
#ifndef ___USE_OPENMP___
    for(int64_t i=0; i<oldCapacity; i++){
        if(old[i] != EMPTY && old[i] != TOMBSTONE){
            int res = insertIfAbsentSequential(tid, (const int)old[i], g, true);
            // if(!res)    printf("insert in expand returned false, %d, %d\n", i, old[i]);
        }
    }
#else 
    #pragma omp parallel for num_threads(16) schedule(static)
    for(int64_t i=0; i<oldCapacity; i++){
        if(old[i] != EMPTY && old[i] != TOMBSTONE){
            int res = insertIfAbsentCAS(tid, (const int)old[i], g, true);
            // if(!res)    printf("insert in expand returned false\n");
        }
    }
#endif
    approxInserts->set(accurateSize);
    approxDeletes->set(0);
    
    g->explicit_fallback();
    delete[] old;
    // auto expansionEndTime = debugTimer.getElapsedMillis();
    // printf("tid=%d expansion at_ms=%ld duration_ms=%ld oldCapacity=%ld newCapacity=%ld\n", tid, expansionStartTime, (expansionEndTime - expansionStartTime), oldCapacity, capacity);
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool TLEHashTableExpand::insertIfAbsent(const int tid, const int & key) {
    bool result;
    TLEGuard g(tid);
    result = insertIfAbsentSequential(tid, key, (TLEGuard*)&g, false);
    g.explicit_commit();
    if(result) approxInserts->inc(tid);
    return result; 
}

// semantics: try to erase key. return true if successful, and false otherwise
bool TLEHashTableExpand::erase(const int tid, const int & key) {
    bool result;
    TLEGuard g(tid);
    result = eraseSequential(tid, key, (TLEGuard*)&g);
    g.explicit_commit();
    if(result) approxDeletes->inc(tid);
    return result;
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
inline bool TLEHashTableExpand::insertIfAbsentSequential(const int tid, const int & key, TLEGuard *g, bool disableExpansion = false) {
    uint32_t h = murmur3(key); 
    for (uint32_t i = 0; i < capacity; i++){
        if(!disableExpansion) { 
            int new_capacity;
            if(isExpandNeeded(tid, i, &new_capacity))  {
                expand(tid, g, &new_capacity);
                return false;
            }
        }
        uint32_t index = (h + i) % capacity;
        if(data[index] == key){
            return false;
        }
        else if(data[index] == EMPTY){ 
            data[index] = key;
            return true;
        }
    }
    return false;
}

// semantics: try to erase key. return true if successful, and false otherwise
inline bool TLEHashTableExpand::eraseSequential(const int tid, const int & key, TLEGuard *g) {
       uint32_t h = murmur3(key); 
    for (uint32_t i = 0; i < capacity; i++){
        int new_capacity;
        if(isExpandNeeded(tid, i, &new_capacity))  {
            expand(tid, g, &new_capacity);
            return false;
        }
        uint32_t index = (h + i) % capacity;
        if(data[index] == EMPTY){ 
            return false;
        }
        else if(data[index] == key){
            data[index] = TOMBSTONE;
            return true;
        }
    }
    return false;
}


// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
inline bool TLEHashTableExpand::insertIfAbsentCAS(const int tid, const int & key, TLEGuard *g, bool disableExpansion = false) {
    #ifdef ___USE_OPENMP___
    uint32_t h = murmur3(key); 
    for (uint32_t i = 0; i < capacity; i++){
        uint32_t index = (h + i) % capacity;
        int value = data[index];
        if(value == key){
            return false;
        }
        else if(value == EMPTY){ 
            if(data[index].compare_exchange_strong(value, key)){
                return true;
            }   else {
                return insertIfAbsentCAS(tid, key, g);
            }
        }
    }
    #endif
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
