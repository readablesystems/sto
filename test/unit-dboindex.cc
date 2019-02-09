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
    bool success, found;
    uintptr_t row;
    const coarse_grained_row *value;

    {
        TestTransaction t(0);
        std::tie(success, found, row, value) = ci.select_row(key_type(1), {{nc::aa, access_t::read}});
        assert(success && found);
        assert(value->aa == 1);
        assert(t.try_commit());
    }

    {
        TestTransaction t(0);
        std::tie(success, found, row, value) = ci.select_row(key_type(1), {{nc::aa, access_t::update}});
        auto new_row = Sto::tx_alloc(value);
        new_row->aa = 2;
        ci.update_row(row, new_row);
        assert(t.try_commit());

        TestTransaction t1(1);
        std::tie(success, found, row, value) = ci.select_row(key_type(1), {{nc::aa, access_t::read}});
        assert(value->aa == 2);
        assert(t1.try_commit());
    }

    printf("pass %s\n", __FUNCTION__);
}

void test_coarse_read_my_split() {
    typedef CoarseIndex::NamedColumn nc;
    CoarseIndex ci;
    ci.thread_init();

    init_cindex(ci);
    bool success, found;
    uintptr_t row;
    const coarse_grained_row *value;

    {
        TestTransaction t(0);
        std::tie(success, found, row, value) = ci.select_row(key_type(20), {{nc::aa, access_t::read}});
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
    bool success, found;
    uintptr_t row;
    const coarse_grained_row *value;

    {
        TestTransaction t1(0);
        std::tie(success, found, row, value) = ci.select_row(key_type(1), {{nc::aa, access_t::read}});
        assert(success && found);
        assert(value->aa == 1);

        TestTransaction t2(1);
        std::tie(success, found, row, value) = ci.select_row(key_type(1), {{nc::aa, access_t::update}});
        assert(success && found);
        auto new_row = Sto::tx_alloc(value);
        new_row->aa = 2;
        ci.update_row(row, new_row);
        assert(t2.try_commit());

        t1.use();
        assert(!t1.try_commit());
    }

    {
        TestTransaction t1(0);
        coarse_grained_row row_value(100, 100, 100);
        std::tie(success, found) = ci.insert_row(key_type(100), &row_value);
        assert(success && !found);

        TestTransaction t2(0);
        std::tie(success, found, row, value) = ci.select_row(key_type(100), {{nc::aa, access_t::read}});
        assert(!success || !found);

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
    bool success, found;
    uintptr_t row;
    const coarse_grained_row *value;

    {
        TestTransaction t1(0);
        std::tie(success, found, row, value) = ci.select_row(key_type(1), {{nc::aa, access_t::read}});
        assert(success && found);
        assert(value->aa == 1);

        TestTransaction t2(1);
        std::tie(success, found, row, value) = ci.select_row(key_type(1), {{nc::bb, access_t::update}});
        assert(success && found);
        auto new_row = Sto::tx_alloc(value);
        new_row->aa = 2; // Will get installed
        new_row->bb = 2;
        ci.update_row(row, new_row);
        assert(t2.try_commit());

        t1.use();
        assert(value->aa == 2);
        assert(!t1.try_commit()); // expected coarse-grained behavior
    }

    printf("pass %s\n", __FUNCTION__);
}

void test_fine_conflict0() {
    typedef FineIndex::NamedColumn nc;
    FineIndex fi;
    fi.thread_init();

    init_findex(fi);
    bool success, found;
    uintptr_t row;
    const example_row *value;

    {
        TestTransaction t1(0);
        std::tie(success, found, row, value) = fi.select_row(key_type(1), {{nc::ytd, access_t::read}});
        assert(success && found);
        assert(value->d_ytd == 3000);

        TestTransaction t2(1);
        std::tie(success, found, row, value) = fi.select_row(key_type(1), {{nc::ytd, access_t::update}});
        assert(success && found);
        auto new_row = Sto::tx_alloc(value);
        new_row->d_ytd += 10;
        fi.update_row(row, new_row);
        assert(t2.try_commit());

        t1.use();
        assert(value->d_ytd == 3010);
        assert(!t1.try_commit());
    }

    printf("pass %s\n", __FUNCTION__);
}

void test_fine_conflict1() {
    typedef FineIndex::NamedColumn nc;
    FineIndex fi;
    fi.thread_init();

    init_findex(fi);
    bool success, found;
    uintptr_t row;
    const example_row *value;

    {
        TestTransaction t1(0);
        std::tie(success, found, row, value) = fi.select_row(key_type(1), {{nc::ytd, access_t::read}});
        assert(success && found);
        assert(value->d_ytd == 3000);

        TestTransaction t2(1);
        std::tie(success, found, row, value) = fi.select_row(key_type(1), {{nc::payment_cnt, access_t::update}});
        assert(success && found);
        auto new_row = Sto::tx_alloc(value);
        new_row->d_ytd += 10;
        new_row->d_payment_cnt += 1;
        fi.update_row(row, new_row);
        assert(t2.try_commit());

        t1.use();
        assert(value->d_ytd == 3000); // unspecified modifications are not installed
        assert(value->d_payment_cnt == 51);
        assert(!t1.try_commit()); // not able to commit due to hierarchical versions
    }

    printf("pass %s\n", __FUNCTION__);
}

void test_fine_conflict2() {
    typedef FineIndex::NamedColumn nc;
    FineIndex fi;
    fi.thread_init();

    init_findex(fi);
    bool success, found;
    uintptr_t row;
    const example_row *value;

    {
        TestTransaction t1(0);
        std::tie(success, found, row, value) = fi.select_row(key_type(1), {{nc::tax, access_t::read}});
        assert(success && found);
        assert(value->d_tax == 10);

        TestTransaction t2(1);
        std::tie(success, found, row, value) = fi.select_row(key_type(1), {{nc::ytd, access_t::update}});
        assert(success && found);
        auto new_row = Sto::tx_alloc(value);
        new_row->d_ytd += 10;
        new_row->d_payment_cnt += 1;
        fi.update_row(row, new_row);
        assert(t2.try_commit());

        t1.use();
        assert(value->d_ytd == 3010);
        assert(value->d_payment_cnt == 50); // unspecified modifications are not installed
        assert(t1.try_commit()); // can commit because of fine-grained versions
    }

    printf("pass %s\n", __FUNCTION__);
}

void test_mvcc_snapshot() {
    typedef CoarseIndex::NamedColumn nc;
    MVIndex mi;
    mi.thread_init();

    init_cindex(mi);
    bool success, found;
    uintptr_t row;
    const coarse_grained_row *value;

    {
        TestTransaction t1(0);
        std::tie(success, found, row, value) = mi.select_row(key_type(1), RowAccess::ObserveValue);
        assert(success && found);
        assert(value->aa == 1);

        TestTransaction t2(1);
        std::tie(success, found, row, value) = mi.select_row(key_type(1), RowAccess::ObserveValue);
        assert(success && found);
        auto new_row = Sto::tx_alloc(value);
        new_row->aa = 2;
        mi.update_row(row, new_row);
        assert(t2.try_commit());

        t1.use();
        assert(t1.try_commit());
    }

    {
        TestTransaction t1(0);
        coarse_grained_row row_value(100, 100, 100);
        std::tie(success, found) = mi.insert_row(key_type(100), &row_value);
        assert(success && !found);

        TestTransaction t2(0);
        std::tie(success, found, row, value) = mi.select_row(key_type(100), RowAccess::ObserveValue);
        assert(!success || !found);

        t1.use();
        assert(t1.try_commit());
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
    return 0;
}
