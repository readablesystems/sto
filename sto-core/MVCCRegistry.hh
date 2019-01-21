// Garbage collection and other bookkeeping tasks

#pragma once

#include "MVCCTypes.hh"
#include "TThread.hh"

#ifndef MAX_THREADS
#define MAX_THREADS 128
#endif

class MvRegistry {
public:
    typedef TransactionTid::type type;

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
    static void reg(MvObject<T> *obj) {
        registrar.reg_(obj);
    }

private:
    typedef MvHistoryBase base_type;
    struct MvRegistryEntry {
        typedef std::atomic<base_type*> atomic_base_type;
        atomic_base_type *atomic_base;
        MvRegistryEntry *next;

        MvRegistryEntry(atomic_base_type *atomic_base) :
            atomic_base(atomic_base), next(nullptr) {}
    };
    typedef std::atomic<MvRegistryEntry*> registry_type;

    static MvRegistry registrar;
    registry_type registries[MAX_THREADS];

    void collect_garbage_();

    template <typename T>
    void reg_(MvObject<T> *obj);

    inline registry_type& registry() {
        return registries[TThread::id()];
    }
};

template <typename T>
void MvRegistry::reg_(MvObject<T> *obj) {
    MvRegistryEntry *entry = new MvRegistryEntry(&obj->h_);
    assert(entry->atomic_base);
    do {
        entry->next = registry().load();
    } while (!registry().compare_exchange_weak(entry->next, entry));
}
