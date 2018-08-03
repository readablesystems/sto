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

    void insert(const char* key, uintptr_t val) {
        insert({key, sizeof(key)}, val);
    }

    void insert(lcdf::Str key, uintptr_t val) override {
        bool success;
        std::tie(success, std::ignore) = oi.insert_row(key, new oi_value{val});
        if (!success) throw Transaction::Abort();
    }

    uintptr_t lookup(const char* key) {
        return lookup({key, sizeof(key)});
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

    void update(const char* key, uintptr_t val) {
        update({key, sizeof(key)}, val);
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

    void erase(const char* key) {
        erase({key, sizeof(key)});
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

void testUpgradeNode() {
    wrapper_type a;
    {
        TransactionGuard t;
        a.insert("1", 1);
        a.insert("10", 1);
        a.insert("11", 1);
        a.insert("12", 1);
        a.insert("15", 1);
    }

    TestTransaction t0(0);
    a.lookup("13");
    a.insert("14", 1);

    TestTransaction t1(1);
    a.insert("13", 1);

    assert(t1.try_commit());
    assert(!t0.try_commit());

    printf("PASS: %s\n", __FUNCTION__); 
}

void testUpgradeNode2() {
    TART a;
    {
        TransactionGuard t;
        a.transPut("1", 1);
        a.transPut("10", 1);
        // a.transPut("11", 1);
    }

    TestTransaction t0(0);
    a.transGet("13");
    a.transPut("14", 1);
    a.transPut("15",1);
    a.transPut("16", 1);
    assert(t0.try_commit());

    TestTransaction t1(1);
    a.transGet("13");
    a.transPut("14", 1);
    a.transPut("15",1);
    a.transPut("16", 1);

    assert(t1.try_commit());
    printf("PASS: %s\n", __FUNCTION__); 
}


void testUpgradeNode3() {
    wrapper_type a;
    {
        TransactionGuard t;
        a.insert("10", 1);
        a.insert("11", 1);
    }

    TestTransaction t0(0);
    a.lookup("13");

    TestTransaction t1(1);
    a.insert("13", 1);

    t0.use();
    a.insert("14", 1);

    assert(t1.try_commit());
    assert(!t0.try_commit());

    printf("PASS: %s\n", __FUNCTION__); 
}

void testUpgradeNode4() {
    wrapper_type a;
    {
        TransactionGuard t;
        a.insert("10", 1);
        a.insert("11", 1);
        a.insert("12", 1);
    }

    TestTransaction t0(0);
    a.lookup("14");

    TestTransaction t1(1);
    a.insert("13", 1);
    a.erase("10");
    a.erase("11");
    a.erase("11");
    a.erase("13");
    a.insert("14", 1);
    a.insert("15", 1);
    a.insert("16", 1);
    a.insert("17", 1);

    assert(t1.try_commit());

    t0.use();
    a.erase("14");
    a.erase("15");
    a.erase("16");
    a.erase("17");

    assert(!t0.try_commit());

    printf("PASS: %s\n", __FUNCTION__); 
}

void testDowngradeNode() {
    wrapper_type a;

    {
        TransactionGuard t;
        a.insert("1", 1);
        a.insert("10", 1);
        a.insert("11", 1);
        a.insert("12", 1);
        a.insert("13", 1);
        a.insert("14", 1);
    }

    TestTransaction t0(0);
    a.lookup("15");
    a.insert("random", 1);

    TestTransaction t1(1);
    a.lookup("10");
    a.insert("hummus", 1);

    TestTransaction t2(2);
    a.erase("15");
    a.insert("linux", 1);

    TestTransaction t3(1);
    a.erase("14");
    a.erase("13");
    a.erase("12");
    a.erase("11");
    assert(t3.try_commit());

    assert(!t0.try_commit());
    assert(t1.try_commit());
    assert(!t2.try_commit());
    printf("PASS: %s\n", __FUNCTION__);
}

void testSplitNode() {
    TART a;
    {
        TransactionGuard t;
        a.transPut("ab", 0);
        a.transPut("1", 0);
    }

    TestTransaction t0(0);
    a.transGet("ad");
    a.transPut("12", 1);

    TestTransaction t1(1);
    a.transGet("abc");
    a.transPut("13", 1);

    TestTransaction t2(2);
    a.transPut("ad", 1);
    a.transPut("abc", 1);

    assert(t2.try_commit());
    assert(!t0.try_commit());
    assert(!t1.try_commit());

    printf("PASS: %s\n", __FUNCTION__); 
}

void testSplitNode2() {
    TART a;
    {
        TransactionGuard t;
        a.transPut("aaa", 0);
        a.transPut("aab", 0);
    }

    TestTransaction t0(0);
    a.transGet("ab");
    a.transPut("1", 0);

    TestTransaction t1(1);
    a.transPut("ab", 0);

    assert(t1.try_commit());
    assert(!t0.try_commit());

    printf("PASS: %s\n", __FUNCTION__); 
}

void testEmptySplit() {
    TART a;
    {
        TransactionGuard t;
        a.transPut("aaa", 1);
        a.transPut("aab", 1);
    }

    TestTransaction t0(0);
    a.transGet("aac");
    a.transPut("1", 0);

    TestTransaction t1(1);
    a.transRemove("aaa");
    a.transRemove("aab");
    assert(t1.try_commit());

    TestTransaction t2(2);
    a.transPut("aac", 0);
    assert(t2.try_commit());

    assert(!t0.try_commit());

    printf("PASS: %s\n", __FUNCTION__); 
}

int main(int argc, char *argv[]) {
    testSimple();
    testSimpleErase();
    testAbsentErase();
    testEmptyErase();
    multiWrite();
    testUpgradeNode();
    testUpgradeNode2();
    testUpgradeNode3();
    testUpgradeNode4();
    testDowngradeNode();
    testSplitNode();
    testSplitNode2();
    testEmptySplit();

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
