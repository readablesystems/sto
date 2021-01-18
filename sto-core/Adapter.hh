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

    // Returns number of columns in the "left split" (which also happens to be
    // the index of the first column in the "right split")
    static inline ssize_t ComputeSplitIndex() {
        // Semi-consistent snapshot of the data; maybe use locks in the future
        std::array<size_t, NCOUNTERS> read_freq;
        std::array<size_t, NCOUNTERS> write_freq;
        size_t read_total = 0;
        size_t write_total = 0;
        for (size_t index = 0; index < NCOUNTERS; index++) {
            read_freq[index] = GetRead(index);
            write_freq[index] = GetWrite(index);
            read_total += read_freq[index];
            write_total += write_freq[index];
        }

        // 25% of expectation is considered frequent
        const size_t rfreq_threshold = read_total / (4 * NCOUNTERS);
        const size_t wfreq_threshold = write_total / (4 * NCOUNTERS);

        // (read, write) data
        std::pair<size_t, size_t> left_data = std::make_pair(0, 0);
        std::pair<size_t, size_t> right_data = std::make_pair(0, 0);
        ssize_t left_count = 0;
        ssize_t right_count = 0;
        while (left_count + right_count < NCOUNTERS) {
            if (!left_count) {
                if (write_freq[left_count] >= wfreq_threshold) {
                    left_data.second += write_freq[left_count];
                }
                if (read_freq[left_count] >= wfreq_threshold) {
                    left_data.first += read_freq[left_count];
                }
                left_count++;
                continue;
            }
            bool lfreq[2] = {
                left_data.first >= rfreq_threshold * left_count / 2,
                left_data.second >= wfreq_threshold * left_count / 2
            };
            bool rfreq[2] = {
                right_data.first >= rfreq_threshold * right_count / 2,
                right_data.second >= wfreq_threshold * right_count / 2
            };
            bool lfreq_curr[2] = {
                read_freq[left_count] >= rfreq_threshold,
                write_freq[left_count] >= wfreq_threshold
            };
            bool rfreq_curr[2] = {
                read_freq[NCOUNTERS - right_count - 1] >= rfreq_threshold,
                write_freq[NCOUNTERS - right_count - 1] >= wfreq_threshold
            };

            bool add_left = false;
            if (lfreq[1] && lfreq_curr[1] || lfreq[0] && lfreq_curr[0]) {
                add_left = true;
            } else if (rfreq[1] && rfreq_curr[1] || rfreq[0] && rfreq_curr[0]) {
                add_left = false;
            } else if (!lfreq_curr[0] && !lfreq_curr[1]) {  // No match :(
                add_left = true;
            } else if (!rfreq_curr[0] && !rfreq_curr[1]) {
                add_left = false;
            } else if (!lfreq[1] && !lfreq_curr[1] && lfreq_curr[0]) {  // Has effect, minimize damage
                add_left = true;
            } else if (!lfreq[0] && !lfreq_curr[0] && !lfreq_curr[1]) {
                add_left = true;
            } else if (!rfreq[1] && !rfreq_curr[1] && rfreq_curr[0]) {
                add_left = false;
            } else if (!rfreq[0] && !rfreq_curr[0] && !rfreq_curr[1]) {
                add_left = false;
            } else {  // Damage has to happen, arbitrarily push to righ
                add_left = false;
            }
            if (add_left) {
                if (lfreq_curr[1]) {
                    left_data.second += write_freq[left_count];
                }
                if (lfreq_curr[0]) {
                    left_data.first += read_freq[left_count];
                }
                left_count++;
            } else {
                if (rfreq_curr[1]) {
                    right_data.second += write_freq[NCOUNTERS - right_count - 1];
                }
                if (rfreq_curr[0]) {
                    right_data.first += read_freq[NCOUNTERS - right_count - 1];
                }
                right_count++;
            }
        }

        // Everything got put into one group and it's frequently written
        if (left_count == NCOUNTERS && left_data.second >= wfreq_threshold * left_count / 2) {
            size_t freq = 0;
            for (left_count = 0; left_count < NCOUNTERS; left_count++) {
                if (freq + write_freq[left_count] >= write_total / 2) {
                    auto distance_without = write_total / 2 - freq;
                    auto distance_with = (freq + write_freq[left_count]) - write_total / 2;
                    if (distance_with > distance_without) {
                        break;
                    }
                }
                freq += write_freq[left_count];
            }
        }

        return left_count;
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
