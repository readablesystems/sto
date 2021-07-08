#undef NDEBUG
#include <iostream>
#include <cassert>
#include <atomic>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include "Sto.hh"
#include "Commutators.hh"
#include "TMvBox.hh"

typedef TMvCommuteIntegerBox tbox_t;
using namespace std::chrono_literals;

std::mutex io_mu;

#define NUM_WRITER_THREADS 10
#define INCREMENTS_PER_THREAD 1000000ul

void ConcurrentThreadIncrement(int thread_id, tbox_t& box) {
    TThread::set_id(thread_id);

    for (size_t i = 0; i < INCREMENTS_PER_THREAD; ++i) {
        RWTRANSACTION {
            TXN_DO(true);
            box.increment(1);
        } RETRY(true);
    }

    {
        std::lock_guard<std::mutex> lk(io_mu);
        std::cout << "Thread " << thread_id << " finished." << std::endl;
    }
}

void ReaderThread(int thread_id, tbox_t& box, std::atomic<bool>& stop) {
    TThread::set_id(thread_id);

    int64_t value_so_far = 0;

    while (!stop.load()) {
        TRANSACTION {
            auto [success, v] = box.read_nothrow();
            assert(v >= value_so_far);
            value_so_far = v;
            TXN_DO(success);
        } RETRY(true);
        std::this_thread::sleep_for(10ms);
    }

    int64_t final_value= -1;
    RWTRANSACTION {
        auto [success, v] = box.read_nothrow();
        TXN_DO(success);
        final_value = v;
    } RETRY(true);

    assert(final_value == (NUM_WRITER_THREADS * INCREMENTS_PER_THREAD));

    {
        std::lock_guard<std::mutex> lk(io_mu);
        std::cout << "Final value: " << final_value << "." << std::endl;
    }
}

int main() {
    std::vector<std::thread> thrs;
    std::thread epoch_advancer;
    std::atomic<bool> stop = false;
    tbox_t box;
    box.nontrans_write(0);

    Transaction::set_epoch_cycle(1000);
    epoch_advancer = std::thread(&Transaction::epoch_advancer, nullptr);
    epoch_advancer.detach();

    for (int i = 0; i < NUM_WRITER_THREADS; ++i) {
        thrs.emplace_back(ConcurrentThreadIncrement, i, std::ref(box));
    }
    for (int i = NUM_WRITER_THREADS; i < NUM_WRITER_THREADS + 2; ++i) {
        thrs.emplace_back(ReaderThread, i, std::ref(box), std::ref(stop));
    }

    for (int i = 0; i < NUM_WRITER_THREADS; ++i) {
        thrs[i].join();
    }
    stop.store(true);

    for (int i = NUM_WRITER_THREADS; i < NUM_WRITER_THREADS + 2; ++i) {
        thrs[i].join();
    }
    std::cout << "Test pass!" << std::endl;

    Transaction::epoch_advance_once();
    Transaction::epoch_advance_once();
    Transaction::rcu_release_all(epoch_advancer, NUM_WRITER_THREADS + 2);
}
