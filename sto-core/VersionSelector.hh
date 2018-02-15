#pragma once

#include "VersionBase.hh"

namespace ver_sel {

///////////////////////////////
// @begin Common definitions //
///////////////////////////////

typedef TransactionTid::type type;

// Version selector interface
template <typename T>
class VerSelBase {
public:
    int map(int col_n) {
        return impl().map_impl();
    }

    T::version_type& select(int col_n) {
        return impl().select_impl();
    }
    T::version_type& version_at(int cell) {
        return impl().version_at_impl(cell);
    }

private:
    T& impl() {
        return static_cast<T&>(*this);
    }
    const T& impl() const {
        return static_cast<const T&>(*this);
    }
};

// Default version selector has only one version for the whole row
// and always returns that version
template <typename RowType, typename VersImpl>
class VerSel : public VerSelBase<VerSel<RowType, VersImpl>> {
public:
    typedef typename VersImpl version_type;

    explicit VerSel(type v) : vers_(v) {}
    VerSel(type v, bool insert) : vers_(v, insert) {}

    int map_impl(int col_n) {
        (void)col_n;
        return 0;
    }

    version_type& select_impl(int col_n) {
        (void)col_n;
        return vers_;
    }

    version_type& version_at_impl(int cell) {
        (void)cell;
        return vers_;
    }

private:
    version_type vers_;
};

}; // namespace ver_sel

// This is the actual "value type" to be put into the index
template <typename RowType, typename VersImpl>
class IndexValueContainer : ver_sel::VerSel<RowType, VersImpl> {
public:
    typedef TransactionTid::type type;
    using Base = ver_sel::VerSel<RowType, VersImpl>;

    IndexValueContainer(type v, const RowType& r) : Base(v), row(r) {}
    IndexValueContainer(type v, bool insert, const RowType& r) : Base(v), row(r) {}

    RowType row;
};

/////////////////////////////
// @end Common definitions //
/////////////////////////////

// EXAMPLE:
//
// This is an example of a row layout
class example_row {
public:
    uint32_t d_ytd;
    uint32_t d_payment_cnt;
    uint32_t d_date;
    uint32_t d_tax;
    uint32_t d_next_oid;
};
// The programer can annotate something like this:
//
// @@@DefineSelector
// class example_row:
//   d_ytd, d_payment_cnt, d_date, d_tax, d_next_oid
// group:
//   {d_ytd}, {d_payment_cnt, d_date, d_tax, d_next_oid}
// @@@SelectorDefined


namespace ver_sel {

// This is the version selector class that should be auto-generated
template <typename VersImpl>
class VerSel<example_row, VersImpl> : public VerSelBase<VerSel<example_row, VersImpl>> {
public:
    typedef typename VersImpl version_type;
    static constexpr size_t num_versions = 2;

    int map_impl(int col_n) {
        if (col_n == 0)
            return 0;
        else
            return 1;
    }

    version_type& select_impl(int col_n) {
        return vers_[map_impl(col_n)];
    }

    version_type& version_at_impl(int cell) {
        return vers_[cell];
    }

private:
    version_type vers_[num_versions];
};

}; // namespace ver_sel

// And in our DB index we can simply use the following type as value type:
// (assuming we use bench::ordered_index<K, V, DBParams>)
// IndexValueContainer<V, version_type>;
