#pragma once

#include <cassert>
#include "recordmgr/record_manager.h"
#include "kcas.h"

class DoublyLinkedListReclaim {
private:
    volatile char padding0[PADDING_BYTES];
    const int numThreads;
    const int minKey;
    const int maxKey;
    volatile char padding1[PADDING_BYTES];

    KCASLockFree<5> kcas;

    struct Node {
        casword_t prev; 
        casword_t next;
        int data;
        casword_t mark;
    };

    Node* head;
    Node* tail;

    simple_record_manager<Node>* recmgr;

public:
    DoublyLinkedListReclaim(const int _numThreads, const int _minKey, const int _maxKey);
    ~DoublyLinkedListReclaim();
    
    bool contains(const int tid, const int & key);
    bool insertIfAbsent(const int tid, const int & key); // try to insert key; return true if successful (if it doesn't already exist), false otherwise
    bool erase(const int tid, const int & key); // try to erase key; return true if successful, false otherwise
    
    long getSumOfKeys(); // should return the sum of all keys in the set
    void printDebuggingDetails(); // print any debugging details you want at the end of a trial in this function
};

DoublyLinkedListReclaim::DoublyLinkedListReclaim(const int _numThreads, const int _minKey, const int _maxKey)
        : numThreads(_numThreads), minKey(_minKey), maxKey(_maxKey) {
    // it may be useful to know about / use the "placement new" operator (google)
    // because the simple_record_manager::allocate does not take constructor arguments
    // ... placement new essentially lets you call a constructor after an object already exists.

    head = new Node();

    kcas.writeInitPtr(0, &head->next, (casword_t) NULL);
    kcas.writeInitPtr(0, &head->prev, (casword_t) NULL);
    head->data = minKey;
    kcas.writeInitVal(0, &head->mark, (casword_t) false);

    tail = new Node();
    kcas.writeInitPtr(0, &tail->next, (casword_t) NULL);
    kcas.writeInitPtr(0, &tail->prev, (casword_t) NULL);
    tail->data = maxKey + 1;
    kcas.writeInitVal(0, &tail->mark, (casword_t) false);
    
    auto descPtrHead = kcas.getDescriptor(0);
    descPtrHead->addPtrAddr(&head->next, (casword_t) NULL, (casword_t) tail);
    if(!kcas.execute(0, descPtrHead)) {
        assert(false);
    }

    auto descPtrTail = kcas.getDescriptor(0);
    descPtrTail->addPtrAddr(&tail->prev, (casword_t) NULL, (casword_t) head);
    if(!kcas.execute(0, descPtrTail)) {
        assert(false); //checking return value
    }

    recmgr = new simple_record_manager<Node>(numThreads);

}

DoublyLinkedListReclaim::~DoublyLinkedListReclaim() {
    recmgr->getGuard(0);
    Node* currentNode = head;
    currentNode = (Node*) kcas.readPtr(0, &currentNode->next);
    while(currentNode != tail){
        Node* nextNode = (Node*) kcas.readPtr(0, &currentNode->next);
        recmgr->deallocate(0, currentNode);
        currentNode = nextNode;
    }
    delete head;
    delete tail;
    delete recmgr;
}

bool DoublyLinkedListReclaim::contains(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);

    recmgr->getGuard(tid);

    Node* currentNode = head;
    while (key > currentNode->data) { //iterate until key is greater than node and keep moving to next node. End at tail.
        currentNode = (Node*) kcas.readPtr(tid, &currentNode->next);
    };
    if (key == currentNode->data) {
        return true;
    }
    return false;
}

bool DoublyLinkedListReclaim::insertIfAbsent(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
    recmgr->getGuard(tid);      // get guard for reclaim manager
    Node* currentNode = head;
    while (key > currentNode->data ) {
        currentNode = (Node*) kcas.readPtr(tid, &currentNode->next); 
    };
    if (key == currentNode->data) { // check if key found is present
        return false; 
    }

    Node* prevNode = (Node*) kcas.readPtr(tid, &currentNode->prev);
    // Node* newNode = new Node();
    Node* newNode = recmgr->allocate<Node>(tid);

    kcas.writeInitPtr(tid, &newNode->next, (casword_t) currentNode);
    kcas.writeInitPtr(tid, &newNode->prev, (casword_t) prevNode);
    newNode->data = key;
    kcas.writeInitVal(tid, &newNode->mark, (casword_t) false);
    
    auto descPtr = kcas.getDescriptor(tid);
    descPtr->addValAddr(&prevNode->mark, false, false);
    descPtr->addValAddr(&currentNode->mark, false, false);
    descPtr->addPtrAddr(&prevNode->next, (casword_t) currentNode,(casword_t) newNode);
    descPtr->addPtrAddr(&currentNode->prev, (casword_t) prevNode, (casword_t) newNode);

    if (kcas.execute(tid, descPtr)) {
        return true;
    }
    else {
        // delete newNode;
        recmgr->deallocate(tid, newNode);
        return false;
    }
    return false;
}

bool DoublyLinkedListReclaim::erase(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
    recmgr->getGuard(tid);
    Node* currentNode = head;
    while (key >= currentNode->data) {
        if (key == currentNode->data) {

            Node* prevNode = (Node*) kcas.readPtr(tid, &currentNode->prev);
            Node* nextNode = (Node*) kcas.readPtr(tid, &currentNode->next);
            
            auto descPtr = kcas.getDescriptor(tid);
            descPtr->addValAddr(&prevNode->mark, false, false);
            descPtr->addValAddr(&currentNode->mark, false, true);
            descPtr->addPtrAddr(&nextNode->mark, false, false);;
            descPtr->addPtrAddr(&prevNode->next, (casword_t) currentNode, (casword_t) nextNode);
            descPtr->addPtrAddr(&nextNode->prev, (casword_t) currentNode, (casword_t) prevNode);

            if (kcas.execute(tid, descPtr)) {   
                recmgr->retire(tid, currentNode);
                return true;    
            }
            return false;
        }
        currentNode = (Node*) kcas.readPtr(tid, &currentNode->next);
    };
    return false;
}

long DoublyLinkedListReclaim::getSumOfKeys() {
    recmgr->getGuard(0);
    Node* currentNode = head;
    long sum = 0;
    while(currentNode->data < tail->data) {
        sum += currentNode->data;
        currentNode = (Node*) kcas.readPtr(0, &currentNode->next);
    };
    return sum;
}

void DoublyLinkedListReclaim::printDebuggingDetails() {
    
}
