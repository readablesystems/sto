#pragma once

#include <atomic>

#include "Sto.hh"
#include <cxxabi.h>

namespace sto {


struct AdapterConfig {
    static bool Enabled;
};

template <typename T, typename IndexType>
struct Adapter {
public:
    static constexpr auto NCOUNTERS = static_cast<size_t>(IndexType::COLCOUNT);

    typedef size_t counter_type;
    typedef std::atomic<counter_type> atomic_counter_type;
    using Index = IndexType;

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

private:
    Adapter() = default;

    static Adapter& Global() {
        static Adapter* instance = new Adapter();
        return *instance;
    }

public:
    static inline void Commit() {
        Commit(TThread::id());
    }

    static inline void Commit(size_t threadid) {
        if (AdapterConfig::Enabled) {
            for (size_t i = 0; i < NCOUNTERS; i++) {
                Global().global_counters.read_counters[i].fetch_add(
                        Global().thread_counters[threadid].read_counters[i], std::memory_order::memory_order_relaxed);
                Global().global_counters.write_counters[i].fetch_add(
                        Global().thread_counters[threadid].write_counters[i], std::memory_order::memory_order_relaxed);
            }
        }
    }

    // Returns number of columns in the "left split" (which also happens to be
    // the index of the first column in the "right split")
    static inline ssize_t ComputeSplitIndex() {
        // Semi-consistent snapshot of the data; maybe use locks in the future
        std::array<size_t, NCOUNTERS> read_freq;
        std::array<size_t, NCOUNTERS> write_freq;
        std::array<size_t, NCOUNTERS> read_psum;  // Prefix sums
        std::array<size_t, NCOUNTERS> write_psum;  // Prefix sums
        size_t read_total = 0;
        size_t write_total = 0;
        for (auto index = Index(0); index < Index::COLCOUNT; index++) {
            auto numindex = static_cast<std::underlying_type_t<Index>>(index);
            read_freq[numindex] = GetRead(index);
            write_freq[numindex] = GetWrite(index);
            if (numindex) {
                read_psum[numindex] = read_psum[numindex - 1] + read_freq[numindex];
                write_psum[numindex] = write_psum[numindex - 1] + write_freq[numindex];
            } else {
                read_psum[numindex] = read_freq[numindex];
                write_psum[numindex] = write_freq[numindex];
            }
            read_total += read_freq[numindex];
            write_total += write_freq[numindex];
        }

        size_t best_split = static_cast<size_t>(
                Global().current_split.load(std::memory_order::memory_order_relaxed));
        double best_data[2] = {read_psum[best_split - 1] * 1.0 / read_total, write_psum[best_split - 1] * 1.0 / write_total};
        // Maximize write load vs read load difference
        for (size_t active_split = NCOUNTERS - 1; active_split; active_split--) {
            double current_data[2] = {read_psum[active_split - 1] * 1.0 / read_total, write_psum[active_split - 1] * 1.0 / write_total};
            double best_diff = std::abs(best_data[1] - best_data[0]);
            double current_diff = std::abs(current_data[1] - current_data[0]);
            if (current_diff > best_diff * 1.05) {
                best_split = active_split;
                best_data[0] = current_data[0];
                best_data[1] = current_data[1];
            }
        }

        // Load difference is unsubstantial, try to balance writes
        if (best_split == NCOUNTERS) {
            best_split = static_cast<size_t>(
                    Global().current_split.load(std::memory_order::memory_order_relaxed));
            for (size_t active_split = NCOUNTERS - 1; active_split; active_split--) {
                double best_diff = std::abs(write_psum[best_split - 1] * 1.0 / write_total - 0.5);
                double current_diff = std::abs(write_psum[active_split - 1] * 1.0 / write_total - 0.5);

                if (current_diff < best_diff * 0.95) {
                    best_split = active_split;
                }
            }
        }

        return best_split;
    }

    static inline void CountRead(const Index index) {
        CountRead(index, 1);
    }

    static inline void CountRead(const Index index, const counter_type count) {
        assert(index < Index::COLCOUNT);
        if (AdapterConfig::Enabled) {
            auto numindex = static_cast<std::underlying_type_t<Index>>(index);
            Global().thread_counters[TThread::id()].read_counters[numindex] += count;
        }
    }

    static inline void CountReads(const Index start, const Index end) {
        CountReads(start, end, 1);
    }

    static inline void CountReads(
            const Index start, const Index end, const counter_type count) {
        assert(start < end);
        assert(end <= Index::COLCOUNT);
        if (AdapterConfig::Enabled) {
            for (auto index = start; index < end; index++) {
                auto numindex =
                    static_cast<std::underlying_type_t<Index>>(index);
                Global().thread_counters[TThread::id()].read_counters[numindex] += count;
            }
        }
    }

    static inline void CountWrite(const Index index) {
        CountWrite(index, 1);
    }

    static inline void CountWrite(const Index index, const counter_type count) {
        assert(index < Index::COLCOUNT);
        if (AdapterConfig::Enabled) {
            auto numindex = static_cast<std::underlying_type_t<Index>>(index);
            Global().thread_counters[TThread::id()].write_counters[numindex] += count;
        }
    }

    static inline void CountWrites(const Index start, const Index end) {
        CountWrites(start, end, 1);
    }

    static inline void CountWrites(
            const Index start, const Index end, const counter_type count) {
        assert(start < end);
        assert(end <= Index::COLCOUNT);
        if (AdapterConfig::Enabled) {
            for (auto index = start; index < end; index++) {
                auto numindex =
                    static_cast<std::underlying_type_t<Index>>(index);
                Global().thread_counters[TThread::id()].write_counters[numindex] += count;
            }
        }
    }

    static inline Index CurrentSplit() {
        auto split = Global().current_split.load(std::memory_order::memory_order_relaxed);
        assert(split > Index(0));  // 0 is an invalid split
        return split;

    }

    static inline std::pair<counter_type, counter_type> Get(const Index index) {
        return std::make_pair(GetRead(index), GetWrite(index));
    }

    static inline counter_type GetRead(const Index index) {
        if (AdapterConfig::Enabled) {
            auto numindex = static_cast<std::underlying_type_t<Index>>(index);
            return Global().global_counters.read_counters[numindex]
                .load(std::memory_order::memory_order_relaxed);
        }
        return 0;
    }

    static inline counter_type GetWrite(const Index index) {
        if (AdapterConfig::Enabled) {
            auto numindex = static_cast<std::underlying_type_t<Index>>(index);
            return Global().global_counters.write_counters[numindex]
                .load(std::memory_order::memory_order_relaxed);
        }
        return 0;
    }

    static void PrintStats() {
        int status;
        char* tname = abi::__cxa_demangle(typeid(T).name(), 0, 0, &status);
        std::cout << tname << " stats:" << std::endl;
        for (Index index = (Index)0; index < Index::COLCOUNT; index++) {
            std::cout
                << "Read [" << index << "] = " << GetRead(index) << "; "
                << "Write [" << index << "] = " << GetWrite(index) << std::endl;
        }

        std::cout << "Ending split index: " << static_cast<int>(CurrentSplit()) << std::endl;
        std::cout << "Next split index:   " << ComputeSplitIndex() << std::endl;
    }

    static inline bool RecomputeSplit() {
        if (AdapterConfig::Enabled) {
            auto split = ComputeSplitIndex();
            ResetGlobal();
            auto next_split = Index(split);
#if DEBUG
            std::cout << "Recomputed split: " << split << std::endl;
#endif
            auto prev_split = Global().current_split.load(std::memory_order::memory_order_relaxed);
            bool changed = (next_split != prev_split);
            Global().current_split.store(next_split, std::memory_order::memory_order_relaxed);
            return changed;
        }
        return false;
    }

    static inline void ResetGlobal() {
        if (AdapterConfig::Enabled) {
            Global().global_counters.template Reset<true>();
        }
    };

    static inline void ResetThread() {
        if (AdapterConfig::Enabled) {
            Global().thread_counters[TThread::id()].template Reset<false>();
        }
    };

    static inline std::pair<counter_type, counter_type> TGet(const Index index) {
        return std::make_pair(TGetRead(index), TGetWrite(index));
    }

    static inline std::pair<counter_type, counter_type> TGet(const size_t threadid, const Index index) {
        return std::make_pair(TGetRead(threadid, index), TGetWrite(threadid, index));
    }

    static inline counter_type TGetRead(const Index index) {
        return TGetRead(TThread::id(), index);
    }

    static inline counter_type TGetRead(const size_t threadid, const Index index) {
        if (AdapterConfig::Enabled) {
            return Global().thread_counters[threadid].read_counters[
                static_cast<std::underlying_type_t<Index>>(index)];
        }
        return 0;
    }

    static inline counter_type TGetWrite(const Index index) {
        return TGetWrite(TThread::id(), index);
    }

    static inline counter_type TGetWrite(const size_t threadid, const Index index) {
        if (AdapterConfig::Enabled) {
            return Global().thread_counters[threadid].write_counters[
                static_cast<std::underlying_type_t<Index>>(index)];
        }
        return 0;
    }

    std::atomic<Index> current_split = T::DEFAULT_SPLIT;
    CounterSet<atomic_counter_type> global_counters;
    std::array<CounterSet<counter_type>, MAX_THREADS> thread_counters;
};

#ifndef ADAPTER_OF
#define ADAPTER_OF(Type) Type##Adapter
#endif

#ifndef DEFINE_ADAPTER
#define DEFINE_ADAPTER(Type, IndexType) \
    using ADAPTER_OF(Type) = ::sto::Adapter<Type, IndexType>;
#endif

/*
#ifndef INITIALIZE_ADAPTER
#define INITIALIZE_ADAPTER(Type) \
    template <> \
    Type::Index Type::current_split = Type::ClassType::DEFAULT_SPLIT; \
    template <> \
    Type::CounterSet<Type::atomic_counter_type> Type::global_counters = {}; \
    template <> \
    std::array<Type::CounterSet<Type::counter_type>, MAX_THREADS> Type::thread_counters = {};
#endif
*/

#ifndef CREATE_ADAPTER
#define CREATE_ADAPTER(Type, IndexType) \
    DEFINE_ADAPTER(Type, IndexType); /*\
    INITIALIZE_ADAPTER(ADAPTER_OF(Type));*/
#endif

}  // namespace sto;
