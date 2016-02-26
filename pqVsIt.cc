#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>
#include <map>
#include "Transaction.hh"
#include "Vector.hh"
#include "PriorityQueue.hh"
#include "PriorityQueue1.hh"
#include "clp.h"
int waiting = 5000;
int max_value = 100000;
int global_seed = 11;
int nthreads = 4;
int opspertrans = 1;
int prepopulate = 1000;
double push_percent = 0.75;
int blocks = 1000;
int runtime = 10;

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

typedef PriorityQueue<int> data_structure;

struct Rand {
    typedef uint32_t result_type;
    
    result_type u, v;
    Rand(result_type u, result_type v) : u(u|1), v(v|1) {}
    
    inline result_type operator()() {
        v = 36969*(v & 65535) + (v >> 16);
        u = 18000*(u & 65535) + (u >> 16);
        return (v << 16) + u;
    }
    
    static constexpr result_type max() {
        return (uint32_t)-1;
    }
    
    static constexpr result_type min() {
        return 0;
    }
};

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
    uint64_t num_trans = 0;
    int N = ntrans/nthreads;
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
        auto transseed = i;
        while (1) {
        Sto::start_transaction();
        try {
            uint32_t seed = transseed*3 + (uint32_t)me*ntrans*7 + (uint32_t)global_seed*MAX_THREADS*ntrans*11;
            auto seedlow = seed & 0xffff;
            auto seedhigh = seed >> 16;
            Rand transgen(seed, seedlow << 16 | seedhigh);
            
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
                        q->pop();
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
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);
#if FIX_RUNTIME
    sleep(runtime);
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
    opt_nthreads, opt_ntrans, opt_opspertrans, opt_pushpercent, opt_toppercent, opt_prepopulate, opt_seed
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
    uint32_t seed = global_seed * 11;
    auto seedlow = seed & 0xffff;
    auto seedhigh = seed >> 16;
    Rand transgen(seed, seedlow << 16 | seedhigh);
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

int main(int argc, char *argv[]) {
    lock = 0;
    struct timeval tv1,tv2;
    uint64_t num_trans;
    uint64_t time;
    uint64_t throughput;
    
    Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);
        
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
            default:
                help();
        }
    }
    Clp_DeleteParser(clp);

    // Run a parallel test with lots of transactions doing pushes and pops
    data_structure q;
    init(&q);
    
    gettimeofday(&tv1, NULL);
    
    num_trans = startAndWait(nthreads, &q);
    
    gettimeofday(&tv2, NULL);
#if FIX_RUNTIME
    time = tv2.tv_sec - tv1.tv_sec + (tv2.tv_usec-tv1.tv_usec)/1000000.0;
    throughput = num_trans/time;
    printf("PQ: %llu\n", throughput);
#else
    printf("Parallel PQ time: ");
    print_time(tv1, tv2);
#endif
    
#if PERF_LOGGING
    Transaction::print_stats();
    {
        using thd = threadinfo_t;
        thd tc = Transaction::tinfo_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time), pop_aborts: %llu, push_aborts: %llu\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts), tc.p(txp_pop_abort), tc.p(txp_push_abort));
    }
    Transaction::clear_stats();
#endif
    
    PriorityQueue1<int> q2;
    init(&q2);
    gettimeofday(&tv1, NULL);
    
    num_trans = startAndWait(nthreads, &q2);
    
    gettimeofday(&tv2, NULL);
#if FIX_RUNTIME
    time = tv2.tv_sec - tv1.tv_sec + (tv2.tv_usec-tv1.tv_usec)/1000000.0;
    throughput = num_trans/time;
    printf("it: %llu\n", throughput);
#else
    printf("Parallel iterator time: ");
    print_time(tv1, tv2);
#endif
#if PERF_LOGGING
    Transaction::print_stats();
    {
        using thd = threadinfo_t;
        thd tc = Transaction::tinfo_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
    Transaction::clear_stats();
#endif
    
	return 0;
}
