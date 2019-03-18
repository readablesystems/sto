// Garbage collection and other bookkeeping tasks

#pragma once

#include <deque>

#include "MVCCTypes.hh"
#include "TThread.hh"

#ifndef MAX_THREADS
#define MAX_THREADS 128
#endif

class MvRegistry {
public:
    typedef MvHistoryBase base_type;
    typedef TransactionTid::type type;
    const static size_t CYCLE_LENGTH = 2;
    const static size_t GC_PER_FLATTEN = 1000;

    MvRegistry() : enable_gc(false) {
        always_assert(
            this == &MvRegistry::registrar_,
            "Only one MvRegistry can be created, which is the "
            "MvRegistry::registrar()");
        for (size_t i = 0; i < (sizeof registries) / (sizeof *registries); i++) {
            registries[i] = new registry_type();
        }
    }

    ~MvRegistry() {
        is_stopping = true;
        while (is_running > 0);
    }

    static void cleanup() {
        registrar().cleanup_();
    }

    // XXX: VERY NOT THREAD-SAFE!
    static void collect_garbage() {
        if (registrar().enable_gc && !((++cycles) % CYCLE_LENGTH)) {
            for (size_t i = 0; i < (sizeof registries) / (sizeof *registries); i++) {
                registrar().collect_garbage_(i);
            }
        }
    }

    // XXX: NOT THREAD-SAFE FOR TWO CALLS WITH THE SAME INDEX
    static void collect_garbage(const size_t index) {
        if (registrar().enable_gc && !((++cycles) % CYCLE_LENGTH)) {
            registrar().collect_garbage_(index);
        }
    }

    static bool done() {
        return registrar().done_();
    }

    template <typename T>
    static void reg(MvObject<T> * const obj, const type tid, std::atomic<bool> * const flag) {
        registrar().reg_(obj, tid, flag);
    }

    inline static MvRegistry& registrar() {
        return registrar_;
    }

    inline static type rtid_inf() {
        return registrar().compute_rtid_inf();
    }

    static void stop() {
        registrar().stop_();
    }

    static void toggle_gc(const bool enabled) {
        registrar().enable_gc = enabled;
    }

private:
    // Represents the head element of an MvObject
    struct MvRegistryEntry;
    typedef MvRegistryEntry entry_type;

    struct MvRegistryEntry {
        typedef std::atomic<base_type*> head_type;
        MvRegistryEntry(
            head_type * const head, base_type * const ih, const type tid,
            std::atomic<bool> * const flag) :
            head(head), inlined(ih), tid(tid), flag(flag) {}

        head_type * const head;
        base_type * const inlined;
        const type tid;
        std::atomic<bool> * const flag;
    };

    static __thread size_t cycles;
    static MvRegistry registrar_;

    std::atomic<bool> enable_gc;
    std::atomic<size_t> is_running;
    std::atomic<bool> is_stopping;
    typedef std::deque<entry_type> registry_type;
    registry_type *registries[MAX_THREADS];

    void cleanup_() {
        for (size_t i = 0; i < (sizeof registries) / (sizeof *registries); i++) {
            delete registries[i];
        }
    }

    void collect_(registry_type&, const type);

    void collect_garbage_(const size_t index) {
        if (is_stopping) {
            return;
        }
        const type rtid_inf = registrar().compute_rtid_inf();
        registrar().is_running++;
        if (is_stopping) {
            is_running--;
            return;
        }
        if (!(cycles % (CYCLE_LENGTH * GC_PER_FLATTEN))) {
            flatten_(registry(index), rtid_inf);
        }
        collect_(registry(index), rtid_inf);
        is_running--;
    }

    type compute_rtid_inf();
   
    bool done_() {
        return is_running == 0;
    }

    void flatten_(registry_type&, const type);

    template <typename T>
    void reg_(MvObject<T> * const, const type, std::atomic<bool> * const);

    inline registry_type& registry(
            const size_t threadid=TThread::id()) {
        return *registries[threadid];
    }

    void stop_() {
        is_stopping = true;
    }
};

template <typename T>
void MvRegistry::reg_(MvObject<T> * const obj, const type tid, std::atomic<bool> * const flag) {
    registry().push_back(entry_type(&obj->h_,
#if MVCC_INLINING
        &obj->ih_
#else
        nullptr
#endif
        , tid, flag));
}
