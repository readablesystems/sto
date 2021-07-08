#undef NDEBUG
#include "Transaction.hh"
#include "clp.h"
#include <stdlib.h>
#include <inttypes.h>
#include <thread>

uint64_t nallocated;
uint64_t nfreed;
double delay;
bool stop;

class Tracker {
public:
    Tracker() {
        __sync_fetch_and_add(&nallocated, 1);
    }
    ~Tracker() {
        __sync_fetch_and_add(&nfreed, 1);
    }
};


void* tracker_run(void* argument) {
    int tid = reinterpret_cast<uintptr_t>(argument);
    TThread::set_id(tid);

    while (!stop) {
        double this_delay = delay + (drand48() - 0.5) * delay / 3;
        usleep(useconds_t(this_delay * 0.7e6));
        TRANSACTION_E {
            Transaction::rcu_delete(new Tracker);
            usleep(useconds_t(this_delay * 0.3e6));
        } RETRY_E(false);
    }
    return nullptr;
}


static const Clp_Option options[] = {
    { "delay", 'd', 'd', Clp_ValDouble, Clp_Negate },
    { "nthreads", 'j', 'j', Clp_ValInt, 0 },
    { "nepochs", 'e', 'e', Clp_ValInt, 0 }
};

int main(int argc, char* argv[]) {
    unsigned nthreads = 4;
    TRcuSet::epoch_type nepochs = 1000;
    delay = 0.000001;

    Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);
    int opt;
    while ((opt = Clp_Next(clp)) != Clp_Done) {
        switch (opt) {
        case 'd':
            delay = clp->val.d;
            break;
        case 'j':
            nthreads = clp->val.i;
            break;
        case 'e':
            nepochs = clp->val.i;
            break;
        default:
            abort();
        }
    }
    Clp_DeleteParser(clp);

    if (nthreads > MAX_THREADS) {
        printf("Asked for %d threads but MAX_THREADS is %d\n", nthreads, MAX_THREADS);
        exit(1);
    }

    pthread_t tids[nthreads];
    for (uintptr_t i = 0; i < nthreads; ++i)
        pthread_create(&tids[i], NULL, tracker_run, reinterpret_cast<void*>(i));
    Transaction::set_epoch_cycle(1000);
    auto advancer = std::thread(&Transaction::epoch_advancer, nullptr);

    while (Transaction::global_epochs.global_epoch < nepochs + 1)
        usleep(useconds_t(delay * 1e6));
    stop = true;
    Transaction::global_epochs.run = false;

    for (unsigned i = 0; i < nthreads; ++i)
        pthread_join(tids[i], NULL);

    Transaction::rcu_release_all(advancer, nthreads);

    auto nfreed_before = nfreed;
    for (unsigned i = 0; i < nthreads; ++i)
        Transaction::tinfo[i].rcu_set.~TRcuSet();

    always_assert(nallocated == nfreed, "rcu check");
    always_assert(nfreed_before > 0, "rcu check");
    printf("created %" PRIu64 ", deleted %" PRIu64 ", finally deleted %" PRIu64 "\n", nallocated, nfreed_before, nfreed);
    printf("Test pass.\n");
    return 0;
}
