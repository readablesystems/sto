#include "DB_index.hh"
#include "DB_structs.hh"
#include "DB_params.hh"

struct coarse_grained_row {
    enum class NamedColumn : int { aa = 0, bb, cc };

    uint64_t aa;
    uint64_t bb;
    uint64_t cc;

    coarse_grained_row() : aa(), bb(), cc() {}

    coarse_grained_row(uint64_t a, uint64_t b, uint64_t c)
            : aa(a), bb(b), cc(c) {}
};

struct key_type {
    uint64_t id;

    explicit key_type(uint64_t key) : id(bench::bswap(key)) {}
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }
};

// using example_row from VersionSelector.hh

namespace bench {

template <>
struct SplitParams<coarse_grained_row> {
    using split_type_list = std::tuple<coarse_grained_row>;
    using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
    static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

    static constexpr auto split_builder = std::make_tuple(
        [](const coarse_grained_row& in) -> coarse_grained_row {
            coarse_grained_row out;
            out.aa = in.aa;
            out.bb = in.bb;
            out.cc = in.cc;
            return out;
        }
    );

    static constexpr auto split_merger = std::make_tuple(
        [](coarse_grained_row* out, const coarse_grained_row& in) -> void {
            out->aa = in.aa;
            out->bb = in.bb;
            out->cc = in.cc;
        }
    );

    static constexpr auto map = [](int col_n) -> int {
        (void)col_n;
        return 0;
    };
};

template <typename A>
class RecordAccessor<A, coarse_grained_row> {
public:
    const uint64_t& aa() const {
        return impl().aa_impl();
    }

    const uint64_t& bb() const {
        return impl().bb_impl();
    }

    const uint64_t& cc() const {
        return impl().cc_impl();
    }

    void copy_into(coarse_grained_row* dst) const {
        return impl().copy_into_impl(dst);
    }

private:
    const A& impl() const {
        return *static_cast<const A*>(this);
    }
};

template <>
class UniRecordAccessor<coarse_grained_row> : public RecordAccessor<UniRecordAccessor<coarse_grained_row>, coarse_grained_row> {
public:
    UniRecordAccessor(const coarse_grained_row* const vptr) : vptr_(vptr) {}

private:
    const uint64_t& aa_impl() const {
        return vptr_->aa;
    }

    const uint64_t& bb_impl() const {
        return vptr_->bb;
    }

    const uint64_t& cc_impl() const {
        return vptr_->cc;
    }

    void copy_into_impl(coarse_grained_row* dst) const {
        if (vptr_) {
            dst->aa = vptr_->aa;
            dst->bb = vptr_->bb;
            dst->cc = vptr_->cc;
        }
    }

    const coarse_grained_row* vptr_;
    friend RecordAccessor<UniRecordAccessor<coarse_grained_row>, coarse_grained_row>;
};

template <>
class SplitRecordAccessor<coarse_grained_row> : public RecordAccessor<SplitRecordAccessor<coarse_grained_row>, coarse_grained_row> {
public:
    static constexpr size_t num_splits = SplitParams<coarse_grained_row>::num_splits;

    SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
        : vptr_0_(reinterpret_cast<coarse_grained_row*>(vptrs[0])) {}

private:
    const uint64_t& aa_impl() const {
        return vptr_0_->aa;
    }

    const uint64_t& bb_impl() const {
        return vptr_0_->bb;
    }

    const uint64_t& cc_impl() const {
        return vptr_0_->cc;
    }

    void copy_into_impl(coarse_grained_row* dst) const {
        if (vptr_0_) {
            dst->aa = vptr_0_->aa;
            dst->bb = vptr_0_->bb;
            dst->cc = vptr_0_->cc;
        }
    }

    const coarse_grained_row* vptr_0_;

    friend RecordAccessor<SplitRecordAccessor<coarse_grained_row>, coarse_grained_row>;
};

template <>
struct SplitParams<example_row> {
    using split_type_list = std::tuple<example_row>;
    using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
    static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

    static constexpr auto split_builder = std::make_tuple(
        [](const example_row& in) -> example_row {
            example_row out;
            out.d_ytd = in.d_ytd;
            out.d_payment_cnt = in.d_payment_cnt;
            out.d_date = in.d_date;
            out.d_tax = in.d_tax;
            out.d_next_oid = in.d_next_oid;
            return out;
        }
    );

    static constexpr auto split_merger = std::make_tuple(
        [](example_row* out, const example_row& in) -> void {
            out->d_ytd = in.d_ytd;
            out->d_payment_cnt = in.d_payment_cnt;
            out->d_date = in.d_date;
            out->d_tax = in.d_tax;
            out->d_next_oid = in.d_next_oid;
        }
    );

    static constexpr auto map = [](int col_n) -> int {
        (void)col_n;
        return 0;
    };
};

template <typename A>
class RecordAccessor<A, example_row> {
public:
    const uint32_t& d_ytd() const {
        return impl().d_ytd_impl();
    }

    const uint32_t& d_payment_cnt() const {
        return impl().d_payment_cnt_impl();
    }

    const uint32_t& d_date() const {
        return impl().d_date_impl();
    }

    const uint32_t& d_tax() const {
        return impl().d_tax_impl();
    }

    const uint32_t& d_next_oid() const {
        return impl().d_next_oid_impl();
    }

    void copy_into(example_row* dst) const {
        return impl().copy_into_impl(dst);
    }

private:
    const A& impl() const {
        return *static_cast<const A*>(this);
    }
};

template <>
class UniRecordAccessor<example_row> : public RecordAccessor<UniRecordAccessor<example_row>, example_row> {
public:
    UniRecordAccessor(const example_row* const vptr) : vptr_(vptr) {}

private:
    const uint32_t& d_ytd_impl() const {
        return vptr_->d_ytd;
    }

    const uint32_t& d_payment_cnt_impl() const {
        return vptr_->d_payment_cnt;
    }

    const uint32_t& d_date_impl() const {
        return vptr_->d_date;
    }

    const uint32_t& d_tax_impl() const {
        return vptr_->d_tax;
    }

    const uint32_t& d_next_oid_impl() const {
        return vptr_->d_next_oid;
    }

    void copy_into_impl(example_row* dst) const {
        if (vptr_) {
            dst->d_ytd = vptr_->d_ytd;
            dst->d_payment_cnt = vptr_->d_payment_cnt;
            dst->d_date = vptr_->d_date;
            dst->d_tax = vptr_->d_tax;
            dst->d_next_oid = vptr_->d_next_oid;
        }
    }

    const example_row* vptr_;
    friend RecordAccessor<UniRecordAccessor<example_row>, example_row>;
};

template <>
class SplitRecordAccessor<example_row> : public RecordAccessor<SplitRecordAccessor<example_row>, example_row> {
public:
    static constexpr size_t num_splits = SplitParams<example_row>::num_splits;

    SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
        : vptr_0_(reinterpret_cast<example_row*>(vptrs[0])) {}

private:
    const uint32_t& d_ytd_impl() const {
        return vptr_0_->d_ytd;
    }

    const uint32_t& d_payment_cnt_impl() const {
        return vptr_0_->d_payment_cnt;
    }

    const uint32_t& d_date_impl() const {
        return vptr_0_->d_date;
    }

    const uint32_t& d_tax_impl() const {
        return vptr_0_->d_tax;
    }

    const uint32_t& d_next_oid_impl() const {
        return vptr_0_->d_next_oid;
    }

    void copy_into_impl(example_row* dst) const {
        if (vptr_0_) {
            dst->d_ytd = vptr_0_->d_ytd;
            dst->d_payment_cnt = vptr_0_->d_payment_cnt;
            dst->d_date = vptr_0_->d_date;
            dst->d_tax = vptr_0_->d_tax;
            dst->d_next_oid = vptr_0_->d_next_oid;
        }
    }

    const example_row* vptr_0_;

    friend RecordAccessor<SplitRecordAccessor<example_row>, example_row>;
};

};  // namespace bench

using CoarseIndex = bench::ordered_index<key_type, coarse_grained_row, db_params::db_default_params>;
using FineIndex = bench::ordered_index<key_type, example_row, db_params::db_default_params>;
using access_t = bench::access_t;
using RowAccess = bench::RowAccess;

using MVIndex = bench::mvcc_ordered_index<key_type, coarse_grained_row, db_params::db_mvcc_params>;

template <typename IndexType>
void init_cindex(IndexType& ci) {
    for (uint64_t i = 1; i <= 10; ++i)
        ci.nontrans_put(key_type(i), coarse_grained_row(i, i, i));
}

void init_findex(FineIndex& fi) {
    example_row row;
    row.d_ytd = 3000;
    row.d_tax = 10;
    row.d_date = 200;
    row.d_next_oid = 100;
    row.d_payment_cnt = 50;

    for (uint64_t i = 1; i <= 10; ++i)
        fi.nontrans_put(key_type(i), row);
}

void test_coarse_basic() {
    typedef CoarseIndex::NamedColumn nc;
    CoarseIndex ci;
    ci.thread_init();

    init_cindex(ci);

    {
        TestTransaction t(0);
        auto [success, found, row, value] = ci.select_split_row(key_type(1), {{nc::aa, access_t::read}});
        (void) row;
        assert(success && found);
        assert(value.aa() == 1);
        assert(t.try_commit());
    }

    {
        TestTransaction t(0);
        auto [success, found, row, value] = ci.select_split_row(key_type(1), {{nc::aa, access_t::update}});
        (void) row;
        assert(success && found);
        auto new_row = Sto::tx_alloc<coarse_grained_row>();
        value.copy_into(new_row);
        new_row->aa = 2;
        ci.update_row(row, new_row);
        assert(t.try_commit());
    }

    {
        TestTransaction t1(1);
        auto [success, found, row, value] = ci.select_split_row(key_type(1), {{nc::aa, access_t::read}});
        (void) row;
        assert(success && found);
        assert(value.aa() == 2);
        assert(t1.try_commit());
    }

    printf("pass %s\n", __FUNCTION__);
}

void test_coarse_read_my_split() {
    typedef CoarseIndex::NamedColumn nc;
    CoarseIndex ci;
    ci.thread_init();

    init_cindex(ci);

    {
        TestTransaction t(0);
        auto [success, found, row, value] = ci.select_split_row(key_type(20), {{nc::aa, access_t::read}});
        (void) row;
        (void) value;
        assert(success && !found);
        for (int i = 0; i < 10; ++i) {
            auto r = Sto::tx_alloc<coarse_grained_row>();
            new (r) coarse_grained_row(i, i, i);
            ci.insert_row(key_type(10 + i), r);
        }
        assert(t.try_commit());
    }

    printf("pass %s\n", __FUNCTION__);
}

void test_coarse_conflict0() {
    typedef CoarseIndex::NamedColumn nc;
    CoarseIndex ci;
    ci.thread_init();

    init_cindex(ci);

    {
        TestTransaction t1(0);
        {
            auto [success, found, row, value] = ci.select_split_row(key_type(1), {{nc::aa, access_t::read}});
            (void) row;
            assert(success && found);
            assert(value.aa() == 1);
        }

        TestTransaction t2(1);
        {
            auto [success, found, row, value] = ci.select_split_row(key_type(1), {{nc::aa, access_t::update}});
            assert(success && found);
            auto new_row = Sto::tx_alloc<coarse_grained_row>();
            value.copy_into(new_row);
            new_row->aa = 2;
            ci.update_row(row, new_row);
            assert(t2.try_commit());
        }

        t1.use();
        assert(!t1.try_commit());
    }

    {
        TestTransaction t1(0);
        coarse_grained_row row_value(100, 100, 100);
        {
            auto [success, found] = ci.insert_row(key_type(100), &row_value);
            assert(success && !found);
        }

        TestTransaction t2(0);
        {
            auto [success, found, row, value] = ci.select_split_row(key_type(100), {{nc::aa, access_t::read}});
            (void) row;
            (void) value;
            assert(!success || !found);
        }

        t1.use();
        assert(t1.try_commit());
    }

    printf("pass %s\n", __FUNCTION__);
}

void test_coarse_conflict1() {
    typedef CoarseIndex::NamedColumn nc;
    CoarseIndex ci;
    ci.thread_init();

    init_cindex(ci);

    {
        TestTransaction t1(0);
        {
            auto [success, found, row, value] = ci.select_split_row(key_type(1), {{nc::aa, access_t::read}});
            (void) row;
            assert(success && found);
            assert(value.aa() == 1);
        }

        TestTransaction t2(1);
        {
            auto [success, found, row, value] = ci.select_split_row(key_type(1), {{nc::bb, access_t::update}});
            assert(success && found);
            auto new_row = Sto::tx_alloc<coarse_grained_row>();
            value.copy_into(new_row);
            new_row->aa = 2; // Will get installed
            new_row->bb = 2;
            ci.update_row(row, new_row);
            assert(t2.try_commit());

            t1.use();
            assert(value.aa() == 2);
            assert(!t1.try_commit()); // expected coarse-grained behavior
        }
    }

    printf("pass %s\n", __FUNCTION__);
}

void test_fine_conflict0() {
    typedef FineIndex::NamedColumn nc;
    FineIndex fi;
    fi.thread_init();

    init_findex(fi);

    {
        TestTransaction t1(0);
        {
            auto [success, found, row, value] = fi.select_split_row(key_type(1), {{nc::ytd, access_t::read}});
            (void) row;
            assert(success && found);
            assert(value.d_ytd() == 3000);
        }

        TestTransaction t2(1);
        {
            auto [success, found, row, value] = fi.select_split_row(key_type(1), {{nc::ytd, access_t::update}});
            assert(success && found);
            auto new_row = Sto::tx_alloc<example_row>();
            value.copy_into(new_row);
            new_row->d_ytd += 10;
            fi.update_row(row, new_row);
            assert(t2.try_commit());

            t1.use();
            assert(value.d_ytd() == 3010);
            assert(!t1.try_commit());
        }
    }

    printf("pass %s\n", __FUNCTION__);
}

void test_fine_conflict1() {
    typedef FineIndex::NamedColumn nc;
    FineIndex fi;
    fi.thread_init();

    init_findex(fi);

    {
        TestTransaction t1(0);
        {
            auto [success, found, row, value] = fi.select_split_row(key_type(1), {{nc::ytd, access_t::read}});
            (void) row;
            assert(success && found);
            assert(value.d_ytd() == 3000);
        }

        TestTransaction t2(1);
        {
            auto [success, found, row, value] = fi.select_split_row(key_type(1), {{nc::payment_cnt, access_t::update}});
            assert(success && found);
            auto new_row = Sto::tx_alloc<example_row>();
            value.copy_into(new_row);
            new_row->d_ytd += 10;
            new_row->d_payment_cnt += 1;
            fi.update_row(row, new_row);
            assert(t2.try_commit());

            t1.use();
            assert(value.d_ytd() == 3000); // unspecified modifications are not installed
            assert(value.d_payment_cnt() == 51);
            assert(!t1.try_commit()); // not able to commit due to hierarchical versions
        }
    }

    printf("pass %s\n", __FUNCTION__);
}

void test_fine_conflict2() {
    typedef FineIndex::NamedColumn nc;
    FineIndex fi;
    fi.thread_init();

    init_findex(fi);

    {
        TestTransaction t1(0);
        {
            auto [success, found, row, value] = fi.select_split_row(key_type(1), {{nc::tax, access_t::read}});
            (void) row;
            assert(success && found);
            assert(value.d_tax() == 10);
        }

        TestTransaction t2(1);
        {
            auto [success, found, row, value] = fi.select_split_row(key_type(1), {{nc::ytd, access_t::update}});
            assert(success && found);
            auto new_row = Sto::tx_alloc<example_row>();
            value.copy_into(new_row);
            new_row->d_ytd += 10;
            new_row->d_payment_cnt += 1;
            fi.update_row(row, new_row);
            assert(t2.try_commit());

            t1.use();
            assert(value.d_ytd() == 3010);
            assert(value.d_payment_cnt() == 50); // unspecified modifications are not installed
            assert(t1.try_commit()); // can commit because of fine-grained versions
        }
    }

    printf("pass %s\n", __FUNCTION__);
}

void test_mvcc_snapshot() {
    typedef CoarseIndex::NamedColumn nc;
    MVIndex mi;
    mi.thread_init();

    init_cindex(mi);

    {
        TestTransaction t1(0);
        {
            auto [success, found, row, value] = mi.select_split_row(key_type(1), {{nc::aa, access_t::read}});
            (void) row;
            assert(success && found);
            assert(value.aa() == 1);
        }

        TestTransaction t2(1);
        {
            auto [success, found, row, value] = mi.select_split_row(key_type(1), {{nc::aa, access_t::update}});
            assert(success && found);
            auto new_row = Sto::tx_alloc<coarse_grained_row>();
            value.copy_into(new_row);
            new_row->aa = 2;
            mi.update_row(row, new_row);
            assert(t2.try_commit());

            t1.use();
            assert(t1.try_commit());
        }
    }

    {
        TestTransaction t1(0);
        {
            coarse_grained_row row_value(100, 100, 100);
            auto [success, found] = mi.insert_row(key_type(100), &row_value);
            assert(success && !found);
        }

        TestTransaction t2(0);
        {
            auto [success, found, row, value] = mi.select_split_row(key_type(100), {{nc::aa, access_t::read}});
            (void) row;
            (void) value;
            assert(!success || !found);

            t1.use();
            assert(t1.try_commit());
        }
    }

    printf("pass %s\n", __FUNCTION__);
}

int main() {
    test_coarse_basic();
    test_coarse_read_my_split();
    test_coarse_conflict0();
    test_coarse_conflict1();
    test_fine_conflict0();
    test_fine_conflict1();
    test_fine_conflict2();
    test_mvcc_snapshot();
    printf("All tests pass!\n");

    std::thread advancer;  // empty thread because we have no advancer thread
    Transaction::rcu_release_all(advancer, 2);
    return 0;
}
