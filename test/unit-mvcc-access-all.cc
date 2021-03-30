#include "xxhash.h"
#include "Sto.hh"
#include "DB_params.hh"
#include "DB_index.hh"

using bench::split_version_helpers;
using bench::mvcc_ordered_index;
using bench::mvcc_unordered_index;
using bench::RowAccess;
using bench::access_t;

struct index_key {
    index_key() = default;
    index_key(int32_t k1, int32_t k2)
        : key_1(bench::bswap(k1)), key_2(bench::bswap(k2)) {}
    bool operator==(const index_key& other) const {
        return (key_1 == other.key_1) && (key_2 == other.key_2);
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
        value_2a,
        value_2b
    };
    int64_t value_1;
    int64_t value_2a;
    int64_t value_2b;
};

struct index_value_part1 {
    int64_t value_1;
};

struct index_value_part2 {
    int64_t value_2a;
    int64_t value_2b;
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
}

template<>
struct bench::SplitParams<index_value> {
    using split_type_list = std::tuple<index_value_part1, index_value_part2>;
    static constexpr auto split_builder = std::make_tuple(
        [](const index_value &in) -> index_value_part1 {
          index_value_part1 p1;
          p1.value_1 = in.value_1;
          return p1;
        },
        [](const index_value &in) -> index_value_part2 {
          index_value_part2 p2;
          p2.value_2a = in.value_2a;
          p2.value_2b = in.value_2b;
          return p2;
        }
    );
    static constexpr auto split_merger = std::make_tuple(
        [](index_value *out, const index_value_part1 *in1) -> void {
          out->value_1 = in1->value_1;
        },
        [](index_value *out, const index_value_part2 *in2) -> void {
          out->value_2a = in2->value_2a;
          out->value_2b = in2->value_2b;
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

namespace bench {

// Record accessor interface
template<typename A>
class RecordAccessor<A, index_value> {
 public:
    int64_t value_1() const {
        return impl().value_1_impl();
    }

    int64_t value_2a() const {
        return impl().value_2a_impl();

    }

    int64_t value_2b() const {
        return impl().value_2b_impl();
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

    int64_t value_2a_impl() const {
        return vptr_->value_2a;
    }

    int64_t value_2b_impl() const {
        return vptr_->value_2b;
    }

    void copy_into_impl(index_value* dst) const {
        if (vptr_) {
            dst->value_1 = vptr_->value_1;
            dst->value_2a = vptr_->value_2a;
            dst->value_2b = vptr_->value_2b;
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

    int64_t value_2a_impl() const {
        return vptr_2_->value_2a;
    }

    int64_t value_2b_impl() const {
        return vptr_2_->value_2b;
    }

    void copy_into_impl(index_value* dst) const {
        if (vptr_1_) {
            dst->value_1 = vptr_1_->value_1;
        }
        if (vptr_2_) {
            dst->value_2a = vptr_2_->value_2a;
            dst->value_2b = vptr_2_->value_2b;
        }
    }

    const index_value_part1 *vptr_1_;
    const index_value_part2 *vptr_2_;

    friend RecordAccessor<SplitRecordAccessor<index_value>, index_value>;
};
}  // namespace bench

namespace commutators {
template <>
class Commutator<index_value> {
public:
    Commutator() = default;

    explicit Commutator(int64_t delta_value_1)
        : delta_value_1(delta_value_1), delta_value_2a(0), delta_value_2b(0) {}
    explicit Commutator(int64_t delta_value_2a, int64_t delta_value_2b)
        : delta_value_1(0), delta_value_2a(delta_value_2a),
          delta_value_2b(delta_value_2b) {}

    void operate(index_value &value) const {
        value.value_1 += delta_value_1;
        value.value_2a += delta_value_2a;
        value.value_2b += delta_value_2b;
    }

protected:
    int64_t delta_value_1;
    int64_t delta_value_2a;
    int64_t delta_value_2b;
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
        value.value_2a += delta_value_2a;
        value.value_2b += delta_value_2b;
    }
};
}  // namespace commutators

template <bool Ordered>
class MVCCIndexTester {
public:
    void RunTests();

private:
    static constexpr size_t index_init_size = 1025;
    using key_type = typename std::conditional<Ordered, bench::masstree_key_adapter<index_key>, index_key>::type;
    using index_type = typename std::conditional<Ordered, mvcc_ordered_index<key_type, index_value, db_params::db_mvcc_params>,
                                                          mvcc_unordered_index<key_type, index_value, db_params::db_mvcc_params>>::type;
    typedef index_value::NamedColumn nc;

    void PostTest() {
        for (auto& t : Transaction::tinfo) {
            t.write_snapshot_epoch = 0;
            t.epoch = 0;
        }

        Transaction::global_epoch_advance_once();
        Transaction::global_epoch_advance_once();
        Transaction::global_epoch_advance_once();
        Transaction::global_epoch_advance_once();

        for (auto& t : Transaction::tinfo) {
            t.rcu_set.clean_until(Transaction::global_epochs.active_epoch);
        }
    }

    void SelectSplitTest();
    void DeleteTest();
    void CommuteTest();
    void ScanTest();
    void InsertTest();
    void InsertDeleteTest();
    void InsertSameKeyTest();
    void InsertSerialTest();
    void InsertUnreadableTest();
    void DeleteReinsertTest();
    void UpdateTest();
};

template <bool Ordered>
void MVCCIndexTester<Ordered>::RunTests() {
    if constexpr (Ordered) {
        printf("Testing Ordered Index (MVCC TS):\n");
    } else {
        printf("Testing Unordered Index (MVCC TS):\n");
    }
    SelectSplitTest();
    PostTest();
    DeleteTest();
    PostTest();
    CommuteTest();
    PostTest();
    InsertTest();
    PostTest();
    InsertDeleteTest();
    PostTest();
    InsertSameKeyTest();
    PostTest();
    InsertSerialTest();
    PostTest();
    InsertUnreadableTest();
    PostTest();
    UpdateTest();
    PostTest();
    DeleteReinsertTest();
    PostTest();
    if constexpr (Ordered) {
        ScanTest();
        PostTest();
    }
}

template <bool Ordered>
void MVCCIndexTester<Ordered>::SelectSplitTest() {
    index_type idx(index_init_size);
    idx.thread_init();

    key_type key{0, 1};
    index_value val{4, 5, 6};
    idx.nontrans_put(key, val);

    TestTransaction t(0);
    auto[success, result, row, accessor]
        = idx.select_split_row(key,
                               {{nc::value_1,access_t::read},
                                {nc::value_2b,access_t::read}});
    (void)row;
    assert(success);
    assert(result);

    assert(accessor.value_1() == 4);
    assert(accessor.value_2b() == 6);

    assert(t.try_commit());

    printf("Test pass: SelectSplitTest\n");
}

template <bool Ordered>
void MVCCIndexTester<Ordered>::DeleteTest() {
    index_type idx(index_init_size);
    idx.thread_init();

    key_type key{0, 1};
    index_value val{4, 5, 6};
    idx.nontrans_put(key, val);

    {
        TestTransaction t1(0);
        TestTransaction t2(1);

        {
            t1.use();

            auto[success, found] = idx.delete_row(key);

            assert(success);
            assert(found);
        }

        // Concurrent observation should not observe delete
        {
            t2.use();
            auto[success, result, row, accessor]
                = idx.select_split_row(key,
                                       {{nc::value_1,access_t::read},
                                        {nc::value_2b,access_t::read}});
            (void)row; (void)accessor;
            assert(success);
            assert(result);
        }

        assert(t1.try_commit());

        assert(t2.try_commit());
    }

    // Serialized observation should observe delete
    {
        TestTransaction t(0);
        t.get_tx().mvcc_rw_upgrade();
        auto[success, result, row, accessor]
            = idx.select_split_row(key,
                                   {{nc::value_1,access_t::read},
                                    {nc::value_2b,access_t::read}});
        (void)row; (void)accessor;
        assert(success);
        assert(!result);

        assert(t.try_commit());
    }

    printf("Test pass: %s\n", __FUNCTION__);
}

template <bool Ordered>
void MVCCIndexTester<Ordered>::CommuteTest() {
    using key_type = bench::masstree_key_adapter<index_key>;
    using index_type = mvcc_ordered_index<key_type,
                                          index_value,
                                          db_params::db_mvcc_params>;
    typedef index_value::NamedColumn nc;
    index_type idx(index_init_size);
    idx.thread_init();

    key_type key{0, 1};
    index_value val{4, 5, 6};
    idx.nontrans_put(key, val);

    for (int64_t i = 0; i < 10; i++) {
        TestTransaction t(0);
        t.get_tx().mvcc_rw_upgrade();
        auto[success, result, row, accessor]
            = idx.select_split_row(key,
                                   {{nc::value_1,access_t::write},
                                    {nc::value_2b,access_t::none}});
        (void)row; (void)accessor;
        assert(success);
        assert(result);

        {
            commutators::Commutator<index_value> comm(10 + i);
            idx.update_row(row, comm);
        }

        assert(t.try_commit());
    }

    {
        TestTransaction t(0);
        t.get_tx().mvcc_rw_upgrade();
        auto[success, result, row, accessor]
            = idx.select_split_row(key,
                                   {{nc::value_1,access_t::read},
                                    {nc::value_2b,access_t::read}});
        (void)row;
        assert(success);
        assert(result);

        assert(accessor.value_1() == 149);
        assert(accessor.value_2a() == 5);
        assert(accessor.value_2b() == 6);
    }


    printf("Test pass: %s\n", __FUNCTION__);
}

template <bool Ordered>
void MVCCIndexTester<Ordered>::ScanTest() {
    if constexpr (!Ordered) {
        printf("Skipped: ScanTest\n");
    } else {
        using accessor_t = bench::SplitRecordAccessor<index_value>;
        index_type idx(index_init_size);
        idx.thread_init();

        {
            key_type key{0, 1};
            index_value val{4, 5, 6};
            idx.nontrans_put(key, val);
        }
        {
            key_type key{0, 2};
            index_value val{7, 8, 9};
            idx.nontrans_put(key, val);
        }
        {
            key_type key{0, 3};
            index_value val{10, 11, 12};
            idx.nontrans_put(key, val);
        }

        {
            TestTransaction t(0);

            auto scan_callback = [&] (const key_type& key, const auto& split_values) -> bool {
                accessor_t accessor(split_values);
                std::cout << "Visiting key: {" << key.key_1 << ", " << key.key_2 << "}, value parts:" << std::endl;
                std::cout << "    " << accessor.value_1() << std::endl;
                std::cout << "    " << accessor.value_2a() << std::endl;
                std::cout << "    " << accessor.value_2b() << std::endl;
                return true;
            };

            key_type k0{0, 0};
            key_type k1{1, 0};

            bool success = idx.template range_scan<decltype(scan_callback), false>(k0, k1,
                                                                                   scan_callback,
                                                                                   {{nc::value_1,access_t::read},
                                                                                   {nc::value_2b,access_t::read}}, true);
            assert(success);
            assert(t.try_commit());
        }

        // Insert-scan concurrency
        {
            key_type start_key{2, 0};
            key_type end_key{2, 100};
            key_type new_key{2, 21};
            index_value new_value{94, 3, 21};
            key_type another_key{3, 21};
            index_value another_value{9, 9, 4};

            idx.nontrans_put(another_key, another_value);

            {
                TestTransaction t(0);
                t.get_tx().mvcc_rw_upgrade();
                auto [success, found, row, accessor] = idx.select_split_row(new_key, {
                        {nc::value_1, access_t::read},
                        {nc::value_2a, access_t::read},
                        {nc::value_2b, access_t::read}
                        });
                (void)row;
                (void)accessor;
                assert(success);
                assert(!found);
                assert(t.try_commit());
            }

            TestTransaction t1(1);
            t1.get_tx().mvcc_rw_upgrade();

            idx.insert_row(new_key, &new_value);

            TestTransaction t2(2);
            t2.get_tx().mvcc_rw_upgrade();

            {
                bool found = false;
                auto scan_callback = [&found] (const key_type&, const auto&) -> bool {
                    found |= true;
                    return true;
                };
                bool success = idx.template range_scan<decltype(scan_callback), false>(
                        start_key, end_key, scan_callback, {
                        {nc::value_1, access_t::read},
                        {nc::value_2a, access_t::read},
                        {nc::value_2b, access_t::read}
                        });
                assert(success);
                assert(!found);
            }

            t1.use();

            {
                index_value val{9, 9, 14};
                auto [success, found] = idx.insert_row(another_key, &val, true);
                assert(success);
                assert(found);
            }

            assert(t1.try_commit());

            t2.use();

            {
                index_value val{9, 9, 24};
                auto [success, found] = idx.insert_row(another_key, &val, true);
                assert(success);
                assert(found);
            }

            assert(!t2.try_commit());

            {
                index_value val;
                idx.nontrans_get(another_key, &val);
                assert(val.value_2b == 14);
            }
        }

        printf("Test pass: ScanTest\n");
    }
}

template <bool Ordered>
void MVCCIndexTester<Ordered>::InsertTest() {
    index_type idx(index_init_size);
    idx.thread_init();

    {
        TestTransaction t(0);
        t.get_tx().mvcc_rw_upgrade();
        key_type key{0, 1};
        index_value val{4, 5, 6};

        auto[success, found] = idx.insert_row(key, &val);
        assert(success);
        assert(!found);
        assert(t.try_commit());
    }

    {
        TestTransaction t(1);
        t.get_tx().mvcc_rw_upgrade();
        key_type key{0, 1};
        auto[success, result, row, accessor] = idx.select_split_row(key, {{nc::value_1, access_t::read},
                                                                          {nc::value_2a, access_t::read},
                                                                          {nc::value_2b, access_t::read}});
        (void)row;
        assert(success);
        assert(result);

        assert(accessor.value_1() == 4);
        assert(accessor.value_2a() == 5);
        assert(accessor.value_2b() == 6);
        assert(t.try_commit());
    }

    // Insert-get concurrency
    {
        key_type start_key{2, 0};
        key_type end_key{2, 100};
        key_type new_key{2, 21};
        index_value new_value{94, 3, 21};
        key_type another_key{3, 21};
        index_value another_value{9, 9, 4};

        idx.nontrans_put(another_key, another_value);

        {
            TestTransaction t(0);
            t.get_tx().mvcc_rw_upgrade();
            auto [success, found, row, accessor] = idx.select_split_row(new_key, {
                    {nc::value_1, access_t::read},
                    {nc::value_2a, access_t::read},
                    {nc::value_2b, access_t::read}
                    });
            (void)row;
            (void)accessor;
            assert(success);
            assert(!found);
            assert(t.try_commit());
        }

        TestTransaction t1(1);
        t1.get_tx().mvcc_rw_upgrade();

        idx.insert_row(new_key, &new_value);

        TestTransaction t2(2);
        t2.get_tx().mvcc_rw_upgrade();

        {
            auto [success, found, row, accessor] = idx.select_split_row(new_key, {
                    {nc::value_1, access_t::read},
                    {nc::value_2a, access_t::read},
                    {nc::value_2b, access_t::read}
                    });
            (void)row;
            (void)accessor;
            assert(success);
            assert(!found);
        }

        t1.use();

        {
            index_value val{9, 9, 14};
            auto [success, found] = idx.insert_row(another_key, &val, true);
            assert(success);
            assert(found);
        }

        assert(t1.try_commit());

        t2.use();

        {
            index_value val{9, 9, 24};
            auto [success, found] = idx.insert_row(another_key, &val, true);
            assert(success);
            assert(found);
        }

        assert(!t2.try_commit());

        {
            index_value val;
            idx.nontrans_get(another_key, &val);
            assert(val.value_2b == 14);
        }
    }
}

template <bool Ordered>
void MVCCIndexTester<Ordered>::InsertDeleteTest() {
    index_type idx(index_init_size);
    idx.thread_init();

    for (int itr = 0; itr < 16; itr++) {
        TestTransaction t1(0);
        t1.get_tx().mvcc_rw_upgrade();
        key_type key1{1, 1};
        index_value val1{10 * itr + 1, 10 * itr + 2, 10 * itr + 3};
        {
            auto [success, found] = idx.insert_row(key1, &val1);
            assert(success);
            assert(!found);
        }
        assert(t1.try_commit());

        TestTransaction t2(1);
        t2.get_tx().mvcc_rw_upgrade();
        key_type key2{1, 1};
        {
            auto[success, result, row, accessor] = idx.select_split_row(key2, {{nc::value_1, access_t::read},
                                                                               {nc::value_2a, access_t::read},
                                                                               {nc::value_2b, access_t::read}});
            (void)row;
            assert(success);
            assert(result);
            assert(accessor.value_1() == 10 * itr + 1);
            assert(accessor.value_2a() == 10 * itr + 2);
            assert(accessor.value_2b() == 10 * itr + 3);
        }
        assert(t2.try_commit());

        TestTransaction t3(2);
        t3.get_tx().mvcc_rw_upgrade();
        key_type key3{1, 1};
        {
            auto [success, found] = idx.delete_row(key2);
            assert(success);
            assert(found);
        }
        assert(t3.try_commit());

        TestTransaction t4(3);
        t4.get_tx().mvcc_rw_upgrade();
        key_type key4{1, 1};
        {
            auto[success, result, row, accessor] = idx.select_split_row(key4, {{nc::value_1, access_t::read},
                                                                               {nc::value_2a, access_t::read},
                                                                               {nc::value_2b, access_t::read}});
            (void)row;
            (void)accessor;
            assert(success);
            assert(!result);
        }
        assert(t4.try_commit());
    }

    printf("Test pass: InsertDeleteTest\n");
}

template <bool Ordered>
void MVCCIndexTester<Ordered>::InsertSameKeyTest() {
    index_type idx(index_init_size);
    idx.thread_init();

    {
        TestTransaction t1(0);
        t1.get_tx().mvcc_rw_upgrade();
        key_type key1{1, 1};
        index_value val1{11, 12, 13};
        {
            auto [success, found] = idx.insert_row(key1, &val1);
            assert(success);
            assert(!found);
        }

        TestTransaction t2(1);
        t2.get_tx().mvcc_rw_upgrade();
        key_type key2{1, 1};
        index_value val2{14, 15, 16};
        {
            auto [success, found] = idx.insert_row(key2, &val2);
            assert(success);
            assert(!found);
        }
        assert(t2.try_commit());

        t1.use();
        assert(!t1.try_commit());
    }

    {
        TestTransaction t(2);
        t.get_tx().mvcc_rw_upgrade();
        key_type key{1, 1};
        auto[success, result, row, accessor] = idx.select_split_row(key, {{nc::value_1, access_t::read},
                                                                          {nc::value_2a, access_t::read},
                                                                          {nc::value_2b, access_t::read}});
        (void)row;
        assert(success);
        assert(result);

        assert(accessor.value_1() == 14);
        assert(accessor.value_2a() == 15);
        assert(accessor.value_2b() == 16);
        assert(t.try_commit());
    }

    printf("Test pass: InsertSameKeyTest\n");
}

template <bool Ordered>
void MVCCIndexTester<Ordered>::InsertSerialTest() {
    index_type idx(index_init_size);
    idx.thread_init();

    {
        TestTransaction t1(0);
        t1.get_tx().mvcc_rw_upgrade();
        key_type key1{1, 1};
        index_value val1{11, 12, 13};
        {
            auto [success, found] = idx.insert_row(key1, &val1);
            assert(success);
            assert(!found);
        }
        assert(t1.try_commit());

        TestTransaction t2(1);
        t2.get_tx().mvcc_rw_upgrade();
        key_type key2{1, 1};
        {
            auto [success, found] = idx.delete_row(key2);
            assert(success);
            assert(found);
        }
        assert(t2.try_commit());

        TestTransaction t3(3);
        t3.get_tx().mvcc_rw_upgrade();
        key_type key3{1, 1};
        index_value val3{14, 15, 16};
        {
            auto [success, found] = idx.insert_row(key3, &val3);
            assert(success);
            assert(!found);
        }
        assert(t3.try_commit());

        TestTransaction t4(1);
        t4.get_tx().mvcc_rw_upgrade();
        key_type key4{1, 1};
        {
            auto[success, result, row, accessor] = idx.select_split_row(key4, {{nc::value_1, access_t::read},
                                                                               {nc::value_2a, access_t::read},
                                                                               {nc::value_2b, access_t::read}});
            (void)row;
            assert(success);
            assert(result);
            assert(accessor.value_1() == 14);
            assert(accessor.value_2a() == 15);
            assert(accessor.value_2b() == 16);
        }
        assert(t4.try_commit());
    }

    printf("Test pass: InsertSerialTest\n");
}

template <bool Ordered>
void MVCCIndexTester<Ordered>::InsertUnreadableTest() {
    index_type idx(index_init_size);
    idx.thread_init();

    {
        TestTransaction t1(0);
        key_type key1{1, 1};
        index_value val1{11, 12, 13};
        {
            auto [success, found] = idx.insert_row(key1, &val1);
            assert(success);
            assert(!found);
        }

        TestTransaction t2(1);
        key_type key2{1, 1};
        {
            auto[success, result, row, accessor] = idx.select_split_row(key2, {{nc::value_1, access_t::read},
                                                                               {nc::value_2a, access_t::read},
                                                                               {nc::value_2b, access_t::read}});
            (void)row;
            (void)accessor;
            assert(success);
            assert(!result);
        }

        assert(t2.try_commit());

        t1.use();
        assert(t1.try_commit());
    }

    printf("Test pass: InsertUnreadableTest\n");
}

template <bool Ordered>
void MVCCIndexTester<Ordered>::UpdateTest() {
    index_type idx(index_init_size);
    idx.thread_init();

    {
        TestTransaction t(0);
        t.get_tx().mvcc_rw_upgrade();
        key_type key{0, 1};
        index_value val{4, 5, 6};

        auto[success, found] = idx.insert_row(key, &val);
        assert(success);
        assert(!found);
        assert(t.try_commit());
    }

    {
        TestTransaction t(1);
        t.get_tx().mvcc_rw_upgrade();
        key_type key{0, 1};
        index_value new_val{7, 0, 0};
        auto[success, result, row, accessor] = idx.select_split_row(key, {{nc::value_1, access_t::update},
                                                                          {nc::value_2a, access_t::read},
                                                                          {nc::value_2b, access_t::read}});
        assert(success);
        assert(result);
        assert(accessor.value_1() == 4);
        assert(accessor.value_2a() == 5);
        assert(accessor.value_2b() == 6);

        idx.update_row(row, &new_val);

        assert(t.try_commit());
    }

    {
        TestTransaction t(0);
        t.get_tx().mvcc_rw_upgrade();
        key_type key{0, 1};
        auto[success, result, row, accessor] = idx.select_split_row(key,
            {{nc::value_1, access_t::read},
             {nc::value_2a, access_t::read},
             {nc::value_2b, access_t::read}});

        (void)row;
        assert(success);
        assert(result);
        assert(accessor.value_1() == 7);
        assert(accessor.value_2a() == 5);
        assert(accessor.value_2b() == 6);

        assert(t.try_commit());
    }

    printf("Test pass: UpdateTest\n");
}

template <bool Ordered>
void MVCCIndexTester<Ordered>::DeleteReinsertTest() {
    using accessor_t = bench::SplitRecordAccessor<index_value>;
    index_type idx(index_init_size);
    idx.thread_init();

    {
        key_type start_key{2, 0};
        key_type end_key{2, 100};
        key_type new_key{2, 21};
        index_value new_value{121, 13, 222};
        index_value newer_value{76, 19, 131};
        key_type another_key{3, 21};
        index_value another_value{9, 9, 4};

        idx.nontrans_put(new_key, new_value);
        idx.nontrans_put(another_key, another_value);

        if constexpr (Ordered) {
            TestTransaction t(0);
            t.get_tx().mvcc_rw_upgrade();

            bool found = false;
            auto scan_callback = [&found] (const key_type&, const auto& split_values) -> bool {
                found |= true;
                accessor_t accessor(split_values);
                assert(accessor.value_1() == 121);
                assert(accessor.value_2a() == 13);
                assert(accessor.value_2b() == 222);
                return true;
            };
            bool success = idx.template range_scan<decltype(scan_callback), false>(
                    start_key, end_key, scan_callback, {
                    {nc::value_1, access_t::read},
                    {nc::value_2a, access_t::read},
                    {nc::value_2b, access_t::read}
                    });
            assert(success);
            assert(found);
            assert(t.try_commit());
        }

        {
            TestTransaction t(0);
            t.get_tx().mvcc_rw_upgrade();
            auto [success, found, row, accessor] = idx.select_split_row(new_key, {
                    {nc::value_1, access_t::read},
                    {nc::value_2a, access_t::read},
                    {nc::value_2b, access_t::read}
                    });
            (void)row;
            assert(success);
            assert(found);
            assert(accessor.value_1() == 121);
            assert(accessor.value_2a() == 13);
            assert(accessor.value_2b() == 222);
            assert(t.try_commit());
        }

        for (auto& t : Transaction::tinfo) {
            t.write_snapshot_epoch = 0;
            t.epoch = 0;
        }

        Transaction::global_epoch_advance_once();
        Transaction::global_epoch_advance_once();

        // t2 is a long-running transaction that observes a soon-to-be-deleted
        // object. The object is deleted and reinserted as a completely
        // different object.
        TestTransaction t2(2);
        {
            t2.get_tx().mvcc_rw_upgrade();
            auto [success, found, row, accessor] = idx.select_split_row(new_key, {
                    {nc::value_1, access_t::read},
                    {nc::value_2a, access_t::read},
                    {nc::value_2b, access_t::read}
                    });
            (void)row;
            assert(success);
            assert(found);
            assert(accessor.value_1() == 121);
            assert(accessor.value_2a() == 13);
            assert(accessor.value_2b() == 222);
        }

        {
            TestTransaction t(0);
            t.get_tx().mvcc_rw_upgrade();

            auto [success, found] = idx.delete_row(new_key);

            assert(success);
            assert(found);

            assert(t.try_commit());
        }

        Transaction::global_epoch_advance_once();
        Transaction::global_epoch_advance_once();

        if constexpr (Ordered) {
            TestTransaction t(0);
            t.get_tx().mvcc_rw_upgrade();

            bool found = false;
            auto scan_callback = [&found] (const key_type&, const auto&) -> bool {
                found |= true;
                return true;
            };
            bool success = idx.template range_scan<decltype(scan_callback), false>(
                    start_key, end_key, scan_callback, {
                    {nc::value_1, access_t::read},
                    {nc::value_2a, access_t::read},
                    {nc::value_2b, access_t::read}
                    });
            assert(success);
            assert(!found);
            assert(t.try_commit());
        }

        {
            TestTransaction t(0);
            t.get_tx().mvcc_rw_upgrade();
            auto [success, found, row, accessor] = idx.select_split_row(new_key, {
                    {nc::value_1, access_t::read},
                    {nc::value_2a, access_t::read},
                    {nc::value_2b, access_t::read}
                    });
            (void)row;
            (void)accessor;
            assert(success);
            assert(!found);
            assert(t.try_commit());
        }

        Transaction::global_epoch_advance_once();
        Transaction::global_epoch_advance_once();

        // Check that MT delete hasn't happened yet
        if constexpr (Ordered) {
            {
                TestTransaction t(0);
                t.try_commit();
            }
            {
                TestTransaction t(2);
                t.try_commit();
            }

            typename index_type::unlocked_cursor_type lp(idx.table_, new_key);
            bool found = lp.find_unlocked(*idx.ti);
            assert(found);
        }

        {
            t2.use();

            {
                index_value val{9, 9, 16};
                auto [success, found] = idx.insert_row(new_key, &val, true);
                assert(success);
                assert(found);
            }

            {
                index_value val{9, 9, 16};
                auto [success, found] = idx.insert_row(another_key, &val, true);
                assert(success);
                assert(found);
            }

            assert(!t2.try_commit());
        }

        for (auto& t : Transaction::tinfo) {
            t.write_snapshot_epoch = 0;
            t.epoch = 0;
        }

        Transaction::global_epoch_advance_once();
        Transaction::global_epoch_advance_once();

        // Check that MT delete has now happened
        if constexpr (Ordered) {
            {
                TestTransaction t(0);
                t.try_commit();
            }
            {
                TestTransaction t(2);
                t.try_commit();
            }

            typename index_type::unlocked_cursor_type lp(idx.table_, new_key);
            bool found = lp.find_unlocked(*idx.ti);
            assert(!found);
        }

        {
            TestTransaction t(1);
            t.get_tx().mvcc_rw_upgrade();

            {
                auto [success, found] = idx.insert_row(new_key, &newer_value, false);

                assert(success);
                assert(!found);
            }

            {
                index_value val{9, 9, 14};
                auto [success, found] = idx.insert_row(another_key, &val, true);
                assert(success);
                assert(found);
            }

            assert(t.try_commit());
        }

        {
            index_value val;
            idx.nontrans_get(new_key, &val);
            assert(val.value_1 == 76);
        }

        {
            index_value val;
            idx.nontrans_get(another_key, &val);
            assert(val.value_2b == 14);
        }
    }

    printf("Test pass: DeleteReinsertTest\n");
}

int main() {
    {
        MVCCIndexTester<true> tester;
        tester.RunTests();
    }
    {
        MVCCIndexTester<false> tester;
        tester.RunTests();
    }
    printf("ALL TESTS PASS, ignore errors after this.\n");
    auto advancer = std::thread(&Transaction::epoch_advancer, nullptr);
    Transaction::rcu_release_all(advancer, 48);
    return 0;
}
