#pragma once

#include "Interface.hh"
#include "timing.hh"
#include <climits>

#define MAX_TS UINT_MAX
#define TS_THRESHOLD 10
#define SUCC_ABORTS_MAX 10
#define WAIT_CYCLES_MULTIPLICATOR 10000
#define INIT_BACKOFF_CYCLES 3072

#define MAX_THREADS 128

class Transaction;

struct CMInfo {
    uint32_t aborted;
    uint32_t seed;
    uint64_t timestamp;
    uint64_t write_set_size;
    uint64_t abort_count;
    uint64_t abort_backoff;
    uint64_t version;
    uint64_t padding[2];

    CMInfo() = default;
};

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
    static CMInfo cm_info[MAX_THREADS];
};

