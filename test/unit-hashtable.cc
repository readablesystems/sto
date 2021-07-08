#undef NDEBUG
#include <string>
#include <iostream>
#include <cassert>
#include <vector>
#include "Sto.hh"
#include "Commutators.hh"
#include "Hashtable.hh"
//XXX disabled string wrapper due to unknown compiler issue
//#include "StringWrapper.hh"

#define GUARDED if (TransactionGuard tguard{})

// Test suite for basic nontransactional put and get functionality
void testBasicNontransPutGet() {
    typedef Hashtable<Hashtable_params<int, int>> ht_type;
    ht_type ht;

    // Test putting and getting the same element
    {
        ht.nontrans_put(1, 42);
        int* vp = ht.nontrans_get(1);
        assert(vp);
        assert(*vp == 42);
    }

    // Test getting an absent element
    {
        int* vp = ht.nontrans_get(42);
        assert(!vp);
    }


    // Test overwriting and existing element
    {
        ht.nontrans_put(1, 9001);
        int* vp = ht.nontrans_get(1);
        assert(vp);
        assert(*vp == 9001);
    }

    // Test putting and getting a new element
    {
        ht.nontrans_put(1776, 74);
        int* vp = ht.nontrans_get(1776);
        assert(vp);
        assert(*vp == 74);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

// Test suite for basic transactional put functionality
void testBasicTransPut() {
    typedef Hashtable<Hashtable_params<int, int>> ht_type;
    ht_type ht;

    // Test transactional put
    {
        TestTransaction t(1);

        int value = 42;
        auto result = ht.transPut(1, &value);
        assert(!result.abort);
        assert(!result.existed);

        assert(t.try_commit());

        int* vp = ht.nontrans_get(1);
        assert(vp);
        assert(*vp == 42);
    }

    // Blind writes should both succeed
    {
        TestTransaction t1(1);

        int value1 = 9001;
        auto result1 = ht.transPut(1, &value1, true);
        assert(!result1.abort);
        assert(result1.existed);

        TestTransaction t2(2);

        int value2 = 9002;
        auto result2 = ht.transPut(1, &value2, true);
        assert(!result2.abort);
        assert(result2.existed);

        assert(t2.try_commit());
        assert(t1.try_commit());

        int* vp = ht.nontrans_get(1);
        assert(vp);
        assert(*vp == 9001);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

// Test suite for basic transactional get functionality
void testBasicTransGet() {
    typedef Hashtable<Hashtable_params<int, int>> ht_type;
    ht_type ht;

    // Test transactional get
    {
        ht.nontrans_put(1, 42);
        TestTransaction t(1);

        auto result = ht.transGet(1, ht_type::ReadWriteAccess);
        assert(result.value);
        assert(*result.value == 42);

        assert(t.try_commit());
    }

    // Reads should not see pending writes
    {
        TestTransaction t1(1);

        int value1 = 9001;
        auto result1 = ht.transPut(3, &value1);
        assert(!result1.abort);
        assert(!result1.existed);

        TestTransaction t2(2);

        auto result2 = ht.transGet(3, ht_type::ReadWriteAccess);
        assert(!result2.value);

        assert(t1.try_commit());
        assert(t2.try_commit());
    }

    printf("PASS: %s\n", __FUNCTION__);
}

// Test suite for basic transactional update functionality
void testBasicTransUpdate() {
    typedef Hashtable<Hashtable_params<int, int>> ht_type;
    ht_type ht;

    // Test transactional update
    {
        ht.nontrans_put(1, 42);
        TestTransaction t(1);

        auto result = ht.transGet(1, ht_type::ReadWriteAccess);
        assert(!result.abort);
        assert(result.success);

        int value = 9001;
        ht.transUpdate(result.ref, &value);

        {
            int* vp = ht.nontrans_get(1);
            assert(vp);
            assert(*vp == 42);
        }

        assert(t.try_commit());

        {
            int* vp = ht.nontrans_get(1);
            assert(vp);
            assert(*vp == 9001);
        }
    }

    printf("PASS: %s\n", __FUNCTION__);
}

// Test suite for basic transactional delete functionality
void testBasicTransDelete() {
    typedef Hashtable<Hashtable_params<int, int>> ht_type;
    ht_type ht;

    // Test transactional delete
    {
        ht.nontrans_put(1, 42);
        TestTransaction t(1);

        {
            int* vp = ht.nontrans_get(1);
            assert(vp);
            assert(*vp == 42);
        }

        auto result = ht.transDelete(1);
        assert(!result.abort);
        assert(result.success);

        {
            int* vp = ht.nontrans_get(1);
            assert(vp);
            assert(*vp == 42);
        }

        assert(t.try_commit());

        {
            int* vp = ht.nontrans_get(1);
            assert(!vp);
        }
    }

    printf("PASS: %s\n", __FUNCTION__);
}


int main() {
    testBasicNontransPutGet();
    testBasicTransPut();
    testBasicTransGet();
    testBasicTransUpdate();
    testBasicTransDelete();
    printf("All tests pass!\n");

    std::thread advancer;  // empty thread because we have no advancer thread
    Transaction::rcu_release_all(advancer, 2);
    return 0;
}

