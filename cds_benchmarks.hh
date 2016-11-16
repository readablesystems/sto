#pragma once

#include <string>
#include <iostream>
#include <cstdarg>
#include <assert.h>
#include <vector>
#include <random>
#include <sys/time.h>
#include <time.h>
#include <memory>

#include <cds/init.h>
#include <cds/gc/hp.h> 
#include <cds/os/alloc_aligned.h> 
#include <cds/opt/options.h> 

#include "Transaction.hh"
#include "randgen.hh"

#define GLOBAL_SEED 10

#define MAX_VALUE 20000
#define NTRANS 5000000 // Number of transactions each thread should run.
#define MAX_NUM_THREADS 24 // Maximum number of concurrent threads
#define INITIAL_THREAD 0 // tid of the first thread spawned

// type of data structure to be used
#define STO 0
#define CDS 1

// globals
unsigned initial_seeds[64];
std::vector<int> init_sizes = {10000, 50000, 100000};//, 150000};
std::vector<int> nthreads_set = {1, 2, 4, 8, 12, 16, 20};//, 24};
int rand_vals[10000];
int rand_txns[10000];

std::atomic_int spawned_barrier(0);

struct __attribute__((aligned(128))) cds_counters {
    txp_counter_type push;
    txp_counter_type pop;
    txp_counter_type skip;
    txp_counter_type insert;
    txp_counter_type erase;
    txp_counter_type find;
    txp_counter_type ke_insert; // key_exists during insert
    txp_counter_type ke_erase; // key_exists during erase
    txp_counter_type ke_find; // key_exists during find
};

cds_counters global_thread_ctrs[MAX_NUM_THREADS];

FILE *global_verbose_stats_file;
FILE *global_stats_file;

// This thread is run after all threads modifying the data structure have been spawned. 
// It then iterates until a thread finishes and measures the max rate of ops/ms on the data structure. 
void* record_perf_thread(void* x);
// Takes a Tester (Test + params) struct and runs the requested test.
void* test_thread(void* data);

// Helper functions for printing
void dualprintf(const char* fmt,...); // prints to both the verbose and csv files
void print_abort_stats(void);        // prints how many aborts/commits, etc. for STO ds

// Abstract types to generalize running tests
template <typename DS> struct DatatypeHarness{};
class GenericTest {
public:
    virtual void initialize(size_t init_sz) = 0;
    virtual void run(int me) = 0;
    virtual void cleanup() = 0;
};

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

struct Test {
    std::string desc;
    const char* ds;
    GenericTest* test;
};
