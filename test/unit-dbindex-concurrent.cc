#undef NDEBUG
#include <iostream>
#include <sstream>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <cassert>

#include "xxhash.h"
#include "Sto.hh"
#include "DB_params.hh"
#include "DB_index.hh"

using bench::split_version_helpers;
using bench::mvcc_ordered_index;
using bench::mvcc_unordered_index;
using bench::ordered_index;
using bench::unordered_index;
using bench::access_t;

struct index_key {
    index_key() = default;
    index_key(int32_t k1, int32_t k2)
        : key_1(bench::bswap(k1)), key_2(bench::bswap(k2)) {}

    bool operator==(const index_key& other) const {
        return ((key_1 == other.key_1) && (key_2 == other.key_2));
    }
    bool operator!=(const index_key& other) const {
        return !(*this == other);
    }

    int32_t key_1;
    int32_t key_2;
};

struct index_value {
    enum class NamedColumn : int {
        value_1 = 0,
        value_2
    };

    index_value() : value_1(), value_2() {}
    index_value(int64_t init_val) : value_1(init_val), value_2(init_val) {}

    int64_t value_1;
    int64_t value_2;
};

struct index_value_part1 {
    int64_t value_1;
};

struct index_value_part2 {
    int64_t value_2;
};

namespace std {
template <>
struct hash<index_key> {
    static constexpr size_t xxh_seed = 0xdeadbeefdeadbeef;
    size_t operator()(const index_key& arg) const {
        return XXH64(&arg, sizeof(index_key), xxh_seed);
    }
};

inline ostream& operator<<(ostream& os, const index_key&) {
    os << "index_key";
    return os;
}
} // namespace std

namespace bench {
template<>
struct SplitParams<index_value> {
    using split_type_list = std::tuple<index_value_part1, index_value_part2>;
    static constexpr auto split_builder = std::make_tuple(
        [](const index_value &in) -> index_value_part1 {
          index_value_part1 p1;
          p1.value_1 = in.value_1;
          return p1;
        },
        [](const index_value &in) -> index_value_part2 {
          index_value_part2 p2;
          p2.value_2 = in.value_2;
          return p2;
        }
    );
    static constexpr auto split_merger = std::make_tuple(
        [](index_value *out, const index_value_part1 *in1) -> void {
          out->value_1 = in1->value_1;
        },
        [](index_value *out, const index_value_part2 *in2) -> void {
          out->value_2 = in2->value_2;
        }
    );
    static constexpr auto map = [](int col_n) -> int {
        if (col_n == 0)
            return 0;
        return 1;
    };

    using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
    static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;
};

// Record accessor interface
template<typename A>
class RecordAccessor<A, index_value> {
 public:
    int64_t value_1() const {
        return impl().value_1_impl();
    }

    int64_t value_2() const {
        return impl().value_2_impl();

    }

    void copy_into(index_value* dst) const {
        return impl().copy_into_impl(dst);
    }

 private:
    const A &impl() const {
        return *static_cast<const A *>(this);
    }
};

// Used by 1V split/non-split versions and MV non-split versions.
template<>
class UniRecordAccessor<index_value>
      : public RecordAccessor<UniRecordAccessor<index_value>, index_value> {
 public:
    UniRecordAccessor(const index_value *const vptr) : vptr_(vptr) {}

 private:
    int64_t value_1_impl() const {
        return vptr_->value_1;
    }

    int64_t value_2_impl() const {
        return vptr_->value_2;
    }

    void copy_into_impl(index_value* dst) const {
        if (vptr_) {
            dst->value_1 = vptr_->value_1;
            dst->value_2 = vptr_->value_2;
        }
    }

    const index_value *vptr_;

    friend RecordAccessor<UniRecordAccessor<index_value>, index_value>;
};

// Used by MVCC split version only.
template<>
class SplitRecordAccessor<index_value>
      : public RecordAccessor<SplitRecordAccessor<index_value>, index_value> {
 public:
    static constexpr size_t
        num_splits = bench::SplitParams<index_value>::num_splits;

    SplitRecordAccessor(const std::array<void *, num_splits> &vptrs)
        : vptr_1_(reinterpret_cast<index_value_part1 *>(vptrs[0])),
          vptr_2_(reinterpret_cast<index_value_part2 *>(vptrs[1])) {}

 private:
    int64_t value_1_impl() const {
        return vptr_1_->value_1;
    }

    int64_t value_2_impl() const {
        return vptr_2_->value_2;
    }

    void copy_into_impl(index_value* dst) const {
        if (vptr_1_) {
            dst->value_1 = vptr_1_->value_1;
        }
        if (vptr_2_) {
            dst->value_2 = vptr_2_->value_2;
        }
    }

    const index_value_part1 *vptr_1_;
    const index_value_part2 *vptr_2_;

    friend RecordAccessor<SplitRecordAccessor<index_value>, index_value>;
};
} // namespace bench

namespace commutators {
template <>
class Commutator<index_value> {
public:
    Commutator() = default;

    explicit Commutator(int64_t delta_value_1, int64_t delta_value_2)
        : delta_value_1(delta_value_1), delta_value_2(delta_value_2) {}

    void operate(index_value &value) const {
        value.value_1 += delta_value_1;
        value.value_2 += delta_value_2;
    }

protected:
    int64_t delta_value_1;
    int64_t delta_value_2;
    friend Commutator<index_value_part1>;
    friend Commutator<index_value_part2>;
};

template <>
class Commutator<index_value_part1> : Commutator<index_value> {
public:
    template <typename... Args>
    Commutator(Args&&... args) : Commutator<index_value>(std::forward<Args>(args)...) {}

    void operate(index_value_part1 &value) const {
        value.value_1 += delta_value_1;
    }
};

template <>
class Commutator<index_value_part2> : Commutator<index_value> {
public:
    template <typename... Args>
    Commutator(Args&&... args) : Commutator<index_value>(std::forward<Args>(args)...) {}

    void operate(index_value_part2 &value) const {
        value.value_2 += delta_value_2;
    }
};
}  // namespace commutators

template <bool MVCC, bool Ordered>
class IndexTester {
public:
    static constexpr size_t index_init_size = 1025;
    static constexpr size_t num_increments_per_key = 1000000;
    static constexpr int32_t num_keys = 4;

    using key_type = typename std::conditional_t<Ordered,
                                bench::masstree_key_adapter<index_key>, index_key>;
    using index_type = typename std::conditional_t<Ordered,
                                  std::conditional_t<MVCC,
                                    mvcc_ordered_index<key_type,
                                                       index_value,
                                                       db_params::db_mvcc_commute_params>,
                                    ordered_index<key_type,
                                                  index_value,
                                                  db_params::db_default_commute_params>>,
                                  std::conditional_t<MVCC,
                                    mvcc_unordered_index<key_type,
                                                         index_value,
                                                         db_params::db_mvcc_commute_params>,
                                    unordered_index<key_type,
                                                    index_value,
                                                    db_params::db_default_commute_params>>>;
    typedef index_value::NamedColumn nc;

    IndexTester() : idx_(index_init_size) {}

    void RunTests();

private:
    void UpdaterThread(int thread_id);
    void ReaderThread(int thread_id);

    index_type idx_;
};

std::atomic<bool> stop;
std::mutex io_mu;

using namespace std::chrono_literals;

template <bool MVCC, bool Ordered>
void IndexTester<MVCC, Ordered>::UpdaterThread(int thread_id) {
    TThread::set_id(thread_id);
    if constexpr (Ordered) {
        idx_.thread_init();
    }

    for (size_t i = 0; i < num_increments_per_key; ++i) {
        RWTRANSACTION {
            for (int32_t k = 0; k < num_keys; ++k) {
                key_type key{0, k};
                auto [success, result, row, accessor]
                    = idx_.select_split_row(key,
                                            {{nc::value_1, access_t::write},
                                             {nc::value_2, access_t::write}});
                TXN_DO(success);
                assert(result);
                (void)accessor;
                commutators::Commutator<index_value> comm(1, 1+k);
                idx_.update_row(row, comm);
            }
        } RETRY(true);
    }

    {
        std::lock_guard<std::mutex> lk(io_mu);
        std::cout << "Thread " << thread_id << " finished." << std::endl;
    }
}

template <bool MVCC, bool Ordered>
void IndexTester<MVCC, Ordered>::ReaderThread(int thread_id) {
    TThread::set_id(thread_id);
    if constexpr (Ordered) {
        idx_.thread_init();
    }

    while (!stop.load()) {
        std::array<index_value, num_keys> ans;
        std::fill(ans.begin(), ans.end(), index_value(-1));

        std::stringstream ss;

        TRANSACTION {
            for (int32_t k = 0; k < num_keys; ++k) {
                key_type key{0, k};
                auto [success, result, row, accessor]
                    = idx_.select_split_row(key,
                                            {{nc::value_1, access_t::read},
                                             {nc::value_2, access_t::read}});
                TXN_DO(success);
                assert(result);
                (void)row;
                ans[k].value_1 = accessor.value_1();
                ans[k].value_2 = accessor.value_2();
            }
        } RETRY(true);

        ss << "Reader " << thread_id << ":" << std::endl;
        for (auto& a : ans) {
            ss << "  v1=" << a.value_1 << ", v2=" << a.value_2 << std::endl;
        }

        {
            std::lock_guard<std::mutex> lk(io_mu);
            std::cout << ss.str() << std::flush;
        }

        std::this_thread::sleep_for(500ms);
    }

    std::array<index_value, num_keys> ans;
    std::fill(ans.begin(), ans.end(), index_value(-1));

    std::stringstream ss;

    RWTRANSACTION {
        for (int32_t k = 0; k < num_keys; ++k) {
            key_type key{0, k};
            auto [success, result, row, accessor]
                = idx_.select_split_row(key,
                                        {{nc::value_1, access_t::read},
                                         {nc::value_2, access_t::read}});
            TXN_DO(success);
            assert(result);
            (void)row;
            ans[k].value_1 = accessor.value_1();
            ans[k].value_2 = accessor.value_2();
        }
    } RETRY(true);

    ss << "Final value check (T-" << thread_id << "):" << std::endl;
    for (int32_t k = 0; k < num_keys; ++k) {
        ss << "k-" << k << ": v1=" << ans[k].value_1 << ", v2=" << ans[k].value_2 << std::endl;
    }
    {
        std::lock_guard<std::mutex> lk(io_mu);
        std::cout << ss.str() << std::flush;
    }
}

template <bool MVCC, bool Ordered>
void IndexTester<MVCC, Ordered>::RunTests() {
    static constexpr int num_writers = 8;
    static constexpr int num_readers = 2;
    // Writers: threads [0, num_writers)
    // Readers: threads [num_writers, num_writers + num_readers)
    
    std::vector<std::thread> writers;
    std::vector<std::thread> readers;
    writers.reserve(num_writers);
    readers.reserve(num_readers);

    idx_.thread_init();
    for (int32_t k = 0; k < num_keys; ++k) {
        key_type key{0, k};
        index_value val{};
        idx_.nontrans_put(key, val);
    }

    Transaction::set_epoch_cycle(1000);
    std::thread epoch_advancer_thread = std::thread(&Transaction::epoch_advancer, nullptr);
    epoch_advancer_thread.detach();

    for (int i = 0; i < num_writers; ++i) {
        writers.emplace_back(&IndexTester<MVCC, Ordered>::UpdaterThread, this, i);
    }
    for (int i = num_writers; i < num_writers + num_readers; ++i) {
        readers.emplace_back(&IndexTester<MVCC, Ordered>::ReaderThread, this, i);
    }
    for (auto& t : writers) {
        t.join();
    }
    stop.store(true);
    for (auto& t : readers) {
        t.join();
    }

    std::cout << "Test pass!" << std::endl;

    Transaction::rcu_release_all(epoch_advancer_thread, num_writers + num_readers);
}

int main() {
    IndexTester<true, true> tester;
    tester.RunTests();
}
