#include <iostream>
#include <assert.h>
#include <random>
#include <climits>
#include <sys/time.h>
#include <sys/resource.h>

#include "Transaction.hh"
#include "MassTrans.hh"
#include "MassTrans_nd.hh"
#incldue "clp.h"


int nthreads = 16;
int ntrans = 1000000;
int nitems = 10;
int maxBid = 1000;
int opspertrans = 1;
typedef int value_type;
typedef MassTrans<value_type> type;

type* db;

int main(int argc, char *argv[]) {
    
    Transaction::epoch_advance_callback = [] (unsigned) {
        // just advance blindly because of the way Masstree uses epochs
        globalepoch++;
    };
    
    db = new type();
    type::thread_init();
    
    // Prepopulate items with bid 0
    for (int i = 0; i < nitems; i++) {
        Transaction t;
        db->transWrite(t, i, 0);
        t.commit();
    }
    
    struct timeval tv1,tv2;
    struct rusage ru1,ru2;
    gettimeofday(&tv1, NULL);
    getrusage(RUSAGE_SELF, &ru1);
    startAndWait();
    gettimeofday(&tv2, NULL);
    getrusage(RUSAGE_SELF, &ru2);
    
    printf("real time: ");
    print_time(tv1,tv2);
    
    printf("utime: ");
    print_time(ru1.ru_utime, ru2.ru_utime);
    printf("stime: ");
    print_time(ru1.ru_stime, ru2.ru_stime);
    
#if PERF_LOGGING
    Transaction::print_stats();
    {
        using thd = threadinfo_t;
        thd tc = Transaction::tinfo_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
#endif
    
    // Validation
    
}

void startAndWait() {
    pthread_t tids[nthreads];
    for (int i = 0; i < nthreads; i++) {
        int id = i;
        pthread_create(&tids[i], NULL, run, &id);
    }
    
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);
    
    for (int i = 0; i < n; ++i) {
        pthread_join(tids[i], NULL);
    }
}

void* run(void* x) {
    int id = (*(int*) x);
    Transaction::threadid = me;
    
    type::thread_init();
    
    int N = ntrans/nthreads;
    int OPS = opspertrans;
    for(int i = 0; i < N; ++i) {
        
        bool done = false;
        while(!done) {
            try {
                uint32_t seed = i*3 + (uint32_t)me*N*7;
                srand(seed);
                
                Transaction& t = Transaction::get_transaction();
                for (int i = 0; i < N; i++) {
                    int item = rand() % nitems;
                    int bid = rand() % maxBid;
                    int curMax = db->transRead(t, item);
                    if (bid > curMax) {
                        db->transWrite(t, item, bid);
                    }
                }
                dome = t.try_commit();
            }catch (Transction::Abort E) {}
            
        }
    }
    
    return nullptr;
}