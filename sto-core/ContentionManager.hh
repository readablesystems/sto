#pragma once

#include "Interface.hh"
#include "timing.hh"
#include <climits>

#define MAX_TS UINT_MAX
#define TS_THRESHOLD 10
#define SUCC_ABORTS_MAX 20
#define WAIT_CYCLES_MULTIPLICATOR 10000

#define MAX_THREADS 128

typedef __uint128_t uint128_t;

class Transaction;

class ContentionManager {
public:
    static void init();
    static bool should_abort(int this_id, int owner_id);

    static bool on_write(int threadid);

    static void start(Transaction *tx); 

    static void on_rollback(int threadid);

public:
    // Global timestamp
    static uint64_t ts;

    static uint128_t aborted[4*MAX_THREADS];
    static uint128_t timestamp[4*MAX_THREADS];
    static uint128_t write_set_size[4*MAX_THREADS];
    static uint128_t abort_count[4*MAX_THREADS];
    static uint128_t version[4*MAX_THREADS];
    static uint128_t seed[4*MAX_THREADS];
};

