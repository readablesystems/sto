#include "ContentionManager.hh"
#include "Transaction.hh"

#define MAX_TS UINT_MAX
#define TS_THRESHOLD 10
#define SUCC_ABORTS_MAX 10
#define WAIT_CYCLES_MULTIPLICATOR 8000

template <bool Opaque>
bool ContentionManager::should_abort(Transaction* tx, TSwissVersion<Opaque> wlock) {
    TXP_INCREMENT(txp_cm_shouldabort);
    int threadid = tx->threadid();
    threadid *= 4;
    if (aborted[threadid] == 1){
        return true;
    }

    // This transaction is still in the timid phase
    if (timestamp[threadid] == MAX_TS) {
        return true;
    }

    int owner_threadid = wlock & TransactionTid::threadid_mask;
    owner_threadid *= 4;
    //if (write_set_size[threadid] < write_set_size[owner_threadid]) {
    if (timestamp[owner_threadid] < timestamp[threadid]) {
        if (aborted[owner_threadid] == 0) {
           return true; 
        } else {
           return false;
        }
    } else {
    	//FIXME: this might abort a new transaction on that thread
        aborted[owner_threadid] = 1;
        return false;
    }

}

void ContentionManager::on_write(Transaction* tx) {
    TXP_INCREMENT(txp_cm_onwrite);
    int threadid = tx->threadid();
    threadid *= 4;
    //if (aborted[threadid] == 1) {
    //  Sto::abort();
    //}
    write_set_size[threadid] += 1;
    if (timestamp[threadid] == MAX_TS && write_set_size[threadid] == TS_THRESHOLD) {
        timestamp[threadid] = fetch_and_add(&ts, uint64_t(1));
        //timestamp[threadid] = 1;
    }
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

void ContentionManager::on_rollback(Transaction *tx) {
    TXP_INCREMENT(txp_cm_onrollback);
    int threadid = tx->threadid();
    threadid *= 4;
    if (abort_count[threadid] < SUCC_ABORTS_MAX)
        ++abort_count[threadid];
    uint64_t cycles_to_wait = rand_r((unsigned int*)&seed[threadid]) % (abort_count[threadid] * WAIT_CYCLES_MULTIPLICATOR);
    wait_cycles(cycles_to_wait);
 }

// Defines and initializes the static fields
uint64_t ContentionManager::ts = 0;
uint128_t ContentionManager::aborted[128] = { 0 };
uint128_t ContentionManager::timestamp[128] = { 0 };
uint128_t ContentionManager::write_set_size[128] = { 0 };
uint128_t ContentionManager::abort_count[128] = { 0 };
uint128_t ContentionManager::version[128] = { 0 };
uint128_t ContentionManager::seed[128];
