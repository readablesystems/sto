#include "Sto.hh"
#include "DB_params.hh"
#include "DB_index.hh"

using bench::split_version_helpers;
using bench::mvcc_ordered_index;
using bench::RowAccess;
using bench::access_t;

struct index_key {
    index_key(int32_t k1, int32_t k2)
        : key_1(bench::bswap(k1)), key_2(bench::bswap(k2)) {}
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
    int32_t value_1() const {
        return impl().value_1_impl();
    }

    int32_t value_2a() const {
        return impl().value_2a_impl();

    }

    int32_t value_2b() const {
        return impl().value_2b_impl();
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
    int32_t value_1_impl() const {
        return vptr_->value_1;
    }

    int32_t value_2a_impl() const {
        return vptr_->value_2a;
    }

    int32_t value_2b_impl() const {
        return vptr_->value_2b;
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
    int32_t value_1_impl() const {
        return vptr_1_->value_1;
    }

    int32_t value_2a_impl() const {
        return vptr_2_->value_2a;
    }

    int32_t value_2b_impl() const {
        return vptr_2_->value_2b;
    }

    const index_value_part1 *vptr_1_;
    const index_value_part2 *vptr_2_;

    friend RecordAccessor<SplitRecordAccessor<index_value>, index_value>;
};
}  // namespace bench

void TestMVCCOrderedIndexSplit() {
    using key_type = bench::masstree_key_adapter<index_key>;
    using index_type = mvcc_ordered_index<key_type,
                                          index_value,
                                          db_params::db_mvcc_params>;
    typedef index_value::NamedColumn nc;
    index_type idx;
    idx.thread_init();

    key_type key{0, 1};
    index_value val{4, 5, 6};
    idx.nontrans_put(key, val);

    TestTransaction t(0);
    auto[success, result, row, accessor]
        = idx.select_split_row(key,
                               {{nc::value_1,access_t::read},
                                {nc::value_2b,access_t::read}});
    assert(success);
    assert(result);

    assert(accessor.value_1() == 4);
    assert(accessor.value_2b() == 6);

    assert(t.try_commit());

    printf("Test pass: %s\n", __FUNCTION__);
}

void TestMVCCOrderedIndexDelete() {
    using key_type = bench::masstree_key_adapter<index_key>;
    using index_type = mvcc_ordered_index<key_type,
                                          index_value,
                                          db_params::db_mvcc_params>;
    typedef index_value::NamedColumn nc;
    index_type idx;
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

            assert(success);
            assert(result);
        }

        assert(t1.try_commit());

        assert(t2.try_commit());
    }

    // Serialized observation should observe delete
    {
        TestTransaction t(0);
        auto[success, result, row, accessor]
            = idx.select_split_row(key,
                                   {{nc::value_1,access_t::read},
                                    {nc::value_2b,access_t::read}});

        assert(success);
        assert(!result);

        assert(t.try_commit());
    }

    printf("Test pass: %s\n", __FUNCTION__);
}

int main() {
    TestMVCCOrderedIndexSplit();
    TestMVCCOrderedIndexDelete();
    return 0;
}
