#pragma once
#include "util.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <immintrin.h>
#include <cassert> 
using namespace std;

#define TIMEDEBUG 1

#if TIMEDEBUG == 1
#define START_TIMER std::chrono::high_resolution_clock::time_point starttime = std::chrono::high_resolution_clock::now();
#define NEND_TIMER std::chrono::high_resolution_clock::time_point endtime = std::chrono::high_resolution_clock::now(); 
#define END_TIMER std::chrono::high_resolution_clock::time_point endtime = std::chrono::high_resolution_clock::now();  \
                    printf("Total time taken : %d\n", endtime-starttime);
#else
#define START_TIMER
#define NEND_TIMER
#define END_TIMER 
#endif
    

atomic<int64_t> num_expansions =0;

#define CHUNK_SIZE 4096
class AlgorithmD {
private:
    enum {
        MARKED_MASK = (int) 0x80000000,     // most significant bit of a 32-bit key
        TOMBSTONE = (int) 0x7FFFFFFF,       // largest value that doesn't use bit MARKED_MASK
        EMPTY = (int) 0
    }; // with these definitions, the largest "real" key we allow in the table is 0x7FFFFFFE, and the smallest is 1 !!

    struct table {
        // data types
        char padding0[64];
        atomic<int> * data;
        char padding15[64];
        atomic<int> * old;
        int capacity;
        char padding13[64];
        int oldCapacity;
        char padding14[64];
        counter * approxSize;
        char padding12[64];
        counter * approxTombstone;
        char padding11[64];
        atomic<int> chunksClaimed;
        char padding1[64];
        atomic<int> chunksDone;
        char padding17[64];
        atomic<int> chunksInitClaimed;
        char padding16[64];
        atomic<int> chunksInitDone;
        char padding2[64];
        // constructor
        table(int _capacity, int old_capacity, atomic<int> * t_old, int num_threads):    
                                capacity(_capacity),
                                approxSize(new counter(num_threads)),
                                approxTombstone(new counter(num_threads)),
                                oldCapacity(old_capacity),
                                chunksInitClaimed(0),
                                chunksInitDone(0),
                                chunksClaimed(0),
                                chunksDone(0),
                                old(t_old) {
                                    data = new atomic<int>[_capacity];
                                    // for (int i=0; i< _capacity; ++i){
                                    //     data[i].store(EMPTY, memory_order_relaxed);
                                    // }
                                                         
                                }
        // destructor
        ~table() {delete data; delete approxSize; delete approxTombstone;}
    };


    
    bool expandAsNeeded(const int tid, table * t, int i);
    void helpExpansion(const int tid, table * t);
    void startExpansion(const int tid, table * t, int new_size);
    void migrate(const int tid, table * t, int myChunk);
    
    char padding0[PADDING_BYTES];
    int numThreads;
    int initCapacity;
    // more fields (pad as appropriate)
    char padding1[PADDING_BYTES];
    atomic<table *> currentTable;
    char padding2[PADDING_BYTES];
    
public:
    AlgorithmD(const int _numThreads, const int _capacity);
    ~AlgorithmD();
    bool insertIfAbsent(const int tid, const int & key, bool disableExpansion);
    bool erase(const int tid, const int & key);
    long getSumOfKeys();
    void printDebuggingDetails(); 
};

/**
 * constructor: initialize the hash table's internals
 * 
 * @param _numThreads maximum number of threads that will ever use the hash table (i.e., at least tid+1, where tid is the largest thread ID passed to any function of this class)
 * @param _capacity is the INITIAL size of the hash table (maximum number of elements it can contain WITHOUT expansion)
 */
AlgorithmD::AlgorithmD(const int _numThreads, const int _capacity)
: numThreads(_numThreads), initCapacity(_capacity) {
    int init_chunks = ceil((float)initCapacity/CHUNK_SIZE);
    currentTable = new table(_capacity, _capacity, NULL, numThreads);
    currentTable.load()->chunksClaimed=ceil((float)_capacity/CHUNK_SIZE);
    currentTable.load()->chunksDone=ceil((float)_capacity/CHUNK_SIZE);
    currentTable.load()->chunksInitClaimed=ceil((float)_capacity/CHUNK_SIZE);
    currentTable.load()->chunksInitDone=ceil((float)_capacity/CHUNK_SIZE);
    for (int i=0; i< _capacity; ++i){
        currentTable.load()->data[i].store(EMPTY, memory_order_relaxed);
    }
}

// destructor: clean up any allocated memory, etc.
AlgorithmD::~AlgorithmD() {
    delete currentTable;

}

bool AlgorithmD::expandAsNeeded(const int tid, table * t, int i) {
    helpExpansion(tid, t);
    if(t->approxSize->get() > t->capacity/2 ){
        // num_expansions++;
        // printf("Expanding due to capacity\n");
        int num_tombstones = t->approxTombstone->getAccurate();
        // int num_keys = t->approxSize->getAccurate() - num_tombstones;
        int new_capacity = (t->capacity - num_tombstones)* 2;
        startExpansion(tid, t, new_capacity);
        return true;
    } else if (i > 100 ){
            // printf("Expanding due to i: %d\n", i);
            int new_capacity = t->capacity;
            startExpansion(tid, t, new_capacity);
            return true;
    } else if (i > 30){
        if(t->approxSize->getAccurate() > t->capacity/2){
            // printf("Expanding due to i and cap: %d\n", i);
            int num_tombstones = t->approxTombstone->getAccurate();
            // int num_keys = t->approxSize->getAccurate() - num_tombstones;
            int new_capacity = (t->capacity - num_tombstones)* 2;
            startExpansion(tid, t, new_capacity);
            return true;
        }
    }
    return false;
}

void AlgorithmD::helpExpansion(const int tid, table * t) {
    int totalOldChunks = ceil((float)t->oldCapacity/CHUNK_SIZE);

    while(t->chunksInitClaimed.load() < totalOldChunks){
        int mychunk = t->chunksInitClaimed.fetch_add(1);
        // printf("%d starting with chunk: %d\n", tid, mychunk);
        if(mychunk < totalOldChunks){
            int start_index = mychunk * CHUNK_SIZE;
            int end_index = t->capacity;
            if((end_index-start_index) > CHUNK_SIZE)    end_index = start_index + CHUNK_SIZE;
            for (int i= start_index; i < end_index; i++){
                t->data[i].store(EMPTY, memory_order_relaxed);
            }
            t->chunksInitDone.fetch_add(1);
        }
    }

    while(t->chunksInitDone != totalOldChunks){
        _mm_pause;
        _mm_pause;
    }
    
    while(t->chunksClaimed.load() < totalOldChunks){
        int mychunk = t->chunksClaimed.fetch_add(1);
        // printf("%d starting with chunk: %d\n", tid, mychunk);
        if(mychunk < totalOldChunks){
            // m.lock();
            // printf("%d old table: \n", tid);
            // for (int i = 0; i< t->oldCapacity; i++){
            //     if(t->old[i] != TOMBSTONE){
            //         printf("%d , ", t->old[i].load() & ~MARKED_MASK);
            //     } else {
            //         printf("D , ");
            //     }
            // }
            // m.unlock();
            migrate(tid, t, mychunk);
            t->chunksDone.fetch_add(1);

            // m.lock();
            // printf("%d old table: \n", tid);
            // for (int i = 0; i< t->oldCapacity; i++){
            //     if(t->old[i] != TOMBSTONE){
            //         printf("%lld , ", t->old[i].load() & ~MARKED_MASK);
            //     } else {
            //         printf("D , ");
            //     }
            // }
            // printf("%d new table: \n", tid);
            // for (int i = 0; i< t->capacity; i++){
            //     if(t->data[i] != TOMBSTONE){
            //         printf("%d , ", t->data[i].load());
            //     } else {
            //         printf("10011 , ");
            //     }
            // }
            // m.unlock();
        }
    }

    while(t->chunksDone != totalOldChunks){
        _mm_pause;
        _mm_pause;
    }
    
}

void AlgorithmD::startExpansion(const int tid, table * t, int new_size) {
    table * tcurrent = currentTable.load();
    if (tcurrent == t){
        
    
        atomic<table *> t_new = new table(new_size, tcurrent->capacity ,tcurrent->data, numThreads); 
        if(!currentTable.compare_exchange_strong(tcurrent, t_new)){
            delete t_new;
        } else {
            // printf("%d New capacity: %d\n",tid, t_new.load()->capacity);
        }
        
    }
    helpExpansion(tid, currentTable);
}


void AlgorithmD::migrate(const int tid, table * t, int myChunk) {

    int start_index = myChunk * CHUNK_SIZE;
    int end_index = t->oldCapacity;
    if((end_index-start_index) > CHUNK_SIZE)    end_index = start_index + CHUNK_SIZE;
    
 
    // printf("Migrating chunk %d from %d to %d tid: %d\n",myChunk,start_index,end_index,tid);
    
    assert(start_index < t->oldCapacity);
    // assert(end_index <= 4096);
 
        
    //printf("Thread %d migrating chunk %d\n",tid,myChunk);
    
    for (int i = start_index; i < end_index; i++)
    {
        int currKey = t->old[i ];
        if (currKey == TOMBSTONE)   continue;
        while(!(t->old[i].compare_exchange_strong(currKey, currKey | MARKED_MASK))) {
            if (currKey == TOMBSTONE)   continue;
            // currKey = t->old[i ];
        }
        int value = t->old[i] & ~MARKED_MASK;
        if ((value != TOMBSTONE) && (value != EMPTY)) {
            assert(!(value & MARKED_MASK));
            insertIfAbsent(tid, value, true);
        }
    }
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmD::insertIfAbsent(const int tid, const int & key, bool disableExpansion = false) {
    uint32_t h = murmur3(key);
    table * t = currentTable.load();
    for (int i=0; i<t->capacity; ++i){
        if(!disableExpansion)
            if (expandAsNeeded(tid, t, i)) return insertIfAbsent(tid, key);

        assert(key != TOMBSTONE);
        assert(key > 0);
        int index = (h + i) % t->capacity;
        int value = t->data[index].load();
        if (value & MARKED_MASK)  {     // current value being inserted on is marked
             assert(!disableExpansion);  // disableExpansion should be true if someone is trying to insert on a marked key
             return insertIfAbsent(tid, key);
        }
        else if(value == key){
            // printf("%d key already present\n", tid);
            return false; 
        }
        else if(value == EMPTY){ 
            // printf("%d: empty space found\n", tid);
            if(t->data[index].compare_exchange_strong(value, key)){
                t->approxSize->inc(tid);
                // if(!disableExpansion){
                //     // printf("%d added %lld \n",tid, key);
                // }
                return true;
            }
            else {
                // value =t->data[index];
                if(value & MARKED_MASK) {
                    assert(!disableExpansion);
                    return insertIfAbsent(tid, key);
                }
                else if (value == key) return false;
            }
                
        }
    }
    assert(false); //table is full
    return false;
}

// semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmD::erase(const int tid, const int & key) {
    uint32_t h = murmur3(key);
    table * t = currentTable.load();
    for (int i = 0; i < t->capacity; i++) {
        if (expandAsNeeded(tid, t, i)) return erase(tid, key);
        int index = (h + i) % t->capacity;
        int value = t->data[index];
        if(value & MARKED_MASK) return erase(tid, key);
        
        else if(value == EMPTY) 
            return false;
        else if(value == key) {
            // printf("starting erase %d\n", t->data[index].load());
            if(t->data[index].compare_exchange_strong(value, TOMBSTONE)){
                t->approxTombstone->inc(tid);
                // printf("%d erased %lld \n", tid, key);
                return true;
            } else {
                assert(value == t->data[index]);
                // value = t->data[index];
                if(value & MARKED_MASK) return erase(tid, key);
                else if (value == TOMBSTONE)    return false;
                // Cannot come here. It means value is found to erase, but erase could not occur and neither the value is marked nor the value is tombstone (not possible)
                assert(false);
            }
        }
    }
    assert(false); //table is full
    return false;
}

// semantics: return the sum of all KEYS in the set
int64_t AlgorithmD::getSumOfKeys() {
    table * t = currentTable.load();
    int64_t sum = 0;
    for (int i=0; i< t->capacity; ++i){
        int a = t->data[i].load();
        if (a != TOMBSTONE)     sum+=a;
    }
    return sum;
}

// print any debugging details you want at the end of a trial in this function
void AlgorithmD::printDebuggingDetails() {

}
