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

int max_value = 100000;
int global_seed = 11;
int nthreads = 4;
int ntrans = 100000;
int opspertrans = 1;
int prepopulate = 10000;
double push_percent = 0.75;

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
};

template <typename T>
void run_serial(T* q, int n) {
    std::uniform_int_distribution<long> slotdist(0, max_value);
    int N = ntrans/nthreads;
    int OPS = opspertrans;
    int ratio = ( push_percent / (1 - push_percent));
    if (ratio < 1) ratio = 1;
   
    for (int i = 0; i < N; ++i) {
        for (int k = 0; k < n; k++) {
            int me = k;
            // so that retries of this transaction do the same thing
            auto transseed = i;
            Sto::start_transaction();
            try {
                uint32_t seed = transseed*3 + (uint32_t)me*ntrans*7 + (uint32_t)global_seed*MAX_THREADS*ntrans*11;
                auto seedlow = seed & 0xffff;
                auto seedhigh = seed >> 16;
                Rand transgen(seed, seedlow << 16 | seedhigh);

                for (int j = 0; j < OPS; ++j) {
                    int op = slotdist(transgen) % 100;
                    if (op < push_percent * 100) {
                        int val = slotdist(transgen);
                        q->push(val);
                    } else {
                        for (int r = 0; r < ratio; r++) {
                            q->top();
                            q->pop();
                        }
                    }
                }

                /* Waiting time */
                for (int i = 0; i < 10000; i++) {asm("");}

                if (Sto::try_commit()) {
                    break;
                }

            } catch (Transaction::Abort e) {
            }
        }
    }
}

template <typename T>
void run(T* q, int me) {
    Transaction::threadid = me;
    
    std::uniform_int_distribution<long> slotdist(0, max_value);
    int N = ntrans/nthreads;
    int OPS = opspertrans;
    int ratio = ( push_percent / (1 - push_percent));
    if (ratio < 1) ratio = 1;

    for (int i = 0; i < N; ++i) {
        // so that retries of this transaction do the same thing
        auto transseed = i;
        Sto::start_transaction();
        try {
            uint32_t seed = transseed*3 + (uint32_t)me*ntrans*7 + (uint32_t)global_seed*MAX_THREADS*ntrans*11;
            auto seedlow = seed & 0xffff;
            auto seedhigh = seed >> 16;
            Rand transgen(seed, seedlow << 16 | seedhigh);
            
            for (int j = 0; j < OPS; ++j) {
                int op = slotdist(transgen) % 100;
                if (op < push_percent * 100) {
                    int val = slotdist(transgen);
                    q->push(val);
                } else {
                    for (int r = 0; r < ratio; r++) {
                        q->top();
                        q->pop();
                    }
                }

            }
            
            /* Waiting time */
            for (int i = 0; i < 10000; i++) {asm("");}

            if (Sto::try_commit()) {
                break;
            }
        
        } catch (Transaction::Abort e) { }
    }
}

template <typename T>
void* runFunc(void* x) {
    TesterPair<T>* tp = (TesterPair<T>*) x;
    run(tp->t, tp->me);
    return nullptr;
}

template <typename T>
void startAndWait(int n, T* queue) {
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
    
    for (int i = 0; i < nthreads; ++i) {
        pthread_join(tids[i], NULL);
    }
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
            q->push(slotdist(transgen));
        } RETRY(false);
    }
}

int main(int argc, char *argv[]) {
    lock = 0;
    struct timeval tv1,tv2;
    
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
    
    startAndWait(nthreads, &q);
    
    gettimeofday(&tv2, NULL);
    printf("Parallel PQ time: ");
    print_time(tv1, tv2);
    
#if PERF_LOGGING
    Transaction::print_stats();
    {
        using thd = threadinfo_t;
        thd tc = Transaction::tinfo_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
    Transaction::clear_stats();
#endif
    
    data_structure q1;
    init(&q1);
    gettimeofday(&tv1, NULL);
    
    run_serial(&q1, nthreads);
    
    gettimeofday(&tv2, NULL);
    printf("Serial PQ time: ");
    print_time(tv1, tv2);
    
    Transaction::clear_stats();
    
    PriorityQueue1<int> q2;
    init(&q2);
    gettimeofday(&tv1, NULL);
    
    startAndWait(nthreads, &q2);
    
    gettimeofday(&tv2, NULL);
    printf("Iterator parallel time: ");
    print_time(tv1, tv2);
    
#if PERF_LOGGING
    Transaction::print_stats();
    {
        using thd = threadinfo_t;
        thd tc = Transaction::tinfo_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
    Transaction::clear_stats();
#endif
    
    PriorityQueue1<int> q3;
    init(&q3);
    gettimeofday(&tv1, NULL);
    
    run_serial(&q3, nthreads);
    
    gettimeofday(&tv2, NULL);
    printf("Serial iterator time: ");
    print_time(tv1, tv2);
    
	return 0;
}
