#include <string>
#include <iostream>
#include <cstdarg>
#include <assert.h>
#include <vector>
#include <random>

#include "cds_benchmarks.hh"

std::atomic_int global_val(MAX_VALUE);
std::vector<int> sizes = {10000, 50000, 100000, 150000};
std::vector<int> nthreads_set = {1, 2, 4, 8, 12, 16, 20, 24};
txp_counter_type global_thread_push_ctrs[N_THREADS];
txp_counter_type global_thread_pop_ctrs[N_THREADS];
txp_counter_type global_thread_skip_ctrs[N_THREADS];

// txn sets
std::vector<std::vector<op>> q_single_op_txn_set = {{push}, {pop}};
std::vector<std::vector<op>> q_push_only_txn_set = {{push}};
std::vector<std::vector<op>> q_pop_only_txn_set = {{push}};
std::vector<std::vector<op>> q_txn_sets[] = 
{
    // 0. short txns
    {
        {push, push, push},
        {pop, pop, pop},
        {pop}, {pop}, {pop},
        {push}, {push}, {push}
    },
    // 1. longer txns
    {
        {push, push, push, push, push},
        {pop, pop, pop, pop, pop},
        {pop}, {pop}, {pop}, {pop}, {pop}, 
        {push}, {push}, {push}, {push}, {push}
    },
    // 2. 100% include both pushes and pops
    {
        {push, push, pop},
        {pop, pop, push},
    },
    // 3. 50% include both pushes and pops
    {
        {push, push, pop},
        {pop, pop, push},
        {pop}, {push}
    },
    // 4. 33% include both pushes and pops
    {
        {push, push, pop},
        {pop, pop, push},
        {pop}, {pop},
        {push}, {push}
    },
    // 5. 33%: longer push + pop txns
    {
        {push, pop, push, pop, push, pop},
        {pop}, 
        {push}
    }
};

void clear_balance_ctrs() {
    for(int i = 0; i < N_THREADS; ++i) {
        global_thread_pop_ctrs[i] = 0;
        global_thread_push_ctrs[i] = 0;
        global_thread_skip_ctrs[i] = 0;
    }
}

void dualprint(const char* fmt,...) {
    va_list args1, args2;
    va_start(args1, fmt);
    va_start(args2, fmt);
    vfprintf(stderr, fmt, args1);
    vfprintf(stdout, fmt, args2);
    va_end(args1);
    va_end(args2);
}

void print_stats(struct timeval tv1, struct timeval tv2, int bm) {
    float seconds = (tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0;
    //dualprint("\t%f", seconds); 

    int n_threads;
    if (bm == NOABORTS) n_threads = 2;
    else n_threads = N_THREADS;
    int total = 0;
    for (int i = 0; i < n_threads; ++i) {
        fprintf(stderr, "Thread %ld \tpushes: %d \tpops: %ld, \tskips: %ld\n",
                i, global_thread_push_ctrs[i], 
                global_thread_pop_ctrs[i], global_thread_skip_ctrs[i]);
        total += global_thread_push_ctrs[i] + global_thread_pop_ctrs[i];
    }
    //dualprint("\tops/ms: %f\n", total/(seconds*1000));
    dualprint("\t%f, ", total/(seconds*1000));
}

void print_abort_stats() {
#if STO_PROFILE_COUNTERS
    if (txp_count >= txp_total_aborts) {
        txp_counters tc = Transaction::txp_counters_combined();

        unsigned long long txc_total_starts = tc.p(txp_total_starts);
        unsigned long long txc_total_aborts = tc.p(txp_total_aborts);
        unsigned long long txc_commit_aborts = tc.p(txp_commit_time_aborts);
        unsigned long long txc_total_commits = txc_total_starts - txc_total_aborts;
        fprintf(stderr, "\t$ %llu starts, %llu max read set, %llu commits",
                txc_total_starts, tc.p(txp_max_set), txc_total_commits);
        if (txc_total_aborts) {
            fprintf(stderr, ", %llu (%.3f%%) aborts",
                    tc.p(txp_total_aborts),
                    100.0 * (double) tc.p(txp_total_aborts) / tc.p(txp_total_starts));
            if (tc.p(txp_commit_time_aborts))
                fprintf(stderr, "\n$ %llu (%.3f%%) of aborts at commit time",
                        tc.p(txp_commit_time_aborts),
                        100.0 * (double) tc.p(txp_commit_time_aborts) / tc.p(txp_total_aborts));
        }
        unsigned long long txc_commit_attempts = txc_total_starts - (txc_total_aborts - txc_commit_aborts);
        fprintf(stderr, "\n\t$ %llu commit attempts, %llu (%.3f%%) nonopaque\n",
                txc_commit_attempts, tc.p(txp_commit_time_nonopaque),
                100.0 * (double) tc.p(txp_commit_time_nonopaque) / txc_commit_attempts);
    }
    Transaction::clear_stats();
#endif
}

template <typename T>
void* run_sto(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    TThread::set_id(tp->me);

    for (int i = 0; i < NTRANS; ++i) {
        auto transseed = i;
        uint32_t seed = transseed*3 + (uint32_t)tp->me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
        auto seedlow = seed & 0xffff;
        auto seedhigh = seed >> 16;
        Rand transgen(seed, seedlow << 16 | seedhigh);

        // so that retries of this transaction do the same thing
        Rand transgen_snap = transgen;
        while (1) {
            Sto::start_transaction();
            try {
                do_txn(tp, transgen);
                if (Sto::try_commit()) {
                    break;
                }
            } catch (Transaction::Abort e) {}
            transgen = transgen_snap;
        }
    }
    return nullptr;
}

template <typename T>
void* run_cds(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    cds::threading::Manager::attachThread();

    // generate all transactions the thread will run
    for (int i = 0; i < NTRANS; ++i) {
        auto transseed = i;
        uint32_t seed = transseed*3 + (uint32_t)tp->me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
        auto seedlow = seed & 0xffff;
        auto seedhigh = seed >> 16;
        Rand transgen(seed, seedlow << 16 | seedhigh);
        do_txn(tp, transgen);
    }
    cds::threading::Manager::detachThread();
    return nullptr;
}

template <typename T>
void startAndWait(T* ds, int ds_type, 
        int bm, 
        std::vector<std::vector<op>> txn_set, 
        size_t size,
        int nthreads) {

    pthread_t tids[nthreads];
    Tester<T> testers[nthreads];
    for (int i = 0; i < nthreads; ++i) {
        // set the txn_set to be only pushes or pops if running the NOABORTS bm
        if (bm == NOABORTS) {
            testers[0].txn_set = q_push_only_txn_set;
            testers[1].txn_set = q_pop_only_txn_set;
        }
        else testers[i].txn_set = txn_set;
        testers[i].ds = ds;
        testers[i].me = i;
        testers[i].bm = bm;
        testers[i].size = size;
        if (ds_type == CDS) {
            pthread_create(&tids[i], NULL, run_cds<T>, &testers[i]);
        } else {
            pthread_create(&tids[i], NULL, run_sto<T>, &testers[i]);
        }
    }
    for (int i = 0; i < nthreads; ++i) {
        pthread_join(tids[i], NULL);
    }

    if (bm == PUSHTHENPOP_RANDOM || bm == PUSHTHENPOP_DECREASING) { 
        while (ds->size() != 0) {
    	    Sto::start_transaction();
            ds->pop();
	    // so that totals are correct
            global_thread_pop_ctrs[0]++;
            assert(Sto::try_commit());
        }
    }
}

