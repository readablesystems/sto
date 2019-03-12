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

    MvRegistry() : enable_gc(false) {
        always_assert(
            this == &MvRegistry::registrar,
            "Only one MvRegistry can be created, which is the "
            "MvRegistry::registrar.");
        for (size_t i = 0; i < (sizeof registries) / (sizeof *registries); i++) {
            registries[i] = new registry_type();
        }
    }

    ~MvRegistry() {
        is_stopping = true;
        while (is_running > 0);
    }

    static void cleanup() {
        registrar.cleanup_();
    }

    // XXX: VERY NOT THREAD-SAFE!
    static void collect_garbage() {
        if (registrar.enable_gc) {
            for (size_t i = 0; i < (sizeof registries) / (sizeof *registries); i++) {
                registrar.collect_garbage_(i);
            }
        }
    }

    // XXX: NOT THREAD-SAFE FOR TWO CALLS WITH THE SAME INDEX
    static void collect_garbage(const size_t index) {
        if (registrar.enable_gc) {
            registrar.collect_garbage_(index);
        }
    }

    static bool done() {
        return registrar.done_();
    }

    template <typename T>
    static void reg(MvObject<T>* const obj, const type tid, std::atomic<bool>* const flag) {
        registrar.reg_(obj, tid, flag);
    }

    static void stop() {
        registrar.stop_();
    }

    static void toggle_gc(const bool enabled) {
        registrar.enable_gc = enabled;
    }

private:
    // Represents the head element of an MvObject
    struct MvRegistryEntry;
    typedef MvRegistryEntry entry_type;

    struct MvRegistryEntry {
        typedef std::atomic<base_type*> head_type;
        MvRegistryEntry(
            head_type* const head, base_type* const ih, const type tid,
            std::atomic<bool>* const flag) :
            head(head), inlined(ih), tid(tid), flag(flag) {}

        head_type* const head;
        base_type* const inlined;
        const type tid;
        std::atomic<bool>* const flag;
    };

    std::atomic<bool> enable_gc;
    std::atomic<size_t> is_running;
    std::atomic<bool> is_stopping;
    static MvRegistry registrar;
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
        const type gc_tid = registrar.compute_gc_tid();
        registrar.is_running++;
        if (is_stopping) {
            is_running--;
            return;
        }
        collect_(registry(index), gc_tid);
        is_running--;
    }

    type compute_gc_tid();
   
    bool done_() {
        return is_running == 0;
    }

    template <typename T>
    void reg_(MvObject<T>* const, const type, std::atomic<bool>* const);

    inline registry_type& registry(
            const size_t threadid=TThread::id()) {
        return *registries[threadid];
    }

    void stop_() {
        is_stopping = true;
    }
};

template <typename T>
void MvRegistry::reg_(MvObject<T>* const obj, const type tid, std::atomic<bool>* const flag) {
    registry().push_back(entry_type(&obj->h_,
#if MVCC_INLINING
        &obj->ih_
#else
        nullptr
#endif
        , tid, flag));
}
