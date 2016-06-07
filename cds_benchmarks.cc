#include "cds_benchmarks.hh"

std::atomic_int global_val(MAX_VALUE);
std::vector<int> sizes = {1000, 10000, 50000, 100000, 150000};
std::vector<int> nthreads_set = {1, 2, 4, 8, 12, 16, 20, 23};
txp_counter_type global_thread_push_ctrs[N_THREADS];
txp_counter_type global_thread_pop_ctrs[N_THREADS];
txp_counter_type global_thread_skip_ctrs[N_THREADS];
double global_thread_performance[N_THREADS];

std::atomic_int spawned(0);
std::mutex run_mtx;
std::mutex spawned_mtx;
std::condition_variable global_run_cv;
std::condition_variable global_spawned_cv;

// txn sets
std::vector<std::vector<op>> q_single_op_txn_set = {{push}, {pop}};
std::vector<std::vector<op>> q_push_only_txn_set = {{push}};
std::vector<std::vector<op>> q_pop_only_txn_set = {{pop}};
std::vector<std::vector<std::vector<op>>> q_txn_sets = 
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
    // 2. 100% include both pushes and ptps
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

void* record_perf_thread(void*) {
    int total1, total2;
    struct timeval tv1, tv2;
    float ops_per_ms = 0; 

    // wait for the threads to be spawned
    std::unique_lock<std::mutex> run_lk(run_mtx);
    global_run_cv.wait(run_lk);
    run_lk.unlock();

    struct timespec ts = {0, 100000};
    nanosleep(&ts, NULL);
    ts = {0, 10000};
    for (int i = 3000; i > 0; --i) {
        total1 = total2 = 0;
        gettimeofday(&tv1, NULL);
        for (int i = 0; i < N_THREADS; ++i) {
            total1 += (global_thread_push_ctrs[i] + global_thread_pop_ctrs[i]);
        }
        nanosleep(&ts, NULL);
        for (int i = 0; i < N_THREADS; ++i) {
            total2 += (global_thread_push_ctrs[i] + global_thread_pop_ctrs[i]);
        }
        gettimeofday(&tv2, NULL);
        float microseconds = ((tv2.tv_sec-tv1.tv_sec)*1000000.0) + ((tv2.tv_usec-tv1.tv_usec)/1000.0);
        ops_per_ms = (total2-total1)/microseconds > ops_per_ms ? (total2-total1)/microseconds : ops_per_ms;
    }
    fprintf(stderr, "\n");
    for (int i = 0; i < N_THREADS; ++i) {
        // prints the number of pushes, pops, and skips
        fprintf(stderr, "Thread %ld \tpushes: %ld \tpops: %ld, \tskips: %d\n", i, 
                global_thread_push_ctrs[i], 
                global_thread_pop_ctrs[i], 
                global_thread_skip_ctrs[i]);
    }
    fprintf(stderr, "\tops/ms: ");
    dualprint("%f, ", ops_per_ms);
    fprintf(stderr, "\n");
    return nullptr;
}

void clear_op_ctrs() {
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
