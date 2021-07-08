#undef NDEBUG
#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include "Sto.hh"
#include "TMvArray.hh"
#include "TMvBox.hh"
#include <time.h>

inline double gettime_d() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec + ts.tv_nsec / 1.0e9;
}

template <typename T, unsigned N>
using TestArray = TMvArray<T, N>;

void testSimpleInt() {
    TestArray<int, 100> f;

    {
        TransactionGuard t;
        f[1] = 100;
    }

    

    {
        TransactionGuard t2;
        int f_read = f[1];
        assert(f_read == 100);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testSimpleString() {
    TestArray<std::string, 100> f;

    {
        TransactionGuard t;
        f[1] = "100";
    }

    

    {
        TransactionGuard t2;
        std::string f_read = f[1];
        assert(f_read.compare("100") == 0);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testIter() {
    std::vector<int> arr;
    TestArray<int, 10> f;
    for (int i = 0; i < 10; i++) {
        int x = rand();
        arr.push_back(x);
        f.nontrans_put(i, x);
    }
    

    int max;
    TRANSACTION_E {
        max = *(std::max_element(f.begin(), f.end()));
    } RETRY_E(false);

    assert(max == *(std::max_element(arr.begin(), arr.end())));
    printf("Max is %i\n", max);
    printf("PASS: %s\n", __FUNCTION__);
}

void testConflictingIter() {
    TMvArray<int, 10> f;
    TMvBox<int> box;
    for (int i = 0; i < 10; i++) {
        f.nontrans_put(i, i);
    }

    

    {
        TestTransaction t(1);
        std::max_element(f.begin(), f.end());

        TestTransaction t1(2);
        f[4] = 10;
        assert(t1.try_commit());
        assert(t.try_commit());
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testModifyingIter() {
    TestArray<int, 10> f;
    for (int i = 0; i < 10; i++)
        f.nontrans_put(i, i);

    

    {
        TransactionGuard t;
        Sto::mvcc_rw_upgrade();
        std::replace(f.begin(), f.end(), 4, 6);
    }

    

    {
        TransactionGuard t1;
        Sto::mvcc_rw_upgrade();
        int v = f[4];
        assert(v == 6);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testConflictingModifyIter1() {
    TMvArray<int, 10> f;
    for (int i = 0; i < 10; i++)
        f.nontrans_put(i, i);

    

    {
        TestTransaction t(1);
        std::replace(f.begin(), f.end(), 4, 6);

        TestTransaction t1(2);
        Sto::mvcc_rw_upgrade();
        f[4] = 10;

        assert(t1.try_commit());
        assert(!t.try_commit());
    }

    

    {
        TransactionGuard t2;
        Sto::mvcc_rw_upgrade();
        int v = f[4];
        assert(v == 10);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testConflictingModifyIter2() {
    TestArray<int, 10> f;
    for (int i = 0; i < 10; i++)
        f.nontrans_put(i, i);

    

    {
        TransactionGuard t;
        std::replace(f.begin(), f.end(), 4, 6);
    }

    

    {
        TransactionGuard t0;
        f[4] = 10;
    }

    

    {
        TransactionGuard t2;
        Sto::mvcc_rw_upgrade();
        int v = f[4];
        assert(v == 10);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testConflictingModifyIter3() {
    TMvArray<int, 10> f;
    TMvBox<int> box;
    for (int i = 0; i < 10; i++)
        f.nontrans_put(i, i);

    

    {
        TestTransaction t1(1);
        int x = f[4];
        assert(x == 4);

        TestTransaction t(2);
        std::replace(f.begin(), f.end(), 4, 6);

        assert(t.try_commit());
        assert(t1.try_commit());
    }

    

    {
        TransactionGuard t2;
        Sto::mvcc_rw_upgrade();
        int v = f[4];
        assert(v == 6);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testOpacity1() {
    TMvArray<int, 10> f;
    for (int i = 0; i < 10; i++)
        f.nontrans_put(i, i);

    

    try {
        TestTransaction t1(1);
        int x = f[3];
        assert(x == 3);

        TestTransaction t(2);
        f[3] = 2;
        std::replace(f.begin(), f.end(), 4, 6);
        assert(t.try_commit());

        t1.use();
        x = f[4];
        assert(x == 4);
        assert(t1.try_commit());
    } catch (Transaction::Abort e) {
        TestTransaction::hard_reset();
    }

    

    {
        TransactionGuard t2;
        Sto::mvcc_rw_upgrade();
        int v = f[4];
        assert(v == 6);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testMvInline() {
    TMvArray<int, 10> f;
    for (int i = 0; i < 10; i++) {
        f.nontrans_put(i, i);
    }

    const int *v0 = &f.nontrans_access(4);
    assert(*v0 == 4);

    {
        TestTransaction t1(1);
        f[4] = 14;
        assert(t1.try_commit());
    }

    const int *v1 = &f.nontrans_access(4);
    assert(*v1 == 14);
    assert(v0 != v1);

    Transaction::epoch_advance_once();

    {
        TestTransaction t2(2);
        f[4] = 24;
        assert(t2.try_commit());
    }

    const int *v2 = &f.nontrans_access(4);
    assert(*v2 == 24);
    assert(v1 != v2);
    assert(v0 == v2);

    printf("PASS: %s\n", __FUNCTION__);
}

void benchArray64() {
    TMvArray<int, 64> a;
    for (int i = 0; i < 64; ++i)
        a.nontrans_put(i, 0);

    const unsigned long niters = 1000;
    double before = gettime_d();
    for (unsigned long iter = 0; iter < niters; ++iter) {
        for (int j = 0; j < 1000; ++j) {
            

            TRANSACTION_E {
                Sto::mvcc_rw_upgrade();
                for (int i = 0; i < 64; ++i)
                    a[i] = a[i] + i;
            } RETRY_E(true);
        }
    }
    double after = gettime_d();

    printf("NS PER ITER (iter = 1000tx): %g\n", (after - before) * 1.0e9 / niters);
}


int main() {
    testSimpleInt();
    testSimpleString();
    testIter();
    testConflictingIter();
    testModifyingIter();
    testConflictingModifyIter1();
    testConflictingModifyIter2();
    testConflictingModifyIter3();
    testOpacity1();
#if MVCC_INLINING
    testMvInline();
#endif
    benchArray64();

    std::thread advancer;  // empty thread because we have no advancer thread
    Transaction::rcu_release_all(advancer, 3);
    return 0;
}
