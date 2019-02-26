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

    MvRegistry() : is_done(false), is_stopping(false) {
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
        is_done = true;
        for (size_t i = 0; i < (sizeof registries) / (sizeof *registries); i++) {
            registries[i] = nullptr;
        }
    }

    static void collect_garbage() {
        registrar.collect_garbage_();
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

private:
    typedef std::atomic<MvRegistryEntry*> registry_type;

    std::atomic<bool> is_done;
    std::atomic<bool> is_stopping;
    static MvRegistry registrar;
    registry_type registries[MAX_THREADS];

    void collect_garbage_();

    bool done_() {
        return is_done;
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
