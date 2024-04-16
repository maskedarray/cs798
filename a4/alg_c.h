#pragma once
#include "util.h"
#include <atomic>
using namespace std;

struct nodeC {
    char padding0[PADDING_BYTES];
    atomic<uint32_t> key;
    char padding1[PADDING_BYTES];
};


class AlgorithmC {
public:
    static constexpr int TOMBSTONE = -1;

    char padding0[PADDING_BYTES];
    const int numThreads;
    int capacity;
    char padding2[PADDING_BYTES];
    nodeC * data;

    AlgorithmC(const int _numThreads, const int _capacity);
    ~AlgorithmC();
    bool insertIfAbsent(const int tid, const int & key);
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
AlgorithmC::AlgorithmC(const int _numThreads, const int _capacity)
: numThreads(_numThreads), capacity(_capacity) {
    data = new nodeC[capacity];
    for (int i = 0; i < _capacity; i++)
        data[i].key = 0; 

}

// destructor: clean up any allocated memory, etc.
AlgorithmC::~AlgorithmC() {
    delete data;
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmC::insertIfAbsent(const int tid, const int & key) {
    uint32_t h = murmur3(key); 
    for (uint32_t i = 0; i < capacity; i++) {
        uint32_t index = (h + i) % capacity;
        uint32_t value = data[index].key;
        if(value == key){
            return false; 
        }
        if(value == 0){ 
            if(data[index].key.compare_exchange_strong(value, key))
                return true;
            else if (data[index].key == key)
                return false;
        }
    }
    return false;
}

// semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmC::erase(const int tid, const int & key) {
    uint32_t h = murmur3(key);
    for (uint32_t i = 0; i < capacity; i++) {
        uint32_t index = (h + i) % capacity;
        uint32_t value = data[index].key;
        if(value == 0) 
            return false;
        else if(value == key) 
            return data[index].key.compare_exchange_strong(value, TOMBSTONE);
    }
    return false;
}

// semantics: return the sum of all KEYS in the set
int64_t AlgorithmC::getSumOfKeys() {
        int64_t sum = 0;
	for (int i = 0; i < capacity; i++){
        if(!(data[i].key == TOMBSTONE)) 
		    sum += data[i].key;
    }
	return sum;
}

// print any debugging details you want at the end of a trial in this function
void AlgorithmC::printDebuggingDetails() {
    
}
