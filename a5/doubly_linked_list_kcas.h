#pragma once

#include <cassert>
#include "kcas.h"

class DoublyLinkedList {
private:
    volatile char padding0[PADDING_BYTES];
    const int numThreads;
    const int minKey;
    const int maxKey;
    volatile char padding1[PADDING_BYTES];

    KCASLockFree<5> kcas;
    volatile char padding2[PADDING_BYTES];

    struct Node {
        volatile char padding3[PADDING_BYTES];
        casword_t prev; 
        casword_t next;
        int data;
        casword_t mark;
        volatile char padding4[PADDING_BYTES];
    };

    Node* head;
    // volatile char padding5[PADDING_BYTES];
    Node* tail;

public:
    DoublyLinkedList(const int _numThreads, const int _minKey, const int _maxKey);
    ~DoublyLinkedList();
    
    bool contains(const int tid, const int & key);
    bool insertIfAbsent(const int tid, const int & key); // try to insert key; return true if successful (if it doesn't already exist), false otherwise
    bool erase(const int tid, const int & key); // try to erase key; return true if successful, false otherwise
    
    long getSumOfKeys(); // should return the sum of all keys in the set
    void printDebuggingDetails(); // print any debugging details you want at the end of a trial in this function



    void initNode(int tid, Node* node, Node* p, Node* n, int d) {
        kcas.writeInitPtr(tid, &node->next, (casword_t) n);
        kcas.writeInitPtr(tid, &node->prev, (casword_t) p);
        node->data = d;
        kcas.writeInitVal(tid, &node->mark, (casword_t) false);
    }
};

DoublyLinkedList::DoublyLinkedList(const int _numThreads, const int _minKey, const int _maxKey)
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

}

DoublyLinkedList::~DoublyLinkedList() {
    delete head;
    delete tail;
}

bool DoublyLinkedList::contains(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);

    Node* currentNode = head;
    while (key > currentNode->data) { //iterate until key is greater than node and keep moving to next node. End at tail.
        currentNode = (Node*) kcas.readPtr(tid, &currentNode->next);
    };
    if (key == currentNode->data) {
        return true;
    }
    return false;
}

bool DoublyLinkedList::insertIfAbsent(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);

    Node* currentNode = head;
    while (key > currentNode->data ) {
        currentNode = (Node*) kcas.readPtr(tid, &currentNode->next); 
    };
    if (key == currentNode->data) { // check if key found is present
        return false; 
    }

    Node* prevNode = (Node*) kcas.readPtr(tid, &currentNode->prev);
    Node* newNode = new Node();

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
        delete newNode;
        return false;
    }

    return false;
}

bool DoublyLinkedList::erase(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);

 
    Node* succ = head;
    while (key >= succ->data) {
        if (key == succ->data) {

            Node* pred = (Node*) kcas.readPtr(tid, &succ->prev);
            Node* after = (Node*) kcas.readPtr(tid, &succ->next);
            
            auto descPtr = kcas.getDescriptor(tid);
            descPtr->addValAddr(&pred->mark, false, false);
            descPtr->addValAddr(&succ->mark, false, true);
            descPtr->addPtrAddr(&after->mark, false, false);;
            descPtr->addPtrAddr(&pred->next, (casword_t) succ, (casword_t) after);
            descPtr->addPtrAddr(&after->prev, (casword_t) succ, (casword_t) pred);

            if (kcas.execute(tid, descPtr)) {   
                return true;    
            }
            return false;
        }
        succ = (Node*) kcas.readPtr(tid, &succ->next);
    };

    return false;   // key not found in list
}

long DoublyLinkedList::getSumOfKeys() {
    Node* currentNode = head;
    long sum = 0;
    while(currentNode->data < tail->data) {
        sum += currentNode->data;
        currentNode = (Node*) kcas.readPtr(0, &currentNode->next);
    };
    return sum;
}

void DoublyLinkedList::printDebuggingDetails() {
    
}
