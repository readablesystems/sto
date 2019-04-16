#include <random>

#include "ContentionManager.hh"
#include "Transaction.hh"

// Contention Manager implementation

void ContentionManager::init() {
    std::mt19937 gen(0);
    std::uniform_int_distribution<int> dis(1);
    for (int i = 0; i < MAX_THREADS; ++i) {
        auto idx = i * 4;
        seed[idx] = dis(gen);
    }
}

bool ContentionManager::should_abort(int this_id, int owner_id) {
    TXP_INCREMENT(txp_cm_shouldabort);
    this_id *= 4;
    acquire_fence();
    if (aborted[this_id] == 1){
        return true;
    }

    // This transaction is still in the timid phase
    acquire_fence();
    if (timestamp[this_id] == MAX_TS) {
        return true;
    }

    owner_id *= 4;
    //if (write_set_size[threadid] < write_set_size[owner_threadid]) {
    if (timestamp[owner_id] < timestamp[this_id]) {
        acquire_fence();
        return (aborted[owner_id] == 0);
    } else {
        //FIXME: this might abort a new transaction on that thread
        aborted[owner_id] = 1;
        release_fence();
        return false;
    }

}

bool ContentionManager::on_write(int threadid) {
    TXP_INCREMENT(txp_cm_onwrite);
    threadid *= 4;
    if (aborted[threadid] == 1) {
      return false;
    }
    write_set_size[threadid] += 1;
    if (timestamp[threadid] == MAX_TS && write_set_size[threadid] == TS_THRESHOLD) {
        timestamp[threadid] = fetch_and_add(&ts, uint64_t(1));
        //timestamp[threadid] = 1;
    }
    return true;
}

void ContentionManager::start(Transaction *tx) {
    TXP_INCREMENT(txp_cm_start);
    int threadid = tx->threadid();
    threadid *= 4;
    if (tx->is_restarted()) {
        // Do not reset abort count
        timestamp[threadid] = MAX_TS;
        aborted[threadid] = 0;
        write_set_size[threadid] = 0;
    } else {
        timestamp[threadid] = MAX_TS;
        aborted[threadid] = 0;
        write_set_size[threadid] = 0;
        abort_count[threadid] = 0;
    }
}

void ContentionManager::on_rollback(int threadid) {
    TXP_INCREMENT(txp_cm_onrollback);
    threadid *= 4;
    if (abort_count[threadid] < SUCC_ABORTS_MAX)
        ++abort_count[threadid];
    uint64_t cycles_to_wait = rand_r((unsigned int*)&seed[threadid]) % (abort_count[threadid] * WAIT_CYCLES_MULTIPLICATOR);
    wait_cycles(cycles_to_wait);
}

// Defines and initializes the static fields
uint64_t ContentionManager::ts = 0;
uint128_t ContentionManager::aborted[4*MAX_THREADS] = { 0 };
uint128_t ContentionManager::timestamp[4*MAX_THREADS] = { 0 };
uint128_t ContentionManager::write_set_size[4*MAX_THREADS] = { 0 };
uint128_t ContentionManager::abort_count[4*MAX_THREADS] = { 0 };
uint128_t ContentionManager::version[4*MAX_THREADS] = { 0 };
uint128_t ContentionManager::seed[4*MAX_THREADS];
