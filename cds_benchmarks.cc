#include "cds_benchmarks.hh"
#include "cds_bm_queues.hh"
#include "cds_bm_maps.hh"
#include <stdlib.h>

/*
 * Threads to run the tests and record performance
 */
void* test_thread(void *data) {
    cds::threading::Manager::attachThread();

    GenericTest *gt = ((Tester*)data)->test;
    int me = ((Tester*)data)->me;
    int nthreads = ((Tester*)data)->nthreads;

    const pthread_t pid = pthread_self();
    // cpu_set_t: This data set is a bitset where each bit represents a CPU.
    cpu_set_t cpuset;
    // CPU_ZERO: This macro initializes the CPU set set to be the empty set.
    CPU_ZERO(&cpuset);
    // CPU_SET: This macro adds cpu to the CPU set set.
    CPU_SET(me, &cpuset);
    assert(!pthread_setaffinity_np(pid, sizeof(cpu_set_t), &cpuset));
    
    TThread::set_id(me);

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
    int nthreads = ((Record*)x)->nthreads;
    long long total1, total2;
    struct timeval tv1, tv2;
    double ops_per_s = 0; 

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
    double seconds = ((tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
    ops_per_s = (total2-total1)/seconds;
   
    ((Record*)x)->speed = ops_per_s;
    for (int i = 0; i < nthreads; ++i) {
        global_thread_ctrs[i].ke_insert = global_thread_ctrs[i].ke_find = global_thread_ctrs[i].ke_erase
        = global_thread_ctrs[i].insert = global_thread_ctrs[i].erase = global_thread_ctrs[i].find
        = global_thread_ctrs[i].push = global_thread_ctrs[i].pop = global_thread_ctrs[i].skip
        = 0;
    }
    return nullptr;
}

void startAndWait(GenericTest* test, size_t size, int nthreads, int repeats) {
    // to record abort and perf results
    std::vector<float> aborts, speeds;
    // create performance recording thread
    pthread_t recorder;
    // create threads to run the test
    pthread_t tids[nthreads];
    Tester testers[nthreads];
    Record record;
   
    for (int r = 0; r < repeats; ++r) {
        test->initialize(size);
        record.speed = 0;
        record.nthreads = nthreads;
        pthread_create(&recorder, NULL, record_perf_thread, &record);

        for (int i = 0; i < nthreads; ++i) {
            testers[i].me = i;
            testers[i].test = test;
            testers[i].nthreads = nthreads;
            pthread_create(&tids[i], NULL, test_thread, &testers[i]);
        }
        for (int i = 0; i < nthreads; ++i) {
            pthread_join(tids[i], NULL);
        }
        pthread_join(recorder, NULL);

        speeds.push_back(record.speed);
        aborts.push_back(get_abort_stats());
        test->cleanup();
    }

    for (float s: speeds) {
        dualprintf("%3f,", s);
    }
    dualprintf(":");
    for (float a: aborts) {
        dualprintf("%3f,", a);
    }
}

void dualprintf(const char* fmt,...) {
    va_list args1, args2, args3;
    va_start(args1, fmt);
    va_start(args2, fmt);
    va_start(args3, fmt);
    vfprintf(global_verbose_stats_file, fmt, args1);
    vfprintf(global_stats_file, fmt, args2);
    vfprintf(stderr, fmt, args3);
    va_end(args1);
    va_end(args2);
    va_end(args3);
}

float get_abort_stats() {
#if STO_PROFILE_COUNTERS
    float aborts = 0;
    if (txp_count >= txp_total_aborts) {
        txp_counters tc = Transaction::txp_counters_combined();

        unsigned long long txc_total_starts = tc.p(txp_total_starts);
        unsigned long long txc_total_aborts = tc.p(txp_total_aborts);
        unsigned long long txc_commit_aborts = tc.p(txp_commit_time_aborts);
        unsigned long long txc_total_commits = txc_total_starts - txc_total_aborts;
        fprintf(global_verbose_stats_file, "\t$ %llu starts, %llu max read set, %llu commits",
                txc_total_starts, tc.p(txp_max_set), txc_total_commits);
        if (txc_total_aborts) {
            aborts = 100.0 * (double) tc.p(txp_total_aborts) / tc.p(txp_total_starts);
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
    return aborts;
#endif
}

void simpleStartAndWait(GenericTest* test, size_t size, int nthreads) {
    // create threads to run the test
    pthread_t tids[nthreads];
    Tester testers[nthreads];
    test->initialize(size);
    for (int i = 0; i < nthreads; ++i) {
        testers[i].me = i;
        testers[i].test = test;
        testers[i].nthreads = nthreads;
        pthread_create(&tids[i], NULL, test_thread, &testers[i]);
    }
    for (int i = 0; i < nthreads; ++i) {
        pthread_join(tids[i], NULL);
    }
    test->cleanup();
}

int main(int argc, char* argv[]) { 
    cds::Initialize();
    cds::gc::HP hpGC(67);
    cds::threading::Manager::attachThread();
        srandomdev();
    for (unsigned i = 0; i < arraysize(initial_seeds); ++i)
        initial_seeds[i] = random();

    std::ios_base::sync_with_stdio(true);

    // create epoch advancer thread
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);
    
        /*
    if (argc >= 2) {
        int test = std::atoi(argv[1]);
      
        //hashmaps
        auto chmna51 = new MapOpTest<DatatypeHarness<CuckooHashMapNA<int, int, 10000, false, 5>>>(STO, 10000, 5, 1, 33, 33);
        auto chmna52 = new MapOpTest<DatatypeHarness<CuckooHashMapNA<int, int, 125000, false, 5>>>(STO, 125000, 5, 1, 33, 33);
        auto chmna53 = new MapOpTest<DatatypeHarness<CuckooHashMapNA<int, int, 1000000, false, 5>>>(STO, 1000000, 5, 1, 33, 33);
        
        auto chmna101 = new MapOpTest<DatatypeHarness<CuckooHashMapNA<int, int, 10000, false, 10>>>(STO, 10000, 10, 1, 33, 33);
        auto chmna102 = new MapOpTest<DatatypeHarness<CuckooHashMapNA<int, int, 125000, false, 10>>>(STO, 125000, 10, 1, 33, 33);
        auto chmna103 = new MapOpTest<DatatypeHarness<CuckooHashMapNA<int, int, 1000000, false, 10>>>(STO, 1000000, 10, 1, 33, 33);

        auto chmna151 = new MapOpTest<DatatypeHarness<CuckooHashMapNA<int, int, 10000, false, 15>>>(STO, 10000, 15, 1, 33, 33);
        auto chmna152 = new MapOpTest<DatatypeHarness<CuckooHashMapNA<int, int, 125000, false, 15>>>(STO, 125000, 15, 1, 33, 33);
        auto chmna153 = new MapOpTest<DatatypeHarness<CuckooHashMapNA<int, int, 1000000, false, 15>>>(STO, 1000000, 15, 1, 33, 33);
        auto chmnt = new MapOpTest<DatatypeHarness<CuckooHashMapNT<int, int, 1000000, 15>>>(STO, 1000000, 15, 1, 33, 33);

        switch(test) {
            case 0:
            simpleStartAndWait(chmna51, 0, 8);
            break;

            case 1:
            simpleStartAndWait(chmna52, 0, 8);
            break;

            case 2:
            simpleStartAndWait(chmna53, 0, 8);
            break;

            case 3:
            simpleStartAndWait(chmna101, 0, 8);
            break;

            case 4:
            simpleStartAndWait(chmna102, 0, 8);
            break;

            case 5:
            simpleStartAndWait(chmna103, 0, 8);
            break;

            case 6:
            simpleStartAndWait(chmna151, 0, 8);
            break;

            case 7:
            simpleStartAndWait(chmna152, 0, 8);
            break;

            case 8:
            simpleStartAndWait(chmna153, 0, 8);
            break;
 
            case 9:
            simpleStartAndWait(chmnt, 0, 8);
            break;

            default:
            assert(0);
        }
        cds::Terminate();
        return 0;
    }
    */
        
    global_verbose_stats_file = fopen("microbenchmarks/cds_benchmarks_stats_verbose.txt", "w");
    global_stats_file = fopen("microbenchmarks/cds_benchmarks_stats.txt", "w");
    if ( !global_stats_file || !global_verbose_stats_file ) {
        fprintf(stderr, "Could not open file to write stats");
        return 1;
    }
    
    dualprintf("\n--------------NEW TEST-----------------\n");

    /*
    std::vector<Test> map_tests = make_map_tests();
    for (unsigned i = 0; i < map_tests.size(); i+=num_maps) {
        dualprintf("\n%s\n", map_tests[i].desc.c_str());
        //for (auto init_keys = begin(init_sizes); init_keys != end(init_sizes); ++init_keys) {
            for (auto nthreads = begin(nthreads_set); nthreads != end(nthreads_set); ++nthreads) {
                for (int j = 0; j < num_maps; ++j) {
                    fprintf(global_verbose_stats_file, "\nRunning Test %s on %s\t init_keys: %d, nthreads: %d\n", 
                            map_tests[i+j].desc.c_str(), map_tests[i+j].ds, 0, *nthreads);
                    startAndWait(map_tests[i+j].test, 0, *nthreads, 5);
                    dualprintf(";");
                }
                dualprintf("\n");
            }
            dualprintf("\n");
        //}
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
                    startAndWait(pqueue_tests[i+j].test, *size, *nthreads, 5);
                    dualprintf(";");
                }
                if (pqueue_tests[i].desc.find("PushPop")==std::string::npos) dualprintf("\n");
            }
            if (pqueue_tests[i].desc.find("PushPop")!=std::string::npos) dualprintf("\n");
            dualprintf("\n");
        }
    }
*/
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
                    startAndWait(queue_tests[i+j].test, *size, *nthreads, 5);
                    dualprintf(";");
                }
                if (queue_tests[i].desc.find("PushPop")==std::string::npos) dualprintf("\n");
            }
            if (queue_tests[i].desc.find("PushPop")!=std::string::npos) dualprintf("\n");
            dualprintf("\n");
        }
    }
    cds::Terminate();
    fclose(global_stats_file);
    fclose(global_verbose_stats_file);
    return 0;
}
/*
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

 * */
