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

template <bool Opaque>
class TSwissVersion;

class ContentionManager {
public:
    static bool should_abort(int this_id, int owner_id);

    static void on_write(int threadid);

    static void start(Transaction *tx); 

    static void on_rollback(int threadid);

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
