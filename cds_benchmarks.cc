#include "cds_benchmarks.hh"
#include "cds_bm_queues.hh"
#include "cds_bm_maps.hh"

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
    spawned_barrier--;

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
    while (spawned_barrier != 0) {
        sched_yield();
    }
    for (int i = 0; i < MAX_NUM_THREADS; ++i) {
        total2 += (global_thread_ctrs[i].push + global_thread_ctrs[i].pop);
        total2 += (global_thread_ctrs[i].insert + global_thread_ctrs[i].erase + global_thread_ctrs[i].find);
    }
    gettimeofday(&tv2, NULL);
    double milliseconds = ((tv2.tv_sec-tv1.tv_sec)*1000.0) + ((tv2.tv_usec-tv1.tv_usec)/1000.0);
    ops_per_ms = (total2-total1)/milliseconds > ops_per_ms ? (total2-total1)/milliseconds : ops_per_ms;
    dualprintf("%f, ", ops_per_ms);
    fprintf(stderr, "%f\n", ops_per_ms);
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
    int total_ke_insert, total_ke_find, total_ke_erase, total_inserts, total_find, total_erase;
    total_ke_insert = total_ke_find = total_ke_erase = total_inserts = total_find = total_erase = 0;
    for (int i = 0; i < nthreads; ++i) {
        if (global_thread_ctrs[i].push || global_thread_ctrs[i].pop  || global_thread_ctrs[i].skip) {
            fprintf(global_verbose_stats_file, "Thread %d \tpushes: %ld \tpops: %ld, \tskips: %ld\n", i, 
                    global_thread_ctrs[i].push, 
                    global_thread_ctrs[i].pop, 
                    global_thread_ctrs[i].skip);
        } else {
            fprintf(global_verbose_stats_file, "Thread %d \tinserts: %ld \terases: %ld, \tfinds: %ld\n", i, 
                    global_thread_ctrs[i].insert, 
                    global_thread_ctrs[i].erase, 
                    global_thread_ctrs[i].find);
            total_ke_insert += global_thread_ctrs[i].ke_insert;
            total_ke_find += global_thread_ctrs[i].ke_find;
            total_ke_erase += global_thread_ctrs[i].ke_erase;
            total_inserts += global_thread_ctrs[i].insert;
            total_erase += global_thread_ctrs[i].erase;
            total_find += global_thread_ctrs[i].find;
        }
        global_thread_ctrs[i].ke_insert = global_thread_ctrs[i].ke_find = global_thread_ctrs[i].ke_erase
        = global_thread_ctrs[i].insert = global_thread_ctrs[i].erase = global_thread_ctrs[i].find
        = global_thread_ctrs[i].push = global_thread_ctrs[i].pop = global_thread_ctrs[i].skip
        = 0;
    }
    fprintf(global_verbose_stats_file, "Success Inserts: %f%%\t Success Finds: %f%%\t Success Erase: %f%%\t\n", 
            100 - 100*(double)total_ke_insert/total_inserts,
            100 - 100*(double)total_ke_find/total_find,
            100*(double)total_ke_erase/total_erase);
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

int main() {
    global_verbose_stats_file = fopen("microbenchmarks/cds_benchmarks_stats_verbose.txt", "w");
    global_stats_file = fopen("microbenchmarks/cds_benchmarks_stats.txt", "w");
    if ( !global_stats_file || !global_verbose_stats_file ) {
        fprintf(stderr, "Could not open file to write stats");
        return 1;
    }
    dualprintf("\n--------------NEW TEST-----------------\n");

    srandomdev();
    for (unsigned i = 0; i < arraysize(initial_seeds); ++i)
        initial_seeds[i] = random();

    std::ios_base::sync_with_stdio(true);

    cds::Initialize();
    cds::gc::HP hpGC(67);
    cds::threading::Manager::attachThread();
    
    // create epoch advancer thread
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);

  
    //auto defaultsto = new MapOpTest<DatatypeHarness<Hashtable<int,int,false,125000>>>(STO, 125000, 0, 33, 33);
    //startAndWait(defaultsto, 500000, 12);
    //auto defaultcds = new MapOpTest<DatatypeHarness<Hashtable<int,int,false,10000>>>(STO, 10000, 0, 33, 33);
    //startAndWait(defaultcds, 500000, 12);
    
    //auto sto = new MapOpTest<DatatypeHarness<Hashtable<int,int,false,125000>>>(STO, 125000, 1, 33, 33);
    //startAndWait(sto, 500000, 12);
    
    //auto chmie = new MapOpTest<DatatypeHarness<CuckooHashMap2<int, int, 125000, false>>>(STO, 125000, 1, 33, 33);
    //startAndWait(chmie, 500000, 1);
    //auto chmkf = new MapOpTest<DatatypeHarness<CuckooHashMap<int,int,125000,false>>>(STO, 125000, 1, 33, 33);
    //startAndWait(chmkf, 500000, 1);

    //startAndWait(chmie, 500000, 1);
    
    //auto chmnt = new MapOpTest<DatatypeHarness<CuckooHashMapNT<int, int, 125000>>>(CDS, 125000, 1, 33, 33);
    //startAndWait(chmnt, 500000, 1);

    auto chmna = new MapOpTest<DatatypeHarness<CuckooHashMapNA<int, int, 10000>>>(STO, 10000, 1, 33, 33);
    startAndWait(chmna, 500000, 1);
   
    /*
    std::vector<Test> map_tests = make_map_tests();
    for (unsigned i = 0; i < map_tests.size(); i+=num_maps) {
        dualprintf("\n%s\n", map_tests[i].desc.c_str());
        for (auto init_keys = begin(init_sizes); init_keys != end(init_sizes); ++init_keys) {
            for (auto nthreads = begin(nthreads_set); nthreads != end(nthreads_set); ++nthreads) {
                for (int j = 0; j < num_maps; ++j) {
                    fprintf(global_verbose_stats_file, "\nRunning Test %s on %s\t init_keys: %d, nthreads: %d\n", 
                            map_tests[i+j].desc.c_str(), map_tests[i+j].ds, *init_keys, *nthreads);
                    startAndWait(map_tests[i+j].test, *init_keys, *nthreads);
                    fprintf(stderr, "\nRan Test %s on %s\t init_keys: %d, nthreads: %d\n", 
                            map_tests[i+j].desc.c_str(), map_tests[i+j].ds, *init_keys, *nthreads);
                }
                dualprintf("\n");
            }
            dualprintf("\n");
        }
    }
    // pqueue tests
    std::vector<Test> pqueue_tests = make_pqueue_tests();
    for (unsigned i = 0; i < pqueue_tests.size(); i+=num_pqueues) {
        dualprintf("\n%s\n", pqueue_tests[i].desc.c_str());
        fprintf(global_verbose_stats_file, "STO, STO(O),STO/FC, FC, FC PairingHeap\n");
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
    // queue tests
    std::vector<Test> queue_tests = make_queue_tests();
    for (unsigned i = 0; i < queue_tests.size(); i+=num_queues) {
        dualprintf("\n%s\n", queue_tests[i].desc.c_str());
        fprintf(global_verbose_stats_file, "STO, STO(2), FC, STO/FC, \n");
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
*/
    cds::Terminate();
    return 0;
}
