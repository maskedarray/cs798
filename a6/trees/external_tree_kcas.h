#pragma once

#include <cassert>

/***CHANGE THIS VALUE TO YOUR LARGEST KCAS SIZE ****/
#define MAX_KCAS 6
/***CHANGE THIS VALUE TO YOUR LARGEST KCAS SIZE ****/

#include "../kcas/kcas.h"

enum DIRECTION {
    LEFT,
    RIGHT,
    NEITHER
};

using namespace std;
class ExternalKCAS {
private:    

	struct Node {
        int key;
        casword<Node *> left;
        casword<Node *> right;
        casword<bool> marked;

        
        bool isLeaf() {
            bool result = (left == NULL);
            assert(!result || right == NULL);
            return result;
        }
        bool isParentOf(Node * other) {
            return (left == other || right == other);
        }

    };

	struct SearchRecord {
        Node * gp;
        Node * p;
        Node * n;
        
        SearchRecord(Node * _gp, Node * _p, Node * _n)
        : gp(_gp), p(_p), n(_n) {
        }
    };

	volatile char padding0[PADDING_BYTES];
	const int numThreads;
	const int minKey;
	const int maxKey;
	volatile char padding1[PADDING_BYTES];
	Node * root;
    volatile char padding2[PADDING_BYTES];
public:
	ExternalKCAS(const int _numThreads, const int _minKey, const int _maxKey);
	~ExternalKCAS();
	long compareTo(int a, int b);
	bool contains(const int tid, const int & key);
	bool insertIfAbsent(const int tid, const int & key); // try to insert key; return true if successful (if it doesn't already exist), false otherwise
	bool erase(const int tid, const int & key); // try to erase key; return true if successful, false otherwise
    
	long getSumOfKeys(); // should return the sum of all keys in the set
	void printDebuggingDetails(); // print any debugging details you want at the end of a trial in this function

private:
	auto search(const int tid, const int & key);
    auto createInternal(int key, Node * left, Node * right);
    auto createLeaf(int key);
    void freeSubtree(const int tid, Node * node);
    long getSumOfKeysInSubtree(Node * node);

};

auto ExternalKCAS::createInternal(int key, Node * left, Node * right) {
    Node * node = new Node();
    node->key = key;
    node->left.setInitVal(left);
    node->right.setInitVal(right);
    node->marked.setInitVal(false);
    return node;
}

auto ExternalKCAS::createLeaf(int key) {
    return createInternal(key, NULL, NULL);
}

ExternalKCAS::ExternalKCAS(const int _numThreads, const int _minKey, const int _maxKey)
        : numThreads(_numThreads), minKey(_minKey), maxKey(_maxKey) {   
	auto rootLeft = createLeaf(minKey - 1);
    auto rootRight = createLeaf(maxKey + 1);
    root = createInternal(minKey - 1, rootLeft, rootRight);               
}

ExternalKCAS::~ExternalKCAS() {
	freeSubtree(0 /* dummy thread id */, root);
}

inline long ExternalKCAS::compareTo(int a, int b) {
    return ((long)a - (long)b); // casts to guarantee correct behaviour with large negative numbers
}

inline auto ExternalKCAS::search(const int tid, const int & key) {
    Node * gp;
    Node * p = NULL;
    Node * n = root;
    while (!n->isLeaf()) {
        gp = p;
        p = n;
        n = (key <= n->key) ? n->left : n->right;
    }
    return SearchRecord(gp, p, n);
}

bool ExternalKCAS::contains(const int tid, const int & key) { 
	assert(key <= maxKey);
    auto rec = search(tid, key);
    return (rec.n->key == key);
}

bool ExternalKCAS::insertIfAbsent(const int tid, const int & key) {
    assert(key <= maxKey);
    while (true) {
        auto ret = search(tid, key);
        auto dir = compareTo(key, ret.n->key);
        if (dir == 0) return false;
        // create two new nodes
        auto na = createLeaf(key);
        auto leftChild = (dir <= 0) ? na : ret.n;
        auto rightChild = (dir <= 0) ? ret.n : na;
        auto n1 = createInternal(std::min(key, ret.n->key), leftChild, rightChild);
        //atomic {
		kcas::start();
        // kcas::add();	// check marked
		if(ret.p->left == ret.n) {
			kcas::add(&ret.p->left, ret.n, n1,
					&ret.p->marked, false, false);		
			if (kcas::execute()) 	
				return true;
			else { // delete na and n1 and keep trying to insert
				delete na; delete n1;
			}		
		}
		else if (ret.p->right == ret.n) {
			kcas::add(&ret.p->right, ret.n, n1,
					&ret.p->marked, false, false);	
			if (kcas::execute()) 	
				return true;
			else { // delete na and n1 and keep trying to insert	
				delete na; delete n1;
			}	
		}
		else { //need to delete na and n1 and retry
			delete na; delete n1;
		}	
    }
}

bool ExternalKCAS::erase(const int tid, const int & key) {
    // return false;
    assert(key <= maxKey);
    while (true) {
        auto ret = search(tid, key);
        auto dir = compareTo(key, ret.n->key);
        if (dir != 0) return false;

        int gpLeft = (ret.gp->isParentOf(ret.p)? ((ret.gp->left == ret.p)? 1 : 0) : -1);
        int pLeft = (ret.p->isParentOf(ret.n)? ((ret.p->left == ret.n)? 1 : 0) : -1);

        // check for parent and grandparent
        // check for mark
        // perform swap based on left and right childs
        if (gpLeft != -1 && pLeft != -1) {
            bool nMark = ret.n->marked;     //mark is supposed to be false, convert it to true
	    	bool pMark = ret.p->marked;
			kcas::start();
           
	        Node * sibling;

            if (gpLeft == 1) {
                if (pLeft == 1) {
		    		sibling = ret.p->right;
                    kcas::add(
                        &ret.n->marked, false, true,        
                        &ret.p->marked, false, true,
                        &ret.gp->marked, false, false,
                        &ret.p->left, ret.n, ret.n,
                        &ret.gp->left, ret.p, sibling,
						&ret.p->right, sibling, sibling
                    );
                }
                else {
		    		sibling = ret.p->left;
                    kcas::add(
                        &ret.n->marked, false, true,        
                        &ret.p->marked, false, true,
                        &ret.gp->marked, false, false,
                        &ret.p->right, ret.n, ret.n,
                        &ret.gp->left, ret.p, sibling,
						&ret.p->left, sibling, sibling
                    );
                } 
            }
            else {
                if (pLeft == 1) {
		    		sibling = ret.p->right;
                    kcas::add(
                        &ret.n->marked, false, true,        
                        &ret.p->marked, false, true,
                        &ret.gp->marked, false, false,
                        &ret.p->left, ret.n, ret.n,
                        &ret.gp->right, ret.p, sibling,
						&ret.p->right, sibling, sibling
                    );
                }
                else {
		    		sibling = ret.p->left;
                    kcas::add(
                        &ret.n->marked, false, true,        
                        &ret.p->marked, false, true,
                        &ret.gp->marked, false, false,
                        &ret.p->right, ret.n, ret.n,
                        &ret.gp->right, ret.p, sibling,
						&ret.p->left, sibling, sibling
                    );
                } 
            }
            if (kcas::execute()) {
                return true;
            }
        }
    }
}

long ExternalKCAS::getSumOfKeysInSubtree(Node * node) {
	if (node == NULL) return 0;
    // only leaves contain real keys
    if (node->isLeaf()) {
        // and we must ignore dummy sentinel keys that are not in [minKey, maxKey]
        if (node->key >= minKey && node->key <= maxKey) {
            //std::cout<<"counting key "<<node->key;
            return node->key;
        } else {
            return 0;
        }
    } else {
        return getSumOfKeysInSubtree(node->left)
                + getSumOfKeysInSubtree(node->right);
    }
}

long ExternalKCAS::getSumOfKeys() {
	return getSumOfKeysInSubtree(root);
}

void ExternalKCAS::freeSubtree(const int tid, Node * node) {
    if (node == NULL) return;
    freeSubtree(tid, node->left);
    freeSubtree(tid, node->right);
    delete node;
}

void ExternalKCAS::printDebuggingDetails() {}
