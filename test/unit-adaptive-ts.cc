#include "xxhash.h"
#include "Sto.hh"
#include "DB_params.hh"
#include "DB_index.hh"
#include "Adapter.hh"

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
        value_2b,
        COLCOUNT
    };
    int64_t value_1;
    int64_t value_2a;
    int64_t value_2b;
};

index_value::NamedColumn& operator++(
        index_value::NamedColumn& nc, std::underlying_type_t<index_value::NamedColumn>) {
    nc = static_cast<index_value::NamedColumn>(
            static_cast<std::underlying_type_t<index_value::NamedColumn>>(nc) + 1);
    return nc;
};

using Adapter = ADAPTER_OF(index_value);

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
        Adapter::CountRead(index_value::NamedColumn::value_1);
        return impl().value_1_impl();
    }

    int64_t value_2a() const {
        Adapter::CountRead(index_value::NamedColumn::value_2a);
        return impl().value_2a_impl();
    }

    int64_t value_2b() const {
        Adapter::CountRead(index_value::NamedColumn::value_2b);
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
class AdaptiveIndexTester {
public:
    void RunTests();

private:
    static constexpr size_t index_init_size = 1025;
    using key_type = typename std::conditional<Ordered, bench::masstree_key_adapter<index_key>, index_key>::type;
    using index_type = typename std::conditional<Ordered, mvcc_ordered_index<key_type, index_value, db_params::db_mvcc_params>,
                                                          mvcc_unordered_index<key_type, index_value, db_params::db_mvcc_params>>::type;
    typedef index_value::NamedColumn nc;

    void SelectSplitTest();
    void UpdateTest();
};

template <bool Ordered>
void AdaptiveIndexTester<Ordered>::RunTests() {
    if constexpr (Ordered) {
        printf("Testing Adapter on Ordered Index:\n");
    } else {
        printf("Testing Adapter on Unordered Index:\n");
    }
    SelectSplitTest();
    UpdateTest();
}

template <bool Ordered>
void AdaptiveIndexTester<Ordered>::SelectSplitTest() {
    Adapter::ResetGlobal();
    index_type idx(index_init_size);
    idx.thread_init();

    key_type key{0, 1};
    index_value val{4, 5, 6};
    idx.nontrans_put(key, val);

    {
        TestTransaction t(0);
        Adapter::ResetThread();
        auto[success, result, row, accessor]
            = idx.select_split_row(key,
                                   {{nc::value_1,access_t::read},
                                    {nc::value_2b,access_t::read}});
        (void)row;
        assert(success);
        assert(result);

        assert(accessor.value_1() == 4);
        assert(accessor.value_2b() == 6);
        assert(Adapter::TGetRead(index_value::NamedColumn::value_1) == 1);
        assert(Adapter::TGetRead(index_value::NamedColumn::value_2a) == 0);
        assert(Adapter::TGetRead(index_value::NamedColumn::value_2b) == 1);
        assert(Adapter::TGetWrite(index_value::NamedColumn::value_1) == 0);
        assert(Adapter::TGetWrite(index_value::NamedColumn::value_2a) == 0);
        assert(Adapter::TGetWrite(index_value::NamedColumn::value_2b) == 0);

        assert(t.try_commit());
    }

    {
        TestTransaction t(1);
        Adapter::ResetThread();
        auto[success, result, row, accessor]
            = idx.select_split_row(key,
                                   {{nc::value_1,access_t::read},
                                    {nc::value_2a,access_t::read}});
        (void)row;
        assert(success);
        assert(result);

        assert(accessor.value_1() == 4);
        assert(accessor.value_2a() == 5);
        assert(Adapter::TGetRead(index_value::NamedColumn::value_1) == 1);
        assert(Adapter::TGetRead(index_value::NamedColumn::value_2a) == 1);
        assert(Adapter::TGetRead(index_value::NamedColumn::value_2b) == 0);
        assert(Adapter::TGetWrite(index_value::NamedColumn::value_1) == 0);
        assert(Adapter::TGetWrite(index_value::NamedColumn::value_2a) == 0);
        assert(Adapter::TGetWrite(index_value::NamedColumn::value_2b) == 0);

        assert(t.try_commit());
    }

    assert(Adapter::TGetRead(0, index_value::NamedColumn::value_1) == 1);
    assert(Adapter::TGetRead(0, index_value::NamedColumn::value_2a) == 0);
    assert(Adapter::TGetRead(0, index_value::NamedColumn::value_2b) == 1);
    assert(Adapter::TGetWrite(0, index_value::NamedColumn::value_1) == 0);
    assert(Adapter::TGetWrite(0, index_value::NamedColumn::value_2a) == 0);
    assert(Adapter::TGetWrite(0, index_value::NamedColumn::value_2b) == 0);
    assert(Adapter::TGetRead(1, index_value::NamedColumn::value_1) == 1);
    assert(Adapter::TGetRead(1, index_value::NamedColumn::value_2a) == 1);
    assert(Adapter::TGetRead(1, index_value::NamedColumn::value_2b) == 0);
    assert(Adapter::TGetWrite(1, index_value::NamedColumn::value_1) == 0);
    assert(Adapter::TGetWrite(1, index_value::NamedColumn::value_2a) == 0);
    assert(Adapter::TGetWrite(1, index_value::NamedColumn::value_2b) == 0);

    Adapter::Commit(0);

    assert(Adapter::GetRead(index_value::NamedColumn::value_1) == 1);
    assert(Adapter::GetRead(index_value::NamedColumn::value_2a) == 0);
    assert(Adapter::GetRead(index_value::NamedColumn::value_2b) == 1);
    assert(Adapter::GetWrite(index_value::NamedColumn::value_1) == 0);
    assert(Adapter::GetWrite(index_value::NamedColumn::value_2a) == 0);
    assert(Adapter::GetWrite(index_value::NamedColumn::value_2b) == 0);

    Adapter::Commit(1);

    assert(Adapter::GetRead(index_value::NamedColumn::value_1) == 2);
    assert(Adapter::GetRead(index_value::NamedColumn::value_2a) == 1);
    assert(Adapter::GetRead(index_value::NamedColumn::value_2b) == 1);
    assert(Adapter::GetWrite(index_value::NamedColumn::value_1) == 0);
    assert(Adapter::GetWrite(index_value::NamedColumn::value_2a) == 0);
    assert(Adapter::GetWrite(index_value::NamedColumn::value_2b) == 0);

    printf("Test pass: SelectSplitTest\n");
}

template <bool Ordered>
void AdaptiveIndexTester<Ordered>::UpdateTest() {
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

class AdaptiveTester {
public:
    void RunTests();
private:
    typedef index_value::NamedColumn nc;

    void NoSplitTest();
    void AllWritesSplitTest();
    void SomeWritesSplitTest();
};

void AdaptiveTester::RunTests() {
    printf("Testing Adapter:\n");
    NoSplitTest();
    AllWritesSplitTest();
    SomeWritesSplitTest();
}

void AdaptiveTester::NoSplitTest() {
    // A large read on the first column
    {
        Adapter::ResetGlobal();
        TestTransaction t(0);
        Adapter::ResetThread();
        for (size_t i = 0; i < 100; i++) {
            Adapter::CountRead(index_value::NamedColumn::value_1);
        }
        Adapter::Commit();
        t.try_commit();

        assert(Adapter::ComputeSplitIndex() == 3);
    }

    // A large read on the last column
    {
        Adapter::ResetGlobal();
        TestTransaction t(0);
        Adapter::ResetThread();
        for (size_t i = 0; i < 100; i++) {
            Adapter::CountRead(index_value::NamedColumn::value_2b);
        }
        Adapter::Commit();
        t.try_commit();

        assert(Adapter::ComputeSplitIndex() == 3);
    }

    // Large reads on the first and last columns
    {
        Adapter::ResetGlobal();
        TestTransaction t(0);
        Adapter::ResetThread();
        for (size_t i = 0; i < 100; i++) {
            Adapter::CountRead(index_value::NamedColumn::value_1);
            Adapter::CountRead(index_value::NamedColumn::value_2b);
        }
        Adapter::Commit();
        t.try_commit();

        assert(Adapter::ComputeSplitIndex() == 3);
    }

    // A large write on the first column
    {
        Adapter::ResetGlobal();
        TestTransaction t(0);
        Adapter::ResetThread();
        for (size_t i = 0; i < 100; i++) {
            Adapter::CountWrite(index_value::NamedColumn::value_1);
        }
        Adapter::Commit();
        t.try_commit();

        assert(Adapter::ComputeSplitIndex() == 3);
    }

    // A large write on the last column
    {
        Adapter::ResetGlobal();
        TestTransaction t(0);
        Adapter::ResetThread();
        for (size_t i = 0; i < 100; i++) {
            Adapter::CountWrite(index_value::NamedColumn::value_2b);
        }
        Adapter::Commit();
        t.try_commit();

        assert(Adapter::ComputeSplitIndex() == 3);
    }

    printf("Test pass: NoSplitTest\n");
}

void AdaptiveTester::AllWritesSplitTest() {
    // A large write on the third column, with no reads
    {
        Adapter::ResetGlobal();
        TestTransaction t(0);
        Adapter::ResetThread();
        for (size_t i = 0; i < 100; i++) {
            Adapter::CountWrite(index_value::NamedColumn::value_1);
            Adapter::CountWrite(index_value::NamedColumn::value_2a);
            Adapter::CountWrite(index_value::NamedColumn::value_2b);
            Adapter::CountWrite(index_value::NamedColumn::value_2b);
        }
        Adapter::Commit();
        t.try_commit();

        assert(Adapter::ComputeSplitIndex() == 2);
    }

    // A large write on the third column, with uniform reads
    {
        Adapter::ResetGlobal();
        TestTransaction t(0);
        Adapter::ResetThread();
        for (size_t i = 0; i < 100; i++) {
            Adapter::CountRead(index_value::NamedColumn::value_1);
            Adapter::CountWrite(index_value::NamedColumn::value_1);
            Adapter::CountRead(index_value::NamedColumn::value_2a);
            Adapter::CountWrite(index_value::NamedColumn::value_2a);
            Adapter::CountRead(index_value::NamedColumn::value_2b);
            Adapter::CountWrite(index_value::NamedColumn::value_2b);
            Adapter::CountWrite(index_value::NamedColumn::value_2b);
        }
        Adapter::Commit();
        t.try_commit();

        assert(Adapter::ComputeSplitIndex() == 2);
    }

    printf("Test pass: AllWritesSplitTest\n");
}

void AdaptiveTester::SomeWritesSplitTest() {
    // A large write on the first and third columns, column 2 is read-only
    {
        Adapter::ResetGlobal();
        TestTransaction t(0);
        Adapter::ResetThread();
        for (size_t i = 0; i < 100; i++) {
            Adapter::CountWrite(index_value::NamedColumn::value_1);
            Adapter::CountRead(index_value::NamedColumn::value_2a);
            Adapter::CountWrite(index_value::NamedColumn::value_2b);
        }
        Adapter::Commit();
        t.try_commit();

        const auto split = Adapter::ComputeSplitIndex();
        assert(split == 1 || split == 2);
    }

    // Uniform writes, first column has reads
    {
        Adapter::ResetGlobal();
        TestTransaction t(0);
        Adapter::ResetThread();
        for (size_t i = 0; i < 100; i++) {
            Adapter::CountRead(index_value::NamedColumn::value_1);
            Adapter::CountWrite(index_value::NamedColumn::value_1);
            Adapter::CountWrite(index_value::NamedColumn::value_2a);
            Adapter::CountWrite(index_value::NamedColumn::value_2b);
        }
        Adapter::Commit();
        t.try_commit();

        assert(Adapter::ComputeSplitIndex() == 1);
    }

    // Uniform writes, third column has reads
    {
        Adapter::ResetGlobal();
        TestTransaction t(0);
        Adapter::ResetThread();
        for (size_t i = 0; i < 100; i++) {
            Adapter::CountWrite(index_value::NamedColumn::value_1);
            Adapter::CountWrite(index_value::NamedColumn::value_2a);
            Adapter::CountRead(index_value::NamedColumn::value_2b);
            Adapter::CountWrite(index_value::NamedColumn::value_2b);
        }
        Adapter::Commit();
        t.try_commit();

        assert(Adapter::ComputeSplitIndex() == 2);
    }

    // Uniform writes, first and third columns have equal reads
    {
        Adapter::ResetGlobal();
        TestTransaction t(0);
        Adapter::ResetThread();
        for (size_t i = 0; i < 100; i++) {
            Adapter::CountRead(index_value::NamedColumn::value_1);
            Adapter::CountWrite(index_value::NamedColumn::value_1);
            Adapter::CountWrite(index_value::NamedColumn::value_2a);
            Adapter::CountRead(index_value::NamedColumn::value_2b);
            Adapter::CountWrite(index_value::NamedColumn::value_2b);
        }
        Adapter::Commit();
        t.try_commit();

        const auto split = Adapter::ComputeSplitIndex();
        assert(split == 1 || split == 2);
    }

    // Uniform writes, first and third columns have equal reads
    {
        Adapter::ResetGlobal();
        TestTransaction t(0);
        Adapter::ResetThread();
        for (size_t i = 0; i < 100; i++) {
            Adapter::CountRead(index_value::NamedColumn::value_1);
            Adapter::CountWrite(index_value::NamedColumn::value_1);
            Adapter::CountWrite(index_value::NamedColumn::value_2a);
            Adapter::CountRead(index_value::NamedColumn::value_2b);
            Adapter::CountWrite(index_value::NamedColumn::value_2b);
        }
        Adapter::Commit();
        t.try_commit();

        const auto split = Adapter::ComputeSplitIndex();
        assert(split == 1 || split == 2);
    }

    // Uniform writes, third column has way more reads than the other two
    {
        Adapter::ResetGlobal();
        TestTransaction t(0);
        Adapter::ResetThread();
        for (size_t i = 0; i < 100; i++) {
            Adapter::CountRead(index_value::NamedColumn::value_1);
            Adapter::CountWrite(index_value::NamedColumn::value_1);
            Adapter::CountWrite(index_value::NamedColumn::value_2a);
            Adapter::CountRead(index_value::NamedColumn::value_2b);
            Adapter::CountRead(index_value::NamedColumn::value_2b);
            Adapter::CountWrite(index_value::NamedColumn::value_2b);
        }
        Adapter::Commit();
        t.try_commit();

        const auto split = Adapter::ComputeSplitIndex();
        assert(split == 2);
    }

    printf("Test pass: SomeWritesSplitTest\n");
}

int main() {
    sto::AdapterConfig::Enable(sto::AdapterConfig::Global);

    {
        AdaptiveIndexTester<true> tester;
        tester.RunTests();
    }
    {
        AdaptiveIndexTester<false> tester;
        tester.RunTests();
    }
    {
        AdaptiveTester tester;
        tester.RunTests();
    }
    printf("ALL TESTS PASS, ignore errors after this.\n");
    auto advancer = std::thread(&Transaction::epoch_advancer, nullptr);
    Transaction::rcu_release_all(advancer, 48);
    return 0;
}
