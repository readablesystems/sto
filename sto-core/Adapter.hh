#pragma once

#include <atomic>

#include "Sto.hh"
#include <cxxabi.h>

namespace sto {

namespace adapter {

template <typename T, typename IndexType> class GlobalAdapter;

class AdapterConfig {
private:
    enum class AdapterType : int8_t {
        None = 0,
        Global,
        Inline
    };

public:
    static constexpr AdapterType None = AdapterType::None;
    static constexpr AdapterType Global = AdapterType::Global;
    static constexpr AdapterType Inline = AdapterType::Inline;

private:
    AdapterConfig() = default;

    static inline AdapterConfig& Instance() {
        static AdapterConfig* instance = new AdapterConfig();
        return *instance;
    }

public:
    static inline void Enable(AdapterType type) {
        Instance().type_ = type;
    }

    static inline bool IsEnabled(AdapterType type) {
        return Instance().type_ == type;
    }

private:

    AdapterType type_ = None;
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

    Adapter() = default;

    inline void Commit() {
        Commit(TThread::id());
    }

    inline void Commit(size_t threadid) {
        if (AdapterConfig::IsEnabled(AdapterConfig::Global)) {
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
    inline ssize_t ComputeSplitIndex() {
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

        size_t prev_split = static_cast<size_t>(
                current_split.load(std::memory_order::memory_order_relaxed));
        size_t best_split = prev_split;
        double best_data[2] = {read_psum[best_split - 1] * 1.0 / read_total, write_psum[best_split - 1] * 1.0 / write_total};
        // Maximize write load vs read load difference
        for (size_t active_split = NCOUNTERS - 1; active_split; active_split--) {
            double current_data[2] = {read_psum[active_split - 1] * 1.0 / read_total, write_psum[active_split - 1] * 1.0 / write_total};
            double best_diff = std::abs(best_data[1] - best_data[0]);
            double current_diff = std::abs(current_data[1] - current_data[0]);
            double factor = best_split == prev_split ? 1.05 : 1;
            if (current_diff > factor * best_diff) {
#if DEBUG
                printf("Split diff %.6f (%zu) > %.2f * %.6f (%zu)\n",
                       current_diff, active_split, factor,  best_diff, best_split);
#endif
                best_split = active_split;
                best_data[0] = current_data[0];
                best_data[1] = current_data[1];
            }
        }

        // Load difference is unsubstantial, try to balance writes
        if (best_split == NCOUNTERS) {
#if DEBUG
            printf("Load difference is unsubstantial, switching to write-balancing\n");
#endif
            best_split = prev_split;
            for (size_t active_split = NCOUNTERS - 1; active_split; active_split--) {
                double best_diff = std::abs(write_psum[best_split - 1] * 1.0 / write_total - 0.5);
                double current_diff = std::abs(write_psum[active_split - 1] * 1.0 / write_total - 0.5);

                double factor = best_split == prev_split ? 0.95 : 1;
                if (current_diff < factor * best_diff) {
#if DEBUG
                    printf("Split diff %.6f (%zu) < %.2f * %.6f (%zu)\n",
                           current_diff, active_split, factor, best_diff, best_split);
#endif
                    best_split = active_split;
                }
            }
        }

        return best_split;
    }

    inline void CountRead(const Index index) {
        CountRead(index, 1);
    }

    inline void CountRead(const Index index, const counter_type count) {
        assert(index < Index::COLCOUNT);
        if (AdapterConfig::IsEnabled(AdapterConfig::Global)) {
            auto numindex = static_cast<std::underlying_type_t<Index>>(index);
            thread_counters[TThread::id()].read_counters[numindex] += count;
        }
    }

    inline void CountReads(const Index start, const Index end) {
        CountReads(start, end, 1);
    }

    inline void CountReads(
            const Index start, const Index end, const counter_type count) {
        assert(start < end);
        assert(end <= Index::COLCOUNT);
        if (AdapterConfig::IsEnabled(AdapterConfig::Global)) {
            for (auto index = start; index < end; index++) {
                auto numindex =
                    static_cast<std::underlying_type_t<Index>>(index);
                thread_counters[TThread::id()].read_counters[numindex] += count;
            }
        }
    }

    inline void CountWrite(const Index index) {
        CountWrite(index, 1);
    }

    inline void CountWrite(const Index index, const counter_type count) {
        assert(index < Index::COLCOUNT);
        if (AdapterConfig::IsEnabled(AdapterConfig::Global)) {
            auto numindex = static_cast<std::underlying_type_t<Index>>(index);
            thread_counters[TThread::id()].write_counters[numindex] += count;
        }
    }

    inline void CountWrites(const Index start, const Index end) {
        CountWrites(start, end, 1);
    }

    inline void CountWrites(
            const Index start, const Index end, const counter_type count) {
        assert(start < end);
        assert(end <= Index::COLCOUNT);
        if (AdapterConfig::IsEnabled(AdapterConfig::Global)) {
            for (auto index = start; index < end; index++) {
                auto numindex =
                    static_cast<std::underlying_type_t<Index>>(index);
                thread_counters[TThread::id()].write_counters[numindex] += count;
            }
        }
    }

    inline Index CurrentSplit() {
        auto split = current_split.load(std::memory_order::memory_order_relaxed);
        assert(split > Index(0));  // 0 is an invalid split
        return split;

    }

    inline std::pair<counter_type, counter_type> Get(const Index index) {
        return std::make_pair(GetRead(index), GetWrite(index));
    }

    inline counter_type GetRead(const Index index) {
        if (AdapterConfig::IsEnabled(AdapterConfig::Global)) {
            auto numindex = static_cast<std::underlying_type_t<Index>>(index);
            return global_counters.read_counters[numindex]
                .load(std::memory_order::memory_order_relaxed);
        }
        return 0;
    }

    inline counter_type GetWrite(const Index index) {
        if (AdapterConfig::IsEnabled(AdapterConfig::Global)) {
            auto numindex = static_cast<std::underlying_type_t<Index>>(index);
            return global_counters.write_counters[numindex]
                .load(std::memory_order::memory_order_relaxed);
        }
        return 0;
    }

    void PrintStats() {
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

    inline bool RecomputeSplit() {
        if (AdapterConfig::IsEnabled(AdapterConfig::Global)) {
            auto split = ComputeSplitIndex();
            ResetGlobal();
            auto next_split = Index(split);
#if DEBUG
            std::cout << "Recomputed split: " << split << std::endl;
#endif
            auto prev_split = current_split.load(std::memory_order::memory_order_relaxed);
            bool changed = (next_split != prev_split);
            current_split.store(next_split, std::memory_order::memory_order_relaxed);
            return changed;
        }
        return false;
    }

    inline void ResetGlobal() {
        if (AdapterConfig::IsEnabled(AdapterConfig::Global)) {
            global_counters.template Reset<true>();
        }
    };

    inline void ResetThread() {
        if (AdapterConfig::IsEnabled(AdapterConfig::Global)) {
            thread_counters[TThread::id()].template Reset<false>();
        }
    };

    inline std::pair<counter_type, counter_type> TGet(const Index index) {
        return std::make_pair(TGetRead(index), TGetWrite(index));
    }

    inline std::pair<counter_type, counter_type> TGet(const size_t threadid, const Index index) {
        return std::make_pair(TGetRead(threadid, index), TGetWrite(threadid, index));
    }

    inline counter_type TGetRead(const Index index) {
        return TGetRead(TThread::id(), index);
    }

    inline counter_type TGetRead(const size_t threadid, const Index index) {
        if (AdapterConfig::IsEnabled(AdapterConfig::Global)) {
            return thread_counters[threadid].read_counters[
                static_cast<std::underlying_type_t<Index>>(index)];
        }
        return 0;
    }

    inline counter_type TGetWrite(const Index index) {
        return TGetWrite(TThread::id(), index);
    }

    inline counter_type TGetWrite(const size_t threadid, const Index index) {
        if (AdapterConfig::IsEnabled(AdapterConfig::Global)) {
            return thread_counters[threadid].write_counters[
                static_cast<std::underlying_type_t<Index>>(index)];
        }
        return 0;
    }

    std::atomic<Index> current_split = T::DEFAULT_SPLIT;
    CounterSet<atomic_counter_type> global_counters;
    std::array<CounterSet<counter_type>, MAX_THREADS> thread_counters;

    friend class GlobalAdapter<T, IndexType>;
};

template <typename T, typename IndexType>
class GlobalAdapter {
public:
    using adapter_type = Adapter<T, IndexType>;
    using Index = typename adapter_type::Index;
    using counter_type = typename adapter_type::counter_type;
    using atomic_counter_type = typename adapter_type::atomic_counter_type;

    static constexpr auto NCOUNTERS = adapter_type::NCOUNTERS;

private:
    GlobalAdapter() = delete;

    static inline adapter_type& Global() {
        static adapter_type* instance = new adapter_type();
        return *instance;
    }

public:
    template <typename... Args>
    static inline void Commit(Args&&... args) {
        return Global().Commit(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline ssize_t ComputeSplitIndex(Args&&... args) {
        return Global().ComputeSplitIndex(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline void CountRead(Args&&... args) {
        return Global().CountRead(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline void CountReads(Args&&... args) {
        return Global().CountReads(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline void CountWrite(Args&&... args) {
        return Global().CountWrite(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline void CountWrites(Args&&... args) {
        return Global().CountWrites(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline Index CurrentSplit(Args&&... args) {
        return Global().CurrentSplit(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline std::pair<counter_type, counter_type> Get(Args&&... args) {
        return Global().counter_type> Get(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline counter_type GetRead(Args&&... args) {
        return Global().GetRead(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline counter_type GetWrite(Args&&... args) {
        return Global().GetWrite(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline void PrintStats(Args&&... args) {
        return Global().PrintStats(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline bool RecomputeSplit(Args&&... args) {
        return Global().RecomputeSplit(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline void ResetGlobal(Args&&... args) {
        return Global().ResetGlobal(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline void ResetThread(Args&&... args) {
        return Global().ResetThread(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline std::pair<counter_type, counter_type> TGet(Args&&... args) {
        return Global().counter_type> TGet(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline counter_type TGetRead(Args&&... args) {
        return Global().TGetRead(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline counter_type TGetWrite(Args&&... args) {
        return Global().TGetWrite(std::forward<Args>(args)...);
    }
};

template <typename T, typename IndexType>
class InlineAdapter {
public:
    static constexpr auto NCOUNTERS = static_cast<size_t>(IndexType::COLCOUNT);

    typedef size_t counter_type;
    typedef std::atomic<counter_type> atomic_counter_type;
    using Index = IndexType;

};

}  // namespace adapter

using adapter::AdapterConfig;

using adapter::Adapter;
using sto::adapter::GlobalAdapter;
using adapter::InlineAdapter;

#ifndef ADAPTER_OF
#define ADAPTER_OF(Type) ::sto::GlobalAdapter<Type, typename Type::NamedColumn>
#endif

#ifndef INLINE_ADAPTER_OF
#define INLINE_ADAPTER_OF(Type) ::sto::InlineAdapter<Type, typename Type::NamedColumn>
#endif

/*
#ifndef DEFINE_ADAPTER
#define DEFINE_ADAPTER(Type, IndexType) \
    using ADAPTER_OF(Type) = ::sto::GlobalAdapter<Type, IndexType>;
#endif

#ifndef INITIALIZE_ADAPTER
#define INITIALIZE_ADAPTER(Type) \
    template <> \
    Type::Index Type::current_split = Type::ClassType::DEFAULT_SPLIT; \
    template <> \
    Type::CounterSet<Type::atomic_counter_type> Type::global_counters = {}; \
    template <> \
    std::array<Type::CounterSet<Type::counter_type>, MAX_THREADS> Type::thread_counters = {};
#endif

#ifndef CREATE_ADAPTER
#define CREATE_ADAPTER(Type, IndexType) \
    DEFINE_ADAPTER(Type, IndexType); \
    INITIALIZE_ADAPTER(ADAPTER_OF(Type));
#endif
*/

}  // namespace sto;
