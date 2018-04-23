#include "TFlexArray.hh"
#include <iostream>

void test_compile() {
    constexpr unsigned sz = 1000;
    TOCCArray<uint64_t , sz> occ_array;
    TAdaptiveArray<uint64_t , sz> lock_array;
    TSwissArray<uint64_t , sz> swiss_array;
    TicTocArray<uint64_t, sz> tictoc_array;

    {
        TransactionGuard guard;
        for (unsigned int i = 0; i < 10; ++i) {
            bool ok = occ_array.transPut(i, uint64_t(i));
            assert(ok);
            ok = lock_array.transPut(i, uint64_t(i));
            assert(ok);
            ok = swiss_array.transPut(i, uint64_t(i));
            assert(ok);
            ok = tictoc_array.transPut(i, uint64_t(i));
            assert(ok);
        }
    }

    {
        TransactionGuard guard;
        for (unsigned int i = 0; i < 10; ++i) {
            uint64_t val;
            bool ok = occ_array.transGet(i, val);
            assert(ok && val == uint64_t(i));
            ok = lock_array.transGet(i, val);
            assert(ok && val == uint64_t(i));
            ok = swiss_array.transGet(i, val);
            assert(ok && val == uint64_t(i));
            ok = tictoc_array.transGet(i, val);
            assert(ok && val == uint64_t(i));
        }
    }
    std::cout << "PASS: " << std::string(__FUNCTION__) << std::endl;
}

void test_tictoc0() {
    TOCCArray<int, 10> array;
    for (int i = 0; i < 10; ++i)
        array.nontrans_put(i, i);
    {
        TestTransaction t1(1);
        int a;
        bool ok = array.transGet(4, a);
        assert(ok && a == 4);

        TestTransaction t2(2);
        ok = array.transPut(4, 5);
        assert(ok && t2.try_commit());
        t1.use();
        ok = array.transGet(5, a);
        assert(ok && a == 5);
        assert(!t1.try_commit());
    }
    std::cout << "PASS: " << std::string(__FUNCTION__) << std::endl;
}

void test_tictoc1() {
    TicTocArray<int, 10> array;
    for (int i = 0; i < 10; ++i)
        array.nontrans_put(i, i);
    {
        TestTransaction t1(1);
        int a;
        bool ok = array.transGet(4, a);
        assert(ok && a == 4);

        TestTransaction t2(2);
        ok = array.transPut(4, 5);
        assert(ok && t2.try_commit());
        t1.use();
        ok = array.transGet(5, a);
        assert(ok && a == 5);
        assert(t1.try_commit());
    }
    std::cout << "PASS: " << std::string(__FUNCTION__) << std::endl;
}

void test_tictoc2() {
    TicTocArray<int, 10> array;
    for (int i = 0; i < 10; ++i)
        array.nontrans_put(i, i);
    {
        TestTransaction t1(1);
        int a;
        bool ok = array.transGet(4, a);
        assert(ok && a == 4);

        TestTransaction t2(2);
        int b;
        ok = array.transGet(4, b);
        assert(ok && b == 4);

        t1.use();
        array.transPut(4, a+1);
        assert(ok && t1.try_commit());

        t2.use();
        ok = array.transPut(4, b+1);
        assert(ok && !t2.try_commit());
    }
    std::cout << "PASS: " << std::string(__FUNCTION__) << std::endl;
}

int main() {
    test_compile();
    test_tictoc0();
    test_tictoc1();
    test_tictoc2();
    std::cout << "ALL TESTS PASS!" << std::endl;
    return 0;
}
