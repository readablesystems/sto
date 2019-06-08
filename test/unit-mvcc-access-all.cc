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
    enum class NamedColumn : int {
        value_1 = 0
    };
    int64_t value_1;
};

struct index_value_part2 {
    enum class NamedColumn : int {
        value_2a = 0,
        value_2b
    };
    int64_t value_2a;
    int64_t value_2b;
};

template <>
struct bench::SplitParams<index_value> {
    using split_type_list = std::tuple<index_value_part2, index_value_part2>;
    static constexpr auto split_builder = std::make_tuple(
        [](index_value_part1* out, index_value* in) -> void {
            out->value_1 = in->value_1;
        },
        [](index_value_part2* out, index_value* in) -> void {
            out->value_2a = in->value_2a;
            out->value_2b = in->value_2b;
        }
    );
    static constexpr auto split_merger = std::make_tuple(
        [](index_value* out, index_value_part1* in1) -> void {
            out->value_1 = in1->value_1;
        },
        [](index_value* out, index_value_part2* in2) -> void {
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

void TestMVCCOrderedIndexSplit() {
    using key_type = bench::masstree_key_adapter<index_key>;
    using index_type = mvcc_ordered_index<key_type, index_value, db_params::db_mvcc_params>;
    typedef index_value::NamedColumn nc;
    index_type idx;
    idx.thread_init();

    key_type key {0, 1};
    index_value val { 4, 5, 6 };
    // XXX Need to implement a new nontrans_split_put()
    //idx.nontrans_put(key, val);

    bool success, result;
    uintptr_t row = 0;

    TestTransaction t(0);
    std::tie(success, result, row, std::ignore)
        = idx.select_split_row(key, {{nc::value_1, access_t::read}, {nc::value_2b, access_t::read}});
    assert(success);
    assert(result);
    assert(t.try_commit());

    printf("Test pass: %s\n", __FUNCTION__);
}

int main() {
    TestMVCCOrderedIndexSplit();
    return 0;
}
