#pragma once

#include "MVCC.hh"
#include "VersionBase.hh"

namespace ver_sel {

///////////////////////////////
// @begin Common definitions //
///////////////////////////////

typedef TransactionTid::type type;

// Version selector interface
template <typename T, typename VersImpl>
class VerSelBase {
public:
    typedef VersImpl version_type;

    constexpr static int map(int col_n) {
        return T::map_impl(col_n);
    }

    version_type& version_at(int cell) {
        return impl().version_at_impl(cell);
    }

    template <typename RowType>
    void install_by_cell(RowType *dst, const RowType *src, int cell) {
        return impl().install_by_cell_impl(dst, src, cell);
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
class VerSel : public VerSelBase<VerSel<RowType, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 1;

    explicit VerSel(type v) : vers_(v) {}
    VerSel(type v, bool insert) : vers_(v, insert) {}

    constexpr static int map_impl(int col_n) {
        (void)col_n;
        return 0;
    }

    version_type& version_at_impl(int cell) {
        (void)cell;
        return vers_;
    }

    void install_by_cell_impl(RowType *dst, const RowType *src, int cell) {
        (void)cell;
        *dst = *src;
    }

private:
    version_type vers_;
};

}  // namespace ver_sel

// This is the actual "value type" to be put into the index
template <typename RowType, typename VersImpl>
class IndexValueContainer : ver_sel::VerSel<RowType, VersImpl> {
public:
    typedef TransactionTid::type type;
    using Selector = ver_sel::VerSel<RowType, VersImpl>;
    typedef typename Selector::version_type version_type;
    using Selector::map;
    using Selector::version_at;
    using Selector::num_versions;
    typedef commutators::Commutator<RowType> comm_type;

    IndexValueContainer(type v, const RowType& r) : Selector(v), row(r) {}
    IndexValueContainer(type v, bool insert, const RowType& r) : Selector(v, insert), row(r) {}

    RowType row;

    void install_cell(const comm_type &comm) {
        comm.operate(row);
    }

    void install_cell(int cell, const RowType *new_row) {
        Selector::install_by_cell(&row, new_row, cell);
    }

    // version_at(0) is always the row-wise version
    version_type& row_version() {
        return version_at(0);
    }
};

/////////////////////////////
// @end Common definitions //
/////////////////////////////

// EXAMPLE:
//
// This is an example of a row layout
struct example_row {
    enum class NamedColumn : int { ytd = 0, payment_cnt, date, tax, next_oid };

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
class VerSel<example_row, VersImpl> : public VerSelBase<VerSel<example_row, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 2;

    explicit VerSel(type v) : vers_() {
        new (&vers_[0]) version_type(v);
    }
    VerSel(type v, bool insert) : vers_() {
        new (&vers_[0]) version_type(v, insert);
    }

    constexpr static int map_impl(int col_n) {
        if (col_n == 0)
            return 0;
        else
            return 1;
    }

    version_type& version_at_impl(int cell) {
        return vers_[cell];
    }

    void install_by_cell_impl(example_row *dst, const example_row *src, int cell) {
        if (cell == 0) {
            dst->d_ytd = src->d_ytd;
        } else {
            dst->d_payment_cnt = src->d_payment_cnt;
            dst->d_date = src->d_date;
            dst->d_tax = src->d_tax;
            dst->d_next_oid = src->d_next_oid;
        }
    }

private:
    version_type vers_[num_versions];
};

}  // namespace ver_sel

// And in our DB index we can simply use the following type as value type:
// (assuming we use bench::ordered_index<K, V, DBParams>)
// IndexValueContainer<V, version_type>;
