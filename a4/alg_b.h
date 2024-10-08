#pragma once
#include "util.h"
#include <atomic>
#include <mutex>
using namespace std;

struct nodeB {
    char padding0[PADDING_BYTES];
    atomic<uint32_t> key;
    mutex m;
    char padding1[PADDING_BYTES];
};

class AlgorithmB {
public:
    static constexpr int TOMBSTONE = -1;

    char padding0[PADDING_BYTES];
    const int numThreads;
    int capacity;
    char padding2[PADDING_BYTES];
    nodeB * data;

    AlgorithmB(const int _numThreads, const int _capacity);
    ~AlgorithmB();
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
AlgorithmB::AlgorithmB(const int _numThreads, const int _capacity)
: numThreads(_numThreads), capacity(_capacity) {
    data = new nodeB[capacity];
    for (int i = 0; i < _capacity; i++)
        data[i].key = 0; 
    
}

// destructor: clean up any allocated memory, etc.
AlgorithmB::~AlgorithmB() {
    delete data;
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmB::insertIfAbsent(const int tid, const int & key){
    uint32_t h = murmur3(key); 
    for (uint32_t i=0; i<capacity; i++){
        uint32_t index = (h+i) % capacity;
        uint32_t value = data[index].key;
        
        if (value == 0){ 
            data[index].m.lock();
            value = data[index].key;        
            if (value == 0){
                data[index].key = key;                          //Looks like LP for true
                data[index].m.unlock();
                return true;
            }
            if (value == key){
                data[index].m.unlock();
                return false;
            }
            data[index].m.unlock();
        }
        else if (value == key){
            return false; 
        }
    }
    return false;
}

// semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmB::erase(const int tid, const int & key) {
    uint32_t h = murmur3(key); 
    for (uint32_t i = 0; i < capacity; i++){
        uint32_t index = (h + i) % capacity;
        uint32_t value = data[index].key;
        if(value == key) {
            data[index].m.lock(); 
            value = data[index].key;
            if(value == key){                            // looks like LP
                data[index].key = TOMBSTONE;
                data[index].m.unlock();
                return true;
            }
            if(value == 0){
                data[index].m.unlock();
                return false;
            }
            data[index].m.unlock();
        }

        else if(value == 0) {
            return false; 
        }
    }
    return false;
}

// semantics: return the sum of all KEYS in the set
int64_t AlgorithmB::getSumOfKeys() {
    int64_t sum = 0;
	for (int i = 0; i < capacity; i++){
        if(!(data[i].key == TOMBSTONE)) 
		    sum += data[i].key;
    }
	return sum;
}

// print any debugging details you want at the end of a trial in this function
void AlgorithmB::printDebuggingDetails() {
    
}
