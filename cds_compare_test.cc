#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>
#include <map>
#include "Transaction.hh"
#include "CDSTesters.hh"
#include "cds_benchmarks.hh"

#define N_THREADS 5

#define QUEUE 0
#define PQUEUE 1
#define DS PQUEUE

PqueueTester<cds::container::FCPriorityQueue<int>, PriorityQueue<int>> pqtester;
QueueTester<cds::container::VyukovMPMCCycleQueue<int>, Queue<int>> qtester;

template <typename T>
void run(T* q, int me) {
    TThread::set_id(me);
    
    std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
    for (int i = 0; i < NTRANS; ++i) {
        // so that retries of this transaction do the same thing
        auto transseed = i;
        txn_record *tr = new txn_record;
        while (1) {
        Sto::start_transaction();
            try {
                tr->ops.clear();
                
                uint32_t seed = transseed*3 + (uint32_t)me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
                auto seedlow = seed & 0xffff;
                auto seedhigh = seed >> 16;
                Rand transgen(seed, seedlow << 16 | seedhigh);

                op_id_mtx.lock();
#if DS==QUEUE
                int op = slotdist(transgen) % qtester.num_ops_;
                tr->ops.push_back(qtester.doOp(q, op, me, slotdist, transgen));
#elif DS==PQUEUE
                int op = slotdist(transgen) % pqtester.num_ops_;
                tr->ops.push_back(pqtester.doOp(q, op, me, slotdist, transgen));
#endif

                if (Sto::try_commit()) {
                    txn_list[me][op_id++] = tr;
                    op_id_mtx.unlock();
                    break;
                }
            } catch (Transaction::Abort e) {}
        }
    }
}

template <typename T>
void* runFunc(void* x) {
    TesterPair<T>* tp = (TesterPair<T>*) x;
    run(tp->t, tp->me);
    return nullptr;
}


template <typename T>
void startAndWait(T* ds) {
    pthread_t tids[N_THREADS];
    TesterPair<T> testers[N_THREADS];
    for (int i = 0; i < N_THREADS; ++i) {
        testers[i].t = ds;
        testers[i].me = i;
        pthread_create(&tids[i], NULL, runFunc<T>, &testers[i]);
    }
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);
    
    for (int i = 0; i < N_THREADS; ++i) {
        pthread_join(tids[i], NULL);
    }
}

int main() {
    std::ios_base::sync_with_stdio(true);
    assert(CONSISTENCY_CHECK); // set CONSISTENCY_CHECK in Transaction.hh
    lock = 0;

#if DS==QUEUE
    Queue<int> stoq;
    cds::container::VyukovMPMCCycleQueue<int> cdsq(100000);
    qtester.init_sut(&cdsq);
    qtester.init_ref(&stoq);
    for (int i = 0; i < N_THREADS; i++) {
        txn_list.emplace_back();
    }
   
    // run cdsq multithreaded, check against stoq 
    startAndWait(&cdsq);
    
    std::map<uint64_t, txn_record *> combined_txn_list;
    
    for (int i = 0; i < N_THREADS; i++) {
        combined_txn_list.insert(txn_list[i].begin(), txn_list[i].end());
    }
    
    std::map<uint64_t, txn_record *>::iterator it = combined_txn_list.begin();
    for(; it != combined_txn_list.end(); it++) {
        Sto::start_transaction();
        for (unsigned i = 0; i < it->second->ops.size(); i++) {
            qtester.redoOp(&stoq, it->second->ops[i]);
        }
        assert(Sto::try_commit());
    }
    
    qtester.check(&cdsq, &stoq);

#elif DS==PQUEUE
    PriorityQueue<int> stopq;
    cds::container::FCPriorityQueue<int> cdspq;
    pqtester.init_sut(&cdspq);
    pqtester.init_ref(&stopq);
    for (int i = 0; i < N_THREADS; i++) {
        txn_list.emplace_back();
    }
    
    startAndWait(&cdspq);
    
    std::map<uint64_t, txn_record *> combined_txn_list;
    
    for (int i = 0; i < N_THREADS; i++) {
        combined_txn_list.insert(txn_list[i].begin(), txn_list[i].end());
    }
    
    std::map<uint64_t, txn_record *>::iterator it = combined_txn_list.begin();
    for(; it != combined_txn_list.end(); it++) {
        Sto::start_transaction();
        for (unsigned i = 0; i < it->second->ops.size(); i++) {
            pqtester.redoOp(&stopq, it->second->ops[i]);
        }
        assert(Sto::try_commit());
    }
    
    pqtester.check(&cdspq, &stopq);
#endif
	return 0;
}
