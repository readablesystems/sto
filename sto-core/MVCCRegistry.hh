// Garbage collection and other bookkeeping tasks

#pragma once

#include "MVCCTypes.hh"
#include "TThread.hh"

#ifndef MAX_THREADS
#define MAX_THREADS 128
#endif

class MvRegistry {
public:
    typedef MvHistoryBase base_type;
    typedef TransactionTid::type type;

    struct MvRegistryEntry {
        typedef std::atomic<base_type*> atomic_base_type;
        atomic_base_type *atomic_base;
        MvRegistryEntry *next;
        std::atomic<bool> valid;
        const base_type *inlined;  // Inlined version
    };

    MvRegistry() : enable_gc(true), enable_GC(true), is_running(0), is_stopping(false) {
        always_assert(
            this == &MvRegistry::registrar,
            "Only one MvRegistry can be created, which is the "
            "MvRegistry::registrar.");

        for (size_t i = 0; i < (sizeof registries) / (sizeof *registries); i++) {
            registries[i] = nullptr;
        }
    }

    ~MvRegistry() {
        is_stopping = true;
        is_running = 0;
        for (size_t i = 0; i < (sizeof registries) / (sizeof *registries); i++) {
            registries[i] = nullptr;
        }
    }

    static void collect_garbage() {
        if (registrar.enable_gc) {
            registrar.collect_garbage_();
        }
    }

    static void collect_garbage(const size_t index) {
        if (registrar.enable_GC) {
            registrar.collect_garbage_(index);
        }
    }

    static bool done() {
        return registrar.done_();
    }

    template <typename T>
    static void reg(MvObject<T> *obj) {
        registrar.reg_(obj);
    }

    static void stop() {
        registrar.stop_();
    }

    static void toggle_gc(const bool enabled) {
        registrar.enable_gc = enabled;
    }

    static void toggle_GC(const bool enabled) {
        registrar.enable_GC = enabled;
    }

private:
    typedef std::atomic<MvRegistryEntry*> registry_type;

    std::atomic<bool> enable_gc;  // For enabling collect_garbage()
    std::atomic<bool> enable_GC;  // For enabling collect_garbage(const size_t)
    std::atomic<size_t> is_running;
    std::atomic<bool> is_stopping;
    static MvRegistry registrar;
    registry_type registries[MAX_THREADS];

    void collect_(registry_type&, const type);

    void collect_garbage_() {
        if (is_stopping) {
            return;
        }
        const type gc_tid = compute_gc_tid();
        for (registry_type &registry : registries) {
            if (is_stopping) {
                return;
            }
            is_running++;
            if (is_stopping) {
                is_running--;
                return;
            }
            collect_(registry, gc_tid);
            is_running--;
        }
    }

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
        collect_(registries[index], gc_tid);
        is_running--;
    }

    type compute_gc_tid();
   
    bool done_() {
        return is_running == 0;
    }

    template <typename T>
    void reg_(MvObject<T> *obj);

    inline registry_type& registry() {
        return registries[TThread::id()];
    }

    void stop_() {
        is_stopping = true;
    }
};

template <typename T>
void MvRegistry::reg_(MvObject<T> *obj) {
    MvRegistryEntry *entry = &obj->rentry_;
    entry->atomic_base = &obj->h_;
    entry->valid = true;
#if MVCC_INLINING
    entry->inlined = &obj->ih_;
#else
    entry->inlined = nullptr;
#endif
    assert(entry->atomic_base);
    do {
        entry->next = registry();
    } while (!registry().compare_exchange_weak(entry->next, entry));
}
