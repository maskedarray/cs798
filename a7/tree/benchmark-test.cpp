#include <immintrin.h> 
#include "../common/util.h"

#include <thread>
#include <cstdlib>
#include <atomic>
#include <string>
#include <cstring>
#include <iostream>

#include "bronson_pext_bst_occ/adapter.h"   // competitor's tree
#include "binding.h"

 int main(){

    TryLock lock;

    int erase_retries = 40;
    bool result;
    int foo = 9;
    // int * bar;
erase_retry:
    if(_xbegin() == _XBEGIN_STARTED){
        // fast path here
        if (lock.isHeld()) _xabort(1);
        foo = foo *1074;
        // bar = new int[2000];
        // bar[0] = foo+1024;
        _xend();
    } else {
        while(lock.isHeld()){_mm_pause();}
        printf("Retrying transaction\n");
        if(erase_retries-- < 0){
            // fallback path here
            lock.acquire();
            printf("Transaction failed\n");
            // bar = new int[2000];
            // bar[0] = foo+1024;
            lock.release();
        } else {
            goto erase_retry;
        }
    }
    printf("completed %d, %d\n", foo);
    return 0;
 }
 
