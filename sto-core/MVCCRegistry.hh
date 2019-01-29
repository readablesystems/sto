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

        MvRegistryEntry(atomic_base_type *atomic_base) :
            atomic_base(atomic_base), next(nullptr), valid(true) {}
    };

    MvRegistry() {
        always_assert(
            this == &MvRegistry::registrar,
            "Only one MvRegistry can be created, which is the "
            "MvRegistry::registrar.");

        for (size_t i = 0; i < (sizeof registries) / (sizeof *registries); i++) {
            registries[i] = nullptr;
        }
    }

    ~MvRegistry() {
        for (size_t i = 0; i < (sizeof registries) / (sizeof *registries); i++) {
            MvRegistryEntry *entry = registries[i].load();
            while (entry) {
                MvRegistryEntry *next = entry->next;
                delete entry;
                entry = next;
            }
            registries[i] = nullptr;
        }
    }

    static void collect_garbage() {
        registrar.collect_garbage_();
    }

    template <typename T>
    static MvRegistryEntry* reg(MvObject<T> *obj) {
        return registrar.reg_(obj);
    }

private:
    typedef std::atomic<MvRegistryEntry*> registry_type;

    static MvRegistry registrar;
    registry_type registries[MAX_THREADS];

    void collect_garbage_();

    template <typename T>
    MvRegistryEntry* reg_(MvObject<T> *obj);

    inline registry_type& registry() {
        return registries[TThread::id()];
    }
};

template <typename T>
MvRegistry::MvRegistryEntry* MvRegistry::reg_(MvObject<T> *obj) {
    MvRegistryEntry *entry = new MvRegistryEntry(&obj->h_);
    assert(entry->atomic_base);
    do {
        entry->next = registry().load();
    } while (!registry().compare_exchange_weak(entry->next, entry));
    return entry;
}
