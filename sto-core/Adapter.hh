#pragma once

#include <atomic>

#include "Sto.hh"

namespace sto {


struct AdapterConfig {
    static bool Enabled;
};
bool AdapterConfig::Enabled = true;

template <typename T, size_t NumCounters>
struct Adapter {
public:
    static constexpr auto NCOUNTERS = NumCounters;

    typedef size_t counter_type;
    typedef std::atomic<counter_type> atomic_counter_type;

    template <typename CounterType>
    struct __attribute__((aligned(128))) CounterSet {
        std::array<CounterType, NCOUNTERS> read_counters;
        std::array<CounterType, NCOUNTERS> write_counters;

        template <bool Loop=true>
        inline void Reset() {
            if constexpr (Loop) {
                for (auto& counter : read_counters) {
                    counter = 0;
                }
                for (auto& counter : write_counters) {
                    counter = 0;
                }
            } else {
                read_counters.fill(0);
                write_counters.fill(0);
            }
        }
    };

    Adapter() = delete;

    static inline void Commit() {
        Commit(TThread::id());
    }

    static inline void Commit(size_t threadid) {
        if (AdapterConfig::Enabled) {
            for (size_t i = 0; i < NCOUNTERS; i++) {
                global_counters.read_counters[i].fetch_add(
                        thread_counters[threadid].read_counters[i], std::memory_order::memory_order_relaxed);
                global_counters.write_counters[i].fetch_add(
                        thread_counters[threadid].write_counters[i], std::memory_order::memory_order_relaxed);
            }
        }
    }

    static inline void CountRead(const size_t index) {
        CountRead(index, 1);
    }

    static inline void CountRead(const size_t index, const counter_type count) {
        assert(index < NCOUNTERS);
        if (AdapterConfig::Enabled) {
            thread_counters[TThread::id()].read_counters[index] += count;
        }
    }

    static inline void CountWrite(const size_t index) {
        CountWrite(index, 1);
    }

    static inline void CountWrite(const size_t index, const counter_type count) {
        assert(index < NCOUNTERS);
        if (AdapterConfig::Enabled) {
            thread_counters[TThread::id()].write_counters[index] += count;
        }
    }

    static inline std::pair<counter_type, counter_type> Get(const size_t index) {
        return std::make_pair(GetRead(index), GetWrite(index));
    }

    static inline counter_type GetRead(const size_t index) {
        if (AdapterConfig::Enabled) {
            return global_counters.read_counters[index].load(std::memory_order::memory_order_relaxed);
        }
        return 0;
    }

    static inline counter_type GetWrite(const size_t index) {
        if (AdapterConfig::Enabled) {
            return global_counters.write_counters[index].load(std::memory_order::memory_order_relaxed);
        }
        return 0;
    }

    static inline void ResetGlobal() {
        if (AdapterConfig::Enabled) {
            global_counters.template Reset<true>();
        }
    };

    static inline void ResetThread() {
        if (AdapterConfig::Enabled) {
            thread_counters[TThread::id()].template Reset<false>();
        }
    };

    static inline std::pair<counter_type, counter_type> TGet(const size_t index) {
        return std::make_pair(TGetRead(index), TGetWrite(index));
    }

    static inline std::pair<counter_type, counter_type> TGet(const size_t threadid, const size_t index) {
        return std::make_pair(TGetRead(threadid, index), TGetWrite(threadid, index));
    }

    static inline counter_type TGetRead(const size_t index) {
        return TGetRead(TThread::id(), index);
    }

    static inline counter_type TGetRead(const size_t threadid, const size_t index) {
        if (AdapterConfig::Enabled) {
            return thread_counters[threadid].read_counters[index];
        }
        return 0;
    }

    static inline counter_type TGetWrite(const size_t index) {
        return TGetWrite(TThread::id(), index);
    }

    static inline counter_type TGetWrite(const size_t threadid, const size_t index) {
        if (AdapterConfig::Enabled) {
            return thread_counters[threadid].write_counters[index];
        }
        return 0;
    }

    static CounterSet<atomic_counter_type> global_counters;
    static std::array<CounterSet<counter_type>, MAX_THREADS> thread_counters;
};

#ifndef ADAPTER_OF
#define ADAPTER_OF(Type) Type##Adapter
#endif

#ifndef INITIALIZE_ADAPTER
#define INITIALIZE_ADAPTER(Type) \
    template <> \
    Type::CounterSet<Type::atomic_counter_type> Type::global_counters = {}; \
    template <> \
    std::array<Type::CounterSet<Type::counter_type>, MAX_THREADS> Type::thread_counters = {};
#endif

#ifndef CREATE_ADAPTER
#define CREATE_ADAPTER(Type, NumCounters) \
    using ADAPTER_OF(Type) = ::sto::Adapter<Type, NumCounters>; \
    INITIALIZE_ADAPTER(ADAPTER_OF(Type));
#endif

}  // namespace sto;
