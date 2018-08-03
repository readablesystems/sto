#undef NDEBUG
#include <fstream>
#include <string>
#include <thread>
#include <iostream>
#include <cassert>
#include <vector>
#include "Sto.hh"
#include "TART.hh"
#include "Transaction.hh"
#include <unistd.h>
#include <random>
#include <time.h>
#include "DB_index.hh"
#include "DB_params.hh"

const char* absentkey1 = "hello";
const char* absentkey2 = "1234";
const char* absentkey2_1 = "1245";
const char* absentkey2_2 = "1256";
const char* absentkey2_3 = "1267";
const char* absentkey2_4 = "1278";
const char* absentkey2_5 = "1289";

const char* checkkey = "check1";
const char* checkkey2 = "check2";
const char* checkkey3 = "check3";

class keyval_db {
public:
    virtual void insert(lcdf::Str int_key, uintptr_t val) = 0;
    virtual void update(lcdf::Str int_key, uintptr_t val) = 0;
    virtual uintptr_t lookup(lcdf::Str int_key) = 0;
    virtual void erase(lcdf::Str int_key) = 0;
};

class masstree_wrapper : public keyval_db {
public:
    struct oi_value {
        enum class NamedColumn : int { val = 0 };
        uintptr_t val;
    };

    typedef bench::ordered_index<lcdf::Str, oi_value, db_params::db_default_params> index_type;
    index_type oi;

    masstree_wrapper() {
    }

    void insert(lcdf::Str key, uintptr_t val) override {
        bool success;
        std::tie(success, std::ignore) = oi.insert_row(key, new oi_value{val});
        if (!success) throw Transaction::Abort();
    }

    uintptr_t lookup(lcdf::Str key) override {
        uintptr_t ret;
        bool success;
        const oi_value* val;
        std::tie(success, std::ignore, std::ignore, val) = oi.select_row(key, bench::RowAccess::None);
        if (!success) throw Transaction::Abort();
        if (!val) {
            ret = 0;
        } else {
            ret = val->val;
        }
        return ret;
    }

    void update(lcdf::Str key, uintptr_t val) override {
        bool success;
        uintptr_t row;
        const oi_value* value;
        std::tie(success, std::ignore, row, value) = oi.select_row(key, bench::RowAccess::UpdateValue);
        if (!success) throw Transaction::Abort();
        auto new_oiv = Sto::tx_alloc<oi_value>(value);
        new_oiv->val = val;
        oi.update_row(row, new_oiv);
    }

    void erase(lcdf::Str key) override {
        oi.delete_row(key);
    }
};

class tart_wrapper : public keyval_db {
public:
    struct oi_value {
        enum class NamedColumn : int { val = 0 };
        uintptr_t val;
    };

    typedef bench::art_index<lcdf::Str, oi_value, db_params::db_default_params> index_type;
    index_type oi;

    tart_wrapper() {
    }

    void insert(lcdf::Str key, uintptr_t val) override {
        bool success;
        std::tie(success, std::ignore) = oi.insert_row(key, new oi_value{val});
        if (!success) throw Transaction::Abort();
    }

    uintptr_t lookup(lcdf::Str key) override {
        uintptr_t ret;
        bool success;
        bool found;
        const oi_value* val;
        std::tie(success, std::ignore, std::ignore, val) = oi.select_row(key, bench::RowAccess::None);
        if (!success) throw Transaction::Abort();
        if (!val) {
            ret = 0;
        } else {
            ret = val->val;
        }
        return ret;
    }

    void update(lcdf::Str key, uintptr_t val) override {
        bool success;
        uintptr_t row;
        const oi_value* value;
        std::tie(success, std::ignore, row, value) = oi.select_row(key, bench::RowAccess::UpdateValue);
        if (!success) throw Transaction::Abort();
        auto new_oiv = Sto::tx_alloc<oi_value>(value);
        new_oiv->val = val;
        oi.update_row(row, new_oiv);
    }

    void erase(lcdf::Str key) override {
        oi.delete_row(key);
    }
};

typedef tart_wrapper wrapper_type;

void testSimple() {
    wrapper_type a;

    const char* key1 = "hello world";
    const char* key2 = "1234";
    {
        TransactionGuard t;
        a.insert(key1, 123);
        a.insert(key2, 321);
    }

    {
        TransactionGuard t;
        volatile auto x = a.lookup(key1);
        volatile auto y = a.lookup(key2);
        assert(x == 123);
        assert(y == 321);
    }

    // {
    //     TransactionGuard t;
    //     a.insert("foo", 1);
    //     a.insert("foobar", 2);
    // }
    //
    // {
    //     TransactionGuard t;
    //     assert(a.lookup("foobar") == 2);
    // }


    printf("PASS: %s\n", __FUNCTION__);
}

void testSimpleErase() {
    wrapper_type a;

    const char* key1 = "hello world";
    const char* key2 = "1234";
    {
        TransactionGuard t;
        a.insert(key1, 123);
        a.insert(key2, 321);
    }

    {
        TransactionGuard t;
        volatile auto x = a.lookup(key1);
        volatile auto y = a.lookup(key2);
        a.insert(checkkey, 100);
        assert(x == 123);
        assert(y == 321);
    }

    {
        TransactionGuard t;
        a.erase(key1);
        volatile auto x = a.lookup(key1);
        a.insert(checkkey, 100);
        assert(x == 0);
    }

    {
        TransactionGuard t;
        volatile auto x = a.lookup(key1);
        assert(x == 0);
        a.insert(key1, 567);
    }

    {
        TransactionGuard t;
        volatile auto x = a.lookup(key1);
        a.insert(checkkey, 100);
        assert(x == 567);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testAbsentErase() {
    wrapper_type a;

    TestTransaction t1(0);
    a.erase("foo");
    a.insert("bar", 1);

    TestTransaction t2(1);
    a.insert("foo", 123);
    assert(t2.try_commit());

    assert(!t1.try_commit());
    printf("PASS: %s\n", __FUNCTION__);
}

void multiWrite() {
    wrapper_type aTART;
    {
        TransactionGuard t;
        aTART.insert(absentkey2, 456);
    }

    {
        TransactionGuard t;
        aTART.update(absentkey2, 123);
    }
    {
        TransactionGuard t;
        volatile auto x = aTART.lookup(absentkey2);
        assert(x == 123);    
    }
    printf("PASS: %s\n", __FUNCTION__);
}

void multiThreadWrites() {
    wrapper_type aTART;
    {
        TransactionGuard t;
        aTART.insert(absentkey2, 456);
    }

    TestTransaction t1(0);
    aTART.insert(absentkey2, 123);

    TestTransaction t2(0);
    aTART.insert(absentkey2, 456);

    assert(t1.try_commit());
    assert(t2.try_commit());

    {
        TransactionGuard t;
        // printf("to lookup\n");
        volatile auto x = aTART.lookup(absentkey2);
        // printf("looked\n");
        assert(x == 456);
    }
    printf("PASS: %s\n", __FUNCTION__);

}

void testReadDelete() {
    wrapper_type aTART;
    TestTransaction t0(0);
    aTART.insert(absentkey1, 10);
    aTART.insert(absentkey2, 10);
    assert(t0.try_commit());

    TestTransaction t1(0);
    aTART.lookup(absentkey1);
    aTART.insert(absentkey2, 10);

    TestTransaction t2(0);
    aTART.erase(absentkey1);

    assert(t2.try_commit());
    assert(!t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = aTART.lookup(absentkey1);
        volatile auto y = aTART.lookup(absentkey2);
        assert(x == 0);
        assert(y == 10);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testReadWriteDelete() {
    wrapper_type aTART;
    TestTransaction t0(0);
    aTART.insert(absentkey1, 10);
    aTART.insert(absentkey2, 10);
    assert(t0.try_commit());

    TestTransaction t1(0);
    aTART.lookup(absentkey1);
    aTART.insert(absentkey2, 123);

    TestTransaction t2(0);
    aTART.erase(absentkey1);

    assert(t2.try_commit());
    assert(!t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = aTART.lookup(absentkey1);
        volatile auto y = aTART.lookup(absentkey2);
        assert(x == 0);
        assert(y == 10);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testReadDeleteInsert() {
    wrapper_type aTART;
    TestTransaction t0(0);
    aTART.insert(absentkey1, 10);
    aTART.insert(absentkey2, 10);
    assert(t0.try_commit());

    TestTransaction t1(0);
    aTART.lookup(absentkey1);
    aTART.insert(absentkey2, 123);

    TestTransaction t2(0);
    aTART.erase(absentkey1);
    assert(t2.try_commit());

    TestTransaction t3(0);
    aTART.insert(absentkey1, 10);
    assert(t3.try_commit());
    assert(!t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = aTART.lookup(absentkey1);
        volatile auto y = aTART.lookup(absentkey2);
        assert(x == 10);
        assert(y == 10);
    }

    printf("PASS: %s\n", __FUNCTION__);
}


void testInsertDelete() {
    wrapper_type aTART;
    TestTransaction t0(0);
    aTART.insert(absentkey1, 10);
    aTART.insert(absentkey2, 10);
    assert(t0.try_commit());

    TestTransaction t1(0);
    aTART.insert(absentkey1, 123);
    aTART.insert(absentkey2, 456);

    TestTransaction t2(0);
    aTART.erase(absentkey1);
    assert(t2.try_commit());

    assert(!t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = aTART.lookup(absentkey1);
        volatile auto y = aTART.lookup(absentkey2);
        assert(x == 0);
        assert(y == 10);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testEmptyErase() {
    wrapper_type a;

    const char* key1 = "hello world";

    // deleting non-existent node
    {
        TransactionGuard t;
        a.erase(key1);
        volatile auto x = a.lookup(key1);
        a.insert(checkkey, 100);
        assert(x == 0);
    }

    {
        TransactionGuard t;
        a.erase(key1);
        volatile auto x = a.lookup(key1);
        assert(x == 0);
        a.insert(key1, 123);
        a.erase(key1);
        x = a.lookup(key1);
        assert(x == 0);    
    }

    printf("PASS: %s\n", __FUNCTION__);

}

int main(int argc, char *argv[]) {
    // testSimple();
    // testSimpleErase();
    testAbsentErase();
    // multiWrite();

    // keyval_db* db;

    // bool use_art = 1;
    // if (argc > 1) {
    //     use_art = atoi(argv[1]);
    // }
    //
    // uint64_t key1 = 1;
    // uint64_t key2 = 2;
    // {
    //     TransactionGuard t;
    //     db->insert(key1, 123);
    //     db->insert(key2, 321);
    // }
    //
    // {
    //     TransactionGuard t;
    //     volatile auto x = db->lookup(key1);
    //     volatile auto y = db->lookup(key2);
    //     assert(x == 123);
    //     assert(y == 321);
    // }

    // if (use_art) {
    //     printf("ART\n");
    //     db = new tart_wrapper();
    // } else {
    //     printf("Masstree\n");
    //     db = new masstree_wrapper();
    // }

    // uint64_t checkkey = 256;
    //
    // {
    //     TransactionGuard t;
    //     db->insert(key1, 123);
    //     db->insert(key2, 321);
    // }
    //
    // {
    //     TransactionGuard t;
    //     volatile auto x = db->lookup(key1);
    //     volatile auto y = db->lookup(key2);
    //     db->insert(checkkey, 100);
    //     assert(x == 123);
    //     assert(y == 321);
    // }
    //
    // {
    //     TransactionGuard t;
    //     db->erase(key1);
    //     volatile auto x = db->lookup(key1);
    //     db->insert(checkkey, 100);
    //     printf("%d\n", x);
    //     assert(x == 0);
    // }
    //
    // {
    //     TransactionGuard t;
    //     volatile auto x = db->lookup(key1);
    //     assert(x == 0);
    //     db->insert(key1, 567);
    // }
    //
    // {
    //     TransactionGuard t;
    //     volatile auto x = db->lookup(key1);
    //     db->insert(checkkey, 100);
    //     assert(x == 567);
    // }

    printf("Tests pass\n");
}
