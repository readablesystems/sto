#pragma once

#include "Interface.hh"
#include "timing.hh"
#include <climits>

#define MAX_TS UINT_MAX
#define TS_THRESHOLD 10
#define SUCC_ABORTS_MAX 10
#define WAIT_CYCLES_MULTIPLICATOR 8000

typedef __uint128_t uint128_t;

class Transaction;

class ContentionManager {
public:
    template <bool Opaque>
    static inline bool should_abort(Transaction* tx, TSwissVersion<Opaque> wlock);

    static void on_write(Transaction* tx);

    static void start(Transaction *tx); 

    static void on_rollback(Transaction *tx); 

public:
    // Global timestamp
    static uint64_t ts;

    static uint128_t aborted[128];
    static uint128_t timestamp[128];
    static uint128_t write_set_size[128];
    static uint128_t abort_count[128];
    static uint128_t version[128];
    static uint128_t seed[128];
};
