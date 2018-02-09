#define TVECTOR_DEFAULT_CAPACITY 16384
#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>
#include <map>
#include <queue>
#include "Transaction.hh"
#include "TVector.hh"
#include "TVector_nopred.hh"
#include "PriorityQueue.hh"
#include "PriorityQueue1.hh"
#include "clp.h"
#include "randgen.hh"
int waiting = 5000;
int max_value = 100000;
int global_seed = 0;
int nthreads = 4;
int opspertrans = 1;
int prepopulate = 1000;
double push_percent = 0.75;
int blocks = 1000;
int runtime = 10;
unsigned initial_seeds[128];

volatile bool running = true;

#define SMALLER_WRITES 0
#define SINGLE_POP 0
#define FIX_RUNTIME 1
#if FIX_RUNTIME
int ntrans = 1000000;
#else
int ntrans = 300000;
#endif

TransactionTid::type lock;

template <typename T>
struct TesterPair {
    T* t;
    int me;
    uint64_t num_trans;
};

template <typename T>
uint64_t run(T* q, int me) {
    TThread::set_id(me);
#if SMALLER_WRITES
    std::uniform_int_distribution<long> slotdist(0, max_value/blocks);
#else
    std::uniform_int_distribution<long> slotdist(0, max_value);
#endif
    Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
    uint64_t num_trans = 0;
#if !FIX_RUNTIME
    int N = ntrans/nthreads;
#endif
    int OPS = opspertrans;
    int ratio = ( push_percent / (1 - push_percent));
    if (ratio < 1) ratio = 1;
    bool only_pop = false;
    bool only_push = false;
#if SINGLE_POP
    if (me == 0) only_pop = true;
    else only_push = true;
#endif

#if FIX_RUNTIME
    int i = -1;
    while(running) {
    i++;
#else
    for (int i = 0; i < N; ++i) {
#endif
        // so that retries of this transaction do the same thing
        Rand transgen_snap = transgen;
        while (1) {
            Sto::start_transaction();
            try {
                for (int j = 0; j < OPS; ++j) {
                    int op = slotdist(transgen) % 100;
                    if (only_push || (!only_pop && op < push_percent * 100)) {
                            int val = slotdist(transgen);
#if SMALLER_READS
                            val = val + (1 - (i/N))*max_value;
#endif
                            q->push(val);
                    } else {
                        for (int r = 0; r < ratio; r++) {
                            try {
                                q->pop();
                            } catch (std::out_of_range e) {
                                // gross but whatever
                            }
                        }
                    }
                }
 
                if (Sto::try_commit()) {
#if FIX_RUNTIME
                    num_trans++;
#endif
                    break;
                }
        
            } catch (Transaction::Abort e) { }
            transgen = transgen_snap;
        }
        /* Waiting time */
        for (int k = 0; k < waiting; k++) {__asm__ __volatile__("");}
    }
    return num_trans;
}

template <typename T>
void* runFunc(void* x) {
    TesterPair<T>* tp = (TesterPair<T>*) x;
    uint64_t num_trans = run(tp->t, tp->me);
    tp->num_trans = num_trans;
    return nullptr;
}

template <typename T>
uint64_t startAndWait(int n, T* queue) {
    running = true;
    pthread_t tids[nthreads];
    TesterPair<T> *testers = new TesterPair<T>[n];
    for (int i = 0; i < nthreads; ++i) {
        testers[i].t = queue;
        testers[i].me = i;
        pthread_create(&tids[i], NULL, runFunc<T>, &testers[i]);
    }
#if FIX_RUNTIME
    sleep(runtime);
    printf("stopping...\n");
    running = false;
#endif 
    __sync_synchronize();
    for (int i = 0; i < nthreads; ++i) {
        pthread_join(tids[i], NULL);
    }

    uint64_t tot_trans = 0;
    for (int i = 0; i < nthreads; ++i) {
       tot_trans += testers[i].num_trans; 
    }
    return tot_trans;
}

void print_time(struct timeval tv1, struct timeval tv2) {
    printf("%f\n", (tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
}

enum {
    opt_nthreads = 1, opt_ntrans, opt_opspertrans, opt_pushpercent, opt_toppercent, opt_prepopulate, opt_seed
};

static const Clp_Option options[] = {
    { "nthreads", 0, opt_nthreads, Clp_ValInt, Clp_Optional },
    { "ntrans", 0, opt_ntrans, Clp_ValInt, Clp_Optional },
    { "opspertrans", 0, opt_opspertrans, Clp_ValInt, Clp_Optional },
    { "pushpercent", 0, opt_pushpercent, Clp_ValDouble, Clp_Optional },
    { "toppercent", 0, opt_toppercent, Clp_ValDouble, Clp_Optional },
    { "prepopulate", 0, opt_prepopulate, Clp_ValInt, Clp_Optional },
    { "seed", 0, opt_seed, Clp_ValInt, Clp_Optional }
};

static void help() {
    printf("Usage: [OPTIONS]\n\
           Options:\n\
           --nthreads=NTHREADS (default %d)\n\
           --ntrans=NTRANS, how many total transactions to run (they'll be split between threads) (default %d)\n\
           --opspertrans=OPSPERTRANS, how many operations to run per transaction (default %d)\n\
           --pushpercent=PUSHPERCENT, probability with which to do pushes (default %f)\n\
           --prepopulate=PREPOPULATE, prepopulate table with given number of items (default %d)\n\
           --seed=SEED, global seed to run the experiment \n",
            nthreads, ntrans, opspertrans, push_percent, prepopulate);
    exit(1);
}

template <typename T>
void init(T* q) {
    std::uniform_int_distribution<long> slotdist(0, max_value);
    Rand transgen(random(), random());
    for (int i = 0; i < prepopulate; i++) {
        TRANSACTION {
            int val = slotdist(transgen);
#if SMALLER_WRITES
            val = val + max_value*(1+blocks)/blocks;
#endif
            q->push(val);
        } RETRY(false);
    }
}

template <typename T>
void run_and_report(const char* name) {
    T* q;
    TRANSACTION {
        q = new T;
    } RETRY(true);
    init(q);

    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);
    unsigned long long num_trans = startAndWait(nthreads, q);
    gettimeofday(&tv2, NULL);

#if FIX_RUNTIME
    double time = tv2.tv_sec - tv1.tv_sec + (tv2.tv_usec-tv1.tv_usec)/1000000.0;
    unsigned long long throughput = num_trans/time;
    printf("%s: %llu\n", name, throughput);
#else
    printf("%s time: ", name);
    print_time(tv1, tv2);
#endif
#if STO_PROFILE_COUNTERS
    Transaction::print_stats();
    {
        txp_counters tc = Transaction::txp_counters_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
    Transaction::clear_stats();
#endif
}

int main(int argc, char *argv[]) {
    lock = 0;
    Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);
    std::vector<const char*> tests;
        
    int opt;
    while ((opt = Clp_Next(clp)) != Clp_Done) {
        switch (opt) {
            case opt_nthreads:
                nthreads = clp->val.i;
                break;
            case opt_ntrans:
                ntrans = clp->val.i;
                break;
            case opt_opspertrans:
                opspertrans = clp->val.i;
                break;
            case opt_pushpercent:
                push_percent = clp->val.d;
                break;
            case opt_prepopulate:
                prepopulate = clp->val.i;
                break;
            case opt_seed:
                global_seed = clp->val.i;
                break;
            case Clp_NotOption:
                tests.push_back(clp->vstr);
                break;
            default:
                help();
        }
    }
    Clp_DeleteParser(clp);

    if (tests.empty()) {
        tests.push_back("PQ");
        tests.push_back("it");
    }

    if (global_seed)
        srandom(global_seed);
    else
        srandomdev();
    for (unsigned i = 0; i < arraysize(initial_seeds); ++i)
        initial_seeds[i] = random();

    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);

    // Run a parallel test with lots of transactions doing pushes and pops
    for (auto test : tests) {
        if (strcmp(test, "PQ") == 0 || strcmp(test, "pq") == 0)
            run_and_report<PriorityQueue<int>>("PQ");
        else if (strcmp(test, "PQ1") == 0 || strcmp(test, "pq1") == 0 || strcmp(test, "it") == 0)
            run_and_report<PriorityQueue1<int>>("PQ1");
        else if (strcmp(test, "std") == 0)
            run_and_report<std::priority_queue<int, TVector<int>>>("std");
        else if (strcmp(test, "std-nopred") == 0)
            run_and_report<std::priority_queue<int, TVector_nopred<int>>>("std-nopred");
        else
            assert(false);
    }

    return 0;
}
