#pragma once

#include <string>
#include <random>

#include "TPCC_structs.hh" // tpcc::fix_string
#include "str.hh" // lcdf::Str
#include "Interface.hh"
#include "TPCC_index.hh"

namespace ycsb {

enum class mode_id : int { ReadOnly = 0, MediumContention, HighContention };

struct ycsb_key {
    ycsb_key(uint64_t id) {
        // no scan operations for ycsb,
        // so byte swap is not required.
        w_id = id;
    }
    bool operator==(const ycsb_key& other) const {
        return w_id == other.w_id;
    }
    bool operator!=(const ycsb_key& other) const {
        return !(*this == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t w_id;
};

#if 0
struct ycsb_value {
//    tpcc::fix_string<100>  cols[10];
    tpcc::fix_string<ycsb_col_size> col;
};
#endif

template <typename DBParams>
class ycsb_value : TObject {
public:
    static constexpr size_t col_width = 10;
    static constexpr size_t num_cols = 10;
    typedef tpcc::fix_string<col_width> col_type;
    typedef typename tpcc::get_version<DBParams>::type version_type;

    ycsb_value() : cols(), v0(Sto::initialized_tid(), false)
#if TABLE_FINE_GRAINED
                   , v1(Sto::initialized_tid(), false)
#endif
    {}

    col_type& col_access(int col_n) {
        return cols[col_n];
    }
    const col_type& col_access(int col_n) const {
        return cols[col_n];
    }

    // return: success
    bool trans_col_update(int col_n, const col_type& new_col) {
        always_assert((size_t)col_n < num_cols, "column index out of bound");
#if TABLE_FINE_GRAINED
        auto item = Sto::item(this, col_n);
        auto& v = (col_n % 2 == 0) ? v0 : v1;
        if (!tpcc::version_adapter::select_for_overwrite(item, v, &new_col))
            return false;
#else
        auto item = Sto::item(this, col_n);
        if (!tpcc::version_adapter::select_for_overwrite(item, v0, &new_col))
            return false;
#endif
        return true;
    }

    // return: success, column
    std::pair<bool, const col_type *> trans_col_read(int col_n) {
        always_assert((size_t)col_n < num_cols, "column index out of bound");
#if TABLE_FINE_GRAINED
        auto item = Sto::item(this, col_n);
        if (!item.observe((col_n % 2 == 0) ? v0 : v1))
            return {false, nullptr};
#else
        auto item = Sto::item(this, col_n);
        if (!item.observe(v0))
            return {false, nullptr};
#endif
        return {true, &cols[col_n]};
    }

    // TObject interface methods
    bool lock(TransItem& item, Transaction& txn) override {
#if TABLE_FINE_GRAINED
        version_type& v = (item.key<int>()%2 == 0) ? v0 : v1;
#else
        version_type& v = v0;
#endif
        return txn.try_lock(item, v);
    }

    bool check(TransItem& item, Transaction& txn) override {
#if TABLE_FINE_GRAINED
        version_type& v = (item.key<int>()%2 == 0) ? v0 : v1;
#else
        version_type& v = v0;
#endif
        return v.cp_check_version(txn, item);
    }

    void install(TransItem& item, Transaction& txn) override {
#if TABLE_FINE_GRAINED
        version_type& v = (item.key<int>()%2 == 0) ? v0 : v1;
#else
        version_type& v = v0;
#endif
        auto new_col = item.write_value<col_type *>();
        cols[item.key<int>()] = *new_col;
        txn.set_version_unlock(v, item);
    }

    void unlock(TransItem& item) override {
#if TABLE_FINE_GRAINED
        version_type& v = (item.key<int>()%2 == 0) ? v0 : v1;
#else
        version_type& v = v0;
#endif
        v.cp_unlock(item);
    }

private:
    col_type cols[num_cols];
    version_type v0;
#if TABLE_FINE_GRAINED
    version_type v1;
#endif
};

template <typename DBParams>
class ycsb_input_generator {
public:
    using value_type = ycsb_value<DBParams>;
    ycsb_input_generator(int thread_id)
            : gen(thread_id), dis(0, 61) {}

    value_type random_ycsb_value() {
        value_type ret;
        for (size_t i = 0; i < value_type::num_cols; i++) {
            ret.col_access(i) = random_a_string(value_type::col_width);
        }
        return ret;
    }

    void random_ycsb_col_value_inplace(typename value_type::col_type *dst) {
        for (size_t i = 0; i < value_type::col_width; ++i)
            (*dst)[i] = random_char();
    }

private:
    std::string random_a_string(size_t len) {
        std::string str;
        str.reserve(len);
        for (auto i = 0u; i < len; ++i) {
            str.push_back(random_char());
        }
        return str;
    }

    char random_char() {
        auto n = dis(gen);
        char c = (n < 26) ? char('a' + n) :
                 ((n < 52) ? char('A' + (n - 26)) : char('0' + (n - 52)));
        return c;
    }

    std::mt19937 gen;
    std::uniform_int_distribution<int> dis;
};

}; // namespace ycsb

namespace std {

template <>
struct hash<ycsb::ycsb_key> {
    size_t operator() (const ycsb::ycsb_key& arg) const {
        return arg.w_id;
    }
};

};
