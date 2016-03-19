#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>
#include <map>
#include "Transaction.hh"
#include "Vector.hh"
#include "clp.h"
#include "randgen.hh"

int max_value = 10000;
int global_seed = 11;
int nthreads = 16;
int ntrans = 1000000;
int opspertrans = 1;
int prepopulate = 1000;
int max_key = 1000;
double search_percent = 0.3;
double update_percent = 0.3;
bool use_iterators = false;


TransactionTid::type lock;

typedef Vector<int> data_structure;

template <typename T>
struct TesterPair {
    T* t;
    int me;
};


template <typename T>
int findK(T* q, int val) {
    if (use_iterators) {
        data_structure::iterator fit = q->begin();
        data_structure::iterator eit = q->end();
        data_structure::iterator it = std::find(fit, eit, val);
        return it-fit;
        
    } else {
        int first = 0;
        int last = q->size();
        while (first != last) {
            if (q->transGet(first) == val) return first;
            ++first;
        }
        return last;
    }
}

template <typename T>
void run(T* q, int me) {
    TThread::set_id(me);
    
    std::uniform_int_distribution<long> slotdist(0, max_value);
    int N = ntrans/nthreads;
    int OPS = opspertrans;

    for (int i = 0; i < N; ++i) {
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
                if (op < search_percent * 100) {
                    int val = slotdist(transgen);
                    findK(q, val);
                } else if (op < (search_percent + update_percent) *100){
                    int key = slotdist(transgen) % max_key;
                    int val = slotdist(transgen) % max_value;
                    q->transUpdate(key, val);
                } else {
                    int key = slotdist(transgen) % max_key;
                    q->transGet(key);
                }

            }
            
            if (Sto::try_commit()) {
                break;
            }
        
        } catch (Transaction::Abort e) { }
        }
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
    opt_nthreads, opt_ntrans, opt_opspertrans, opt_searchpercent, opt_updatepercent, opt_prepopulate, opt_seed, opt_useiterators
};

static const Clp_Option options[] = {
    { "nthreads", 0, opt_nthreads, Clp_ValInt, Clp_Optional },
    { "ntrans", 0, opt_ntrans, Clp_ValInt, Clp_Optional },
    { "opspertrans", 0, opt_opspertrans, Clp_ValInt, Clp_Optional },
    { "searchpercent", 0, opt_searchpercent, Clp_ValDouble, Clp_Optional },
    { "updatepercent", 0, opt_updatepercent, Clp_ValDouble, Clp_Optional },
    { "prepopulate", 0, opt_prepopulate, Clp_ValInt, Clp_Optional },
    { "seed", 0, opt_seed, Clp_ValInt, Clp_Optional },
    { "useiterators", 0, opt_useiterators, Clp_ValInt, Clp_Optional}
};

static void help() {
    printf("Usage: [OPTIONS]\n\
           Options:\n\
           --nthreads=NTHREADS (default %d)\n\
           --ntrans=NTRANS, how many total transactions to run (they'll be split between threads) (default %d)\n\
           --opspertrans=OPSPERTRANS, how many operations to run per transaction (default %d)\n\
           --searchpercent=SEARCHPERCENT, probability with which to do searches (default %f)\n\
           --updatepercent=UPDATEPERCENT, probability with which to do updates (default %f)\n\
           --prepopulate=PREPOPULATE, prepopulate table with given number of items (default %d)\n\
           --seed=SEED, global seed to run the experiment \n",
            nthreads, ntrans, opspertrans, search_percent, update_percent, prepopulate);
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
            q->push_back(slotdist(transgen));
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
            case opt_searchpercent:
                search_percent = clp->val.d;
                break;
            case opt_updatepercent:
                update_percent = clp->val.d;
                break;
            case opt_prepopulate:
                prepopulate = clp->val.i;
                break;
            case opt_seed:
                global_seed = clp->val.i;
                break;
            case opt_useiterators:
                use_iterators = clp->val.i == 1;
                break;
            default:
                help();
        }
    }
    Clp_DeleteParser(clp);

    if (!use_iterators) {
    // Run a parallel test with lots of transactions doing pushes and pops
    data_structure q;
    init(&q);
    
    gettimeofday(&tv1, NULL);
    
    startAndWait(nthreads, &q);
    
    gettimeofday(&tv2, NULL);
    printf("Parallel PQ time: ");
    print_time(tv1, tv2);
    
#if STO_PROFILE_COUNTERS
    Transaction::print_stats();
    {
        txp_counters tc = Transaction::txp_counters_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
    Transaction::clear_stats();
#endif
    } else {
    
    data_structure q2;
    init(&q2);
    gettimeofday(&tv1, NULL);
    
    startAndWait(nthreads, &q2);
    
    gettimeofday(&tv2, NULL);
    printf("Iterator parallel time: ");
    print_time(tv1, tv2);
    
#if STO_PROFILE_COUNTERS
    Transaction::print_stats();
    {
        txp_counters tc = Transaction::txp_counters_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
    Transaction::clear_stats();
#endif
   }    
	return 0;
}
