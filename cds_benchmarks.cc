#include "cds_benchmarks.hh"

/*
 * Tester defines the test parameters 
 * and the Test object to run the test
 */

struct Tester {
    int me; // tid
    int nthreads;
    size_t init_size;
    GenericTest* test;   
};

/*
 * Threads to run the tests and record performance
 */
void* test_thread(void *data) {
    cds::threading::Manager::attachThread();

    GenericTest *gt = ((Tester*)data)->test;
    int me = ((Tester*)data)->me;
    int nthreads = ((Tester*)data)->nthreads;
    size_t init_size = ((Tester*)data)->init_size;

    TThread::set_id(me);

    if (me == INITIAL_THREAD) {
        gt->cleanup();
        gt->initialize(init_size); 
    }
    
    spawned_barrier++;
    while (spawned_barrier != nthreads) {
        sched_yield();
    }
   
    gt->run(me);

    spawned_barrier = 0;

    cds::threading::Manager::detachThread();
    return nullptr;
}

void* record_perf_thread(void* x) {
    int nthreads = *(int*)x;
    int total1, total2;
    struct timeval tv1, tv2;
    double ops_per_ms = 0; 

    while (spawned_barrier != nthreads) {
        sched_yield();
    }

    // benchmark until the first thread finishes
    gettimeofday(&tv1, NULL);
    total1 = total2 = 0;
    for (int i = 0; i < MAX_NUM_THREADS; ++i) {
        total1 += (global_thread_ctrs[i].push + global_thread_ctrs[i].pop);
    }
    while (spawned_barrier == nthreads) {
        sched_yield();
    }
    for (int i = 0; i < MAX_NUM_THREADS; ++i) {
        total2 += (global_thread_ctrs[i].push + global_thread_ctrs[i].pop);
    }
    gettimeofday(&tv2, NULL);
    double milliseconds = ((tv2.tv_sec-tv1.tv_sec)*1000.0) + ((tv2.tv_usec-tv1.tv_usec)/1000.0);
    ops_per_ms = (total2-total1)/milliseconds > ops_per_ms ? (total2-total1)/milliseconds : ops_per_ms;
    dualprintf("%f, ", ops_per_ms);
    return nullptr;
}

void startAndWait(GenericTest* test, size_t size, int nthreads) {
    // create performance recording thread
    pthread_t recorder;
    pthread_create(&recorder, NULL, record_perf_thread, &nthreads);
    // create threads to run the test
    pthread_t tids[nthreads];
    Tester testers[nthreads];
    for (int i = 0; i < nthreads; ++i) {
        testers[i].me = i;
        testers[i].test = test;
        testers[i].init_size = size;
        testers[i].nthreads = nthreads;
        pthread_create(&tids[i], NULL, test_thread, &testers[i]);
    }
    for (int i = 0; i < nthreads; ++i) {
        pthread_join(tids[i], NULL);
    }
    pthread_join(recorder, NULL);

    fprintf(global_verbose_stats_file, "\n");
    for (int i = 0; i < nthreads; ++i) {
        // prints the number of pushes, pops, and skips
        fprintf(global_verbose_stats_file, "Thread %d \tpushes: %ld \tpops: %ld, \tskips: %ld\n", i, 
                global_thread_ctrs[i].push, 
                global_thread_ctrs[i].pop, 
                global_thread_ctrs[i].skip);
        global_thread_ctrs[i].push = 0;
        global_thread_ctrs[i].pop = 0;
        global_thread_ctrs[i].skip = 0;
    }
    print_abort_stats();
}

void dualprintf(const char* fmt,...) {
    va_list args1, args2;
    va_start(args1, fmt);
    va_start(args2, fmt);
    vfprintf(global_verbose_stats_file, fmt, args1);
    vfprintf(global_stats_file, fmt, args2);
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
        fprintf(global_verbose_stats_file, "\t$ %llu starts, %llu max read set, %llu commits",
                txc_total_starts, tc.p(txp_max_set), txc_total_commits);
        if (txc_total_aborts) {
            fprintf(global_verbose_stats_file, ", %llu (%.3f%%) aborts",
                    tc.p(txp_total_aborts),
                    100.0 * (double) tc.p(txp_total_aborts) / tc.p(txp_total_starts));
            if (tc.p(txp_commit_time_aborts))
                fprintf(global_verbose_stats_file, "\n$ %llu (%.3f%%) of aborts at commit time",
                        tc.p(txp_commit_time_aborts),
                        100.0 * (double) tc.p(txp_commit_time_aborts) / tc.p(txp_total_aborts));
        }
        unsigned long long txc_commit_attempts = txc_total_starts - (txc_total_aborts - txc_commit_aborts);
        fprintf(global_verbose_stats_file, "\n\t$ %llu commit attempts, %llu (%.3f%%) nonopaque\n",
                txc_commit_attempts, tc.p(txp_commit_time_nonopaque),
                100.0 * (double) tc.p(txp_commit_time_nonopaque) / txc_commit_attempts);
    }
    Transaction::clear_stats();
#endif
}

#define MAKE_PQUEUE_TESTS(desc, test, type, ...) \
    {desc, "STO pqueue", new test<DatatypeHarness<PriorityQueue<type>>>(STO, ## __VA_ARGS__)},      \
    {desc, "STO pqueue opaque", new test<DatatypeHarness<PriorityQueue<type, true>>>(STO, ## __VA_ARGS__)},\
    {desc, "MS pqueue", new test<DatatypeHarness<cds::container::MSPriorityQueue<type>>>(CDS, ## __VA_ARGS__)},\
    {desc, "FC pqueue", new test<DatatypeHarness<cds::container::FCPriorityQueue<type>>>(CDS, ## __VA_ARGS__)}, \
    {desc, "FC pairing heap pqueue", new test<DatatypeHarness<cds::container::FCPriorityQueue<type, PairingHeap<type>>>>(CDS, ## __VA_ARGS__)} 
struct Test {
    std::string desc;
    const char* ds;
    GenericTest* test;
} pqueue_tests[] = {
    //MAKE_PQUEUE_TESTS("PQRandSingleOps:R", RandomSingleOpTest, int, RANDOM_VALS),
    //MAKE_PQUEUE_TESTS("PQRandSingleOps:D", RandomSingleOpTest, int, DECREASING_VALS),
    //MAKE_PQUEUE_TESTS("PQPushPop:R", PushPopTest, int, RANDOM_VALS),
    //MAKE_PQUEUE_TESTS("PQPushPop:D", PushPopTest, int, DECREASING_VALS),
    //MAKE_PQUEUE_TESTS("PQPushOnly:R", SingleOpTest, int, RANDOM_VALS, push),
    //MAKE_PQUEUE_TESTS("PQPushOnly:D", SingleOpTest, int, DECREASING_VALS, push),
    //MAKE_PQUEUE_TESTS("General Txns Test with Random Vals", GeneralTxnsTest, int, RANDOM_VALS, q_txn_sets[0]),
    //MAKE_PQUEUE_TESTS("General Txns Test with Decreasing Vals", GeneralTxnsTest, int, DECREASING_VALS, q_txn_sets[0]),
};
int num_pqueues = 5;

#define MAKE_QUEUE_TESTS(desc, test, type, ...) \
    {desc, "STO queue", new test<DatatypeHarness<Queue<type>>>(STO, ## __VA_ARGS__)},                                  \
    {desc, "STO queue2", new test<DatatypeHarness<Queue2<type>>>(STO, ## __VA_ARGS__)},                                  \
    {desc, "FC queue", new test<DatatypeHarness<cds::container::FCQueue<type>>>(CDS, ## __VA_ARGS__)},                 \
    {desc, "STO/FC queue", new test<DatatypeHarness<FCQueue<type>>(STO, ## __VA_ARGS__)}
/*
    {desc, "Basket queue", new test<DatatypeHarness<cds::container::BasketQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)},         \
    {desc, "Moir queue", new test<DatatypeHarness<cds::container::MoirQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)}, \
    {desc, "MS queue", new test<DatatypeHarness<cds::container::MSQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)},   \
    {desc, "Opt queue", new test<DatatypeHarness<cds::container::OptimisticQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)},   \
    {desc, "RW queue", new test<DatatypeHarness<cds::container::RWQueue<type>>>(CDS, ## __VA_ARGS__)}, \
    {desc, "Segmented queue", new test<DatatypeHarness<cds::container::SegmentedQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)}, \
    {desc, "TC queue", new test<DatatypeHarness<cds::container::TsigasCycleQueue<type>>>(CDS, ## __VA_ARGS__)}, \
    {desc, "VyukovMPMC queue", new test<DatatypeHarness<cds::container::VyukovMPMCCycleQueue<type>>>(CDS, ## __VA_ARGS__)}
*/
Test queue_tests[] = {
    MAKE_QUEUE_TESTS("Q:PushPop", PushPopTest, int, RANDOM_VALS),
    MAKE_QUEUE_TESTS("Q:RandSingleOps", RandomSingleOpTest, int, RANDOM_VALS),
    //MAKE_QUEUE_TESTS("General Txns Test with Random Vals", GeneralTxnsTest, int, RANDOM_VALS, q_txn_sets[0]),
};
int num_queues = 4;

int main() {
    global_verbose_stats_file = fopen("microbenchmarks/cds_benchmarks_stats_verbose.txt", "w");
    global_stats_file = fopen("microbenchmarks/cds_benchmarks_stats.txt", "w");
    if ( !global_stats_file || !global_verbose_stats_file ) {
        fprintf(stderr, "Could not open file to write stats");
        return 1;
    }

    srandomdev();
    for (unsigned i = 0; i < arraysize(initial_seeds); ++i)
        initial_seeds[i] = random();

    for (unsigned i = 0; i < arraysize(rand_vals); ++i)
        rand_vals[i] = random() % MAX_VALUE;

    std::ios_base::sync_with_stdio(true);
    assert(CONSISTENCY_CHECK); // set CONSISTENCY_CHECK in Transaction.hh

    cds::Initialize();
    cds::gc::HP hpGC;
    cds::threading::Manager::attachThread();

    // create epoch advancer thread
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);
    
    RandomSingleOpTest<DatatypeHarness<cds::container::FCQueue<int>>> fctest(CDS, RANDOM_VALS);
    RandomSingleOpTest<DatatypeHarness<PriorityQueue<int>>> pqtestr(STO, RANDOM_VALS);
    RandomSingleOpTest<DatatypeHarness<PriorityQueue<int>>> pqtestd(STO, DECREASING_VALS);
    
    for (unsigned i = 0; i < arraysize(pqueue_tests); i+=num_pqueues) {
        dualprintf("\n%s\n", pqueue_tests[i].desc.c_str());
        fprintf(global_verbose_stats_file, "STO, STO(O), MS, FC\n");
        for (auto size = begin(init_sizes); size != end(init_sizes); ++size) {
            for (auto nthreads = begin(nthreads_set); nthreads != end(nthreads_set); ++nthreads) {
                for (int j = 0; j < num_pqueues; ++j) {
                    // for the two-thread test, skip if nthreads != 2
                    if (pqueue_tests[i].desc.find("PushPop")!=std::string::npos
                            && *nthreads != 2) {
                        continue;
                    }
                    fprintf(global_verbose_stats_file, "\nRunning Test %s on %s\t size: %d, nthreads: %d\n", 
                            pqueue_tests[i+j].desc.c_str(), pqueue_tests[i+j].ds, *size, *nthreads);
                    startAndWait(pqueue_tests[i+j].test, *size, *nthreads);
                    fprintf(stderr, "\nRan Test %s on %s\t size: %d, nthreads: %d\n", 
                            pqueue_tests[i+j].desc.c_str(), pqueue_tests[i+j].ds, *size, *nthreads);
                }
                if (pqueue_tests[i].desc.find("PushPop")==std::string::npos) dualprintf("\n");
            }
            if (pqueue_tests[i].desc.find("PushPop")!=std::string::npos) dualprintf("\n");
            dualprintf("\n");
        }
    }
    for (unsigned i = 0; i < arraysize(queue_tests); i+=num_queues) {
        dualprintf("\n%s\n", queue_tests[i].desc.c_str());
        fprintf(global_verbose_stats_file, "STO, STO(2), FC, FC(elim), \n");
        for (auto size = begin(init_sizes); size != end(init_sizes); ++size) {
            for (auto nthreads = begin(nthreads_set); nthreads != end(nthreads_set); ++nthreads) {
                for (int j = 0; j < num_queues; ++j) {
                    if (queue_tests[i].desc.find("PushPop")!=std::string::npos && *nthreads != 2) {
                        continue;
                    }
                    fprintf(global_verbose_stats_file, "\nRunning Test %s on %s\t size: %d, nthreads: %d\n", 
                            queue_tests[i+j].desc.c_str(), queue_tests[i+j].ds, *size, *nthreads);
                    startAndWait(queue_tests[i+j].test, *size, *nthreads);
                    fprintf(stderr, "\nRan Test %s on %s\t size: %d, nthreads: %d\n", 
                            queue_tests[i+j].desc.c_str(), queue_tests[i+j].ds, *size, *nthreads);
                }
                if (queue_tests[i].desc.find("PushPop")==std::string::npos) dualprintf("\n");
            }
            if (queue_tests[i].desc.find("PushPop")!=std::string::npos) dualprintf("\n");
            dualprintf("\n");
        }
    }
    cds::Terminate();
    return 0;
}


