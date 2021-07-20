#pragma once

#include <atomic>
#include <random>

#include "Sto.hh"
#include <cxxabi.h>
#include "xxhash.h"

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

struct InlineCounterPool;
struct InlineCounterSet;

struct InlineCounterSet {
    uintptr_t obj = 0x0;
    int64_t read_score = 0;
    int64_t write_score = 0;
    int32_t read_count = 0;
    int32_t write_count = 0;

    inline void reset() {
        memset(this, 0, sizeof *this);
    }
};

struct InlineCounterPool {
public:
    static constexpr size_t PoolSize = 4ULL << 20;  // 4MiB
    static constexpr size_t xxh_seed = 0xfeedf00d2cabba0e;
    static constexpr size_t TableSize = PoolSize / sizeof (InlineCounterSet);

private:
    template <int Algorithm>
    static inline uint64_t GetIndex(uint64_t x) {
        static_assert(Algorithm < 3, "Invalid hashing algorithm specified.");
        if constexpr (Algorithm == 0) {
            return XXH64(&x, sizeof x, xxh_seed) % TableSize;
        } else if constexpr (Algorithm == 1) {
            return std::minstd_rand(x)() % TableSize;
        } else if constexpr (Algorithm == 2) {
            // xorshift algorithm from Wikipedia
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            return x % TableSize;
        }
    }

public:
    static inline InlineCounterPool* Get() {
        thread_local static InlineCounterPool* pool = new InlineCounterPool;
        static_assert(sizeof pool->pool <= PoolSize);
        return pool;
    }

    static inline auto GetCounterSet(void* ptr) {
        return GetCounterSet(reinterpret_cast<uintptr_t>(ptr));
    }

    static inline InlineCounterSet* GetCounterSet(uintptr_t obj) {
        auto pool = Get();
        InlineCounterSet* counters = nullptr;
        uintptr_t input = obj;
        for (int attempt = 0; attempt < 10; ++attempt) {
            auto index = GetIndex<2>(input);
            counters = &pool->pool[index];

            if (!counters->obj) {
                counters->obj = obj;
                break;
            }
            if (counters->obj == obj) {
                break;
            }

            input = reinterpret_cast<uintptr_t>(counters);
        }

        if (counters && counters->obj != obj) {
            counters = nullptr;
        }

        return counters;
    }

    static inline void Reset() {
        auto pool = Get();
        pool->pool.fill({});
    }

    std::array<InlineCounterSet, TableSize> pool = {};
};

template <typename T, typename IndexType>
class InlineAdapter {
public:
    static constexpr auto NCOUNTERS = static_cast<size_t>(IndexType::COLCOUNT);

    typedef size_t counter_type;
    typedef std::atomic<counter_type> atomic_counter_type;
    using Index = IndexType;

    struct GlobalCounters {
        std::atomic<uint64_t> target;
        std::atomic<uint64_t> count;
    };

    inline auto counters() {
        return InlineCounterPool::GetCounterSet(this);
    }

    inline void countRead(const Index column, const Index current_split) {
        if (AdapterConfig::IsEnabled(AdapterConfig::Inline)) {
            if (doCount()) {
                auto ptr = counters();
                if (ptr) {
                    (void) current_split;
                    ptr->read_score += static_cast<int64_t>(column);
                    ++ptr->read_count;
                    //printf("%p R: (Score, Count) = %ld, %ld\n", this, ptr->read_score, ptr->read_count);
               }
            }
        }
    }

    inline void countReads(const Index column, const int64_t count, const Index current_split) {
        if (AdapterConfig::IsEnabled(AdapterConfig::Inline)) {
            if (doCount()) {
                auto ptr = counters();
                if (ptr) {
                    (void) current_split;
                    ptr->read_score += count * static_cast<int64_t>(column) +
                        count * (count - 1) / 2;
                    ptr->read_count += count;
                    //printf("%p Rs: (Score, Count) = %ld, %ld\n", this, ptr->read_score, ptr->read_count);
                }
            }
        }
    }

    inline void countWrite(const Index column, const Index current_split) {
        if (AdapterConfig::IsEnabled(AdapterConfig::Inline)) {
            if (doCount()) {
                auto ptr = counters();
                if (ptr) {
                    (void) current_split;
                    ptr->write_score += static_cast<int64_t>(column);
                    ++ptr->write_count;
                    //printf("%p W: (Score, Count) = %ld, %ld\n", this, ptr->write_score, ptr->write_count);
                }
            }
        }
    }

    inline void countWrites(const Index column, const int64_t count, const Index current_split) {
        if (AdapterConfig::IsEnabled(AdapterConfig::Inline)) {
            if (doCount()) {
                auto ptr = counters();
                if (ptr) {
                    (void) current_split;
                    ptr->write_score += count * static_cast<int64_t>(column) +
                        count * (count - 1) / 2;
                    ptr->write_count += count;
                    //printf("%p Ws: (Score, Count) = %ld, %ld\n", this, ptr->write_score, ptr->write_count);
                }
            }
        }
    }

    inline Index currentSplit() const {
        return current_split.load(std::memory_order::memory_order_relaxed);
    }

    inline bool doCount() const {
        auto aborts = this->aborts.load(std::memory_order::memory_order_relaxed);
        auto commits = this->commits.load(std::memory_order::memory_order_relaxed);
        return aborts > commits && aborts > 0;
    }

    inline void finish(const bool committed) {
        int64_t aborts = 0;
        int64_t commits = 0;
        if (committed) {
            //commits = ++this->commits;
            //aborts = this->aborts;
            commits = 1 + this->commits.fetch_add(1, std::memory_order::memory_order_relaxed);
            aborts = this->aborts.load(std::memory_order::memory_order_relaxed);
        } else {
            //aborts = ++this->aborts;
            //commits = this->commits;
            aborts = 1 + this->aborts.fetch_add(1, std::memory_order::memory_order_relaxed);
            commits = this->commits.load(std::memory_order::memory_order_relaxed);
        }

        // Do stuff here
        if (!committed && aborts > commits && aborts > 1) {
            auto ptr = counters();
            if (ptr) {
                int64_t score = 0;
                int64_t read_mean = ptr->read_count ?
                    ptr->read_score / ptr->read_count :
                    static_cast<int64_t>(0);
                int64_t write_mean = ptr->write_count ?
                    ptr->write_score / ptr->write_count :
                    static_cast<int64_t>(Index::COLCOUNT);
                score = read_mean * ptr->write_count + write_mean * ptr->read_count;
                int64_t count = ptr->read_count + ptr->write_count;
                //printf("%p (Score, Count) = %ld, %ld, %ld, %ld\n", this, read_mean, write_mean, score, count);
                score += global_counters.target.fetch_add(
                    score, std::memory_order::memory_order_relaxed);
                count += global_counters.count.fetch_add(
                    count, std::memory_order::memory_order_relaxed);
                if (count) {
                    recomputeSplit();
                }
            }
        }
    }

    inline static uint64_t rand(uint64_t min, uint64_t max) {
        return std::uniform_int_distribution<uint64_t>(min, max)(TThread::gen[TThread::id()].gen);
    }

    inline void recomputeSplit() {
        if (AdapterConfig::IsEnabled(AdapterConfig::Inline)) {
            auto count = global_counters.count.load(
                std::memory_order::memory_order_relaxed);
            auto target = static_cast<Index>(global_counters.target.load(
                std::memory_order::memory_order_relaxed) / count);
            auto split = current_split.load(
                std::memory_order::memory_order_relaxed);
            if (target == Index(0)) {
                target = Index::COLCOUNT;
            }
            //printf("Target: %ld\n", target);
            while ((split != target) &&
                    current_split.compare_exchange_weak(split, target)) {
                split = current_split.load(std::memory_order::memory_order_relaxed);
            }
        }
    }

    inline void reset() {
        if (AdapterConfig::IsEnabled(AdapterConfig::Inline)) {
            auto ptr = counters();
            if (ptr) {
                ptr->reset();
            }
        }
    }

    std::atomic<Index> current_split = T::DEFAULT_SPLIT;
    std::atomic<uint64_t> aborts = 0;
    std::atomic<uint64_t> commits = 0;
    GlobalCounters global_counters = {};
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
