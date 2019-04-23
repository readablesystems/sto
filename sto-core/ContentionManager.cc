#include <random>

#include "ContentionManager.hh"
#include "Transaction.hh"

// Contention Manager implementation

void ContentionManager::init() {
    static_assert(sizeof(CMInfo) == 64, "CMInfo not cacheline aligned.");
    std::mt19937 gen(0);
    std::uniform_int_distribution<uint32_t> dis(1);
    for (int i = 0; i < MAX_THREADS; ++i) {
        cm_info[i].seed = dis(gen);
        cm_info[i].abort_backoff = INIT_BACKOFF_CYCLES;
    }
}

bool ContentionManager::should_abort(int this_id, int owner_id) {
    TXP_INCREMENT(txp_cm_shouldabort);
    acquire_fence();
    if (cm_info[this_id].aborted == 1){
        return true;
    }

    // This transaction is still in the timid phase
    acquire_fence();
    if (cm_info[this_id].timestamp == MAX_TS) {
        return true;
    }

    //if (cm_info[threadid].write_set_size < cm_info[owner_threadid].write_set_size) {
    if (cm_info[owner_id].timestamp < cm_info[this_id].timestamp) {
        acquire_fence();
        return (cm_info[owner_id].aborted == 0);
    } else {
        //FIXME: this might abort a new transaction on that thread
        cm_info[owner_id].aborted = 1;
        release_fence();
        return false;
    }

}

bool ContentionManager::on_write(int threadid) {
    TXP_INCREMENT(txp_cm_onwrite);
    if (cm_info[threadid].aborted == 1) {
      return false;
    }
    cm_info[threadid].write_set_size += 1;
    if ((cm_info[threadid].timestamp == MAX_TS) &&
            (cm_info[threadid].write_set_size == TS_THRESHOLD)) {
        cm_info[threadid].timestamp = fetch_and_add(&ts, uint64_t(1));
        //cm_info[threadid].timestamp = 1;
    }
    return true;
}

void ContentionManager::start(Transaction *tx) {
    TXP_INCREMENT(txp_cm_start);
    int threadid = tx->threadid();
    if (tx->is_restarted()) {
        // Do not reset abort count
        cm_info[threadid].timestamp = MAX_TS;
        cm_info[threadid].aborted = 0;
        cm_info[threadid].write_set_size = 0;
    } else {
        cm_info[threadid].timestamp = MAX_TS;
        cm_info[threadid].aborted = 0;
        cm_info[threadid].write_set_size = 0;
        cm_info[threadid].abort_count = 0;
        cm_info[threadid].abort_backoff = INIT_BACKOFF_CYCLES;
    }
}

void ContentionManager::on_rollback(int threadid) {
    TXP_INCREMENT(txp_cm_onrollback);
    if (cm_info[threadid].abort_count < SUCC_ABORTS_MAX) {
        ++cm_info[threadid].abort_count;
        cm_info[threadid].abort_backoff <<= 1;
    }
    //uint64_t cycles_to_wait = rand_r((unsigned int*)&cm_info[threadid].seed) % (cm_info[threadid].abort_count * WAIT_CYCLES_MULTIPLICATOR);
    uint64_t cycles_to_wait = rand_r(&(cm_info[threadid].seed)) % cm_info[threadid].abort_backoff;
    wait_cycles(cycles_to_wait);
}

// Defines and initializes the static fields
uint64_t ContentionManager::ts = 0;
CMInfo ContentionManager::cm_info[MAX_THREADS];
