#pragma once
#include "TWrapped.hh"
#include "compiler.hh"
#include "WriteLock.hh"
#include "timing.hh"
#include <limits.h>

typedef __uint128_t uint128_t;

class ContentionManager {
public:
    static bool should_abort(Transaction* tx, WriteLock wlock); 

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
