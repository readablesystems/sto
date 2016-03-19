#include <iostream>
#include <assert.h>
#include <stdio.h>

#include "Hashtable.hh"
#include "MassTrans.hh"
#include "Transaction.hh"
#include "simple_str.hh"
#include "randgen.hh"

#define DS 0
#define USE_STRINGS 1

#if DS == 0
#if USE_STRINGS == 1
typedef Hashtable<std::string, std::string, false, 1000000, simple_str> ds;
#else
typedef Hashtable<int, std::string, false, 1000000, simple_str> ds;
#endif
#else
typedef MassTrans<std::string> ds;
#endif

#define NTRANS 5000000
#define NINIT 100000
#define MAX_VALUE NINIT
using namespace std;

std::string value;


void run(ds& h) {
    std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
    for (int i = 0; i < NTRANS; ++i) {
        auto transseed = i;
        uint32_t seed = transseed*3;// + (uint32_t)me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
        auto seedlow = seed & 0xffff;
        auto seedhigh = seed >> 16;
        Rand transgen(seed, seedlow << 16 | seedhigh);
            
        int key = slotdist(transgen);
        std::string value;
#if DS == 0
        TRANSACTION{
#if USE_STRINGS == 1
            std::string s = std::to_string(key);
            h.transGet(s, value);
#else
	    h.transGet(key, value);
#endif
        } RETRY(false);
#else
        TRANSACTION{
#if USE_STRINGS == 1
            std::string s = std::to_string(key);
            h.transGet(s, value);
#else
            h.transGet(key, value);
#endif 
	} RETRY(false);
#endif
    }
    
}


void init(ds& h) {
    for (int i = 0; i < NINIT; ++i) {
        TRANSACTION{
#if USE_STRINGS == 1
            std::string s = std::to_string(i);
            h.transPut(s, value);
#else
	    h.transPut(i, value);
#endif
        } RETRY(false);
    }
    
}


void print_time(struct timeval tv1, struct timeval tv2) {
    printf("%f\n", (tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
}

int main() {
    value = std::string('a', 100);
    ds h;
#if DS != 0
    h.thread_init();
#endif
    
    init(h);
    struct timeval tv1,tv2;
    gettimeofday(&tv1, NULL);
    
    run(h);
    
    gettimeofday(&tv2, NULL);
    printf("Time taken: ");
    print_time(tv1, tv2);
}
