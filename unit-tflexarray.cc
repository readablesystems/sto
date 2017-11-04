#include "TFlexArray.hh"

static constexpr unsigned default_size = 10000000;

TOCCArray<uint64_t , default_size> occ_array;
TLockArray<uint64_t , default_size> lock_array;
TSwissArray<uint64_t , default_size> swiss_array;
TicTocArray<uint64_t, default_size> tictoc_array;

int main() {
    {
        TransactionGuard guard;
        for (unsigned int i = 0; i < default_size; ++i) {
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
        for (unsigned int i = 0; i < default_size; ++i) {
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

    return 0;
}