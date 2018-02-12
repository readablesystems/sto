#pragma once
#include <stdint.h>

// 64 bits including a pointer and 16 bits of flags
// (technically we have 19 flag bits on a 64 bit machine and 35 on a 32 bit machine but who cares)
template <typename T>
class Tagged64 {
    static constexpr int shift = 64 - 16;
#if SIZEOF_VOID_P == 8
    typedef T* packed_type;
    static constexpr uintptr_t ptrmask = 0xFFFFFFFFFFFFULL;
#elif SIZEOF_VOID_P == 4
    typedef uint64_t packed_type;
    static constexpr uintptr_t ptrmask = 0xFFFFFFFFU;
#else
# error "bad sizeof(void*)"
#endif
    typedef uint64_t flags_type;

    static packed_type pack(T* p, int flags) {
        return reinterpret_cast<packed_type>(reinterpret_cast<uintptr_t>(p)
                                             | (flags_type(flags) << shift));
    }

public:

    Tagged64(T* p)
        : p_(pack(p, 0)) {
    }

    T* ptr() const {
        return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(p_) & ptrmask);
    }

    uint16_t flags() const {
        return reinterpret_cast<flags_type>(p_) >> shift;
    }

    void assign_flags(uint16_t flags) {
        p_ = pack(ptr(), flags);
    }

    void or_flags(uint16_t flags) {
        p_ = reinterpret_cast<packed_type>(reinterpret_cast<flags_type>(p_)
                                           | (flags_type(flags) << shift));
    }

    void rm_flags(uint16_t flags) {
        p_ = reinterpret_cast<packed_type>(reinterpret_cast<flags_type>(p_)
                                           & ~(flags_type(flags) << shift));
    }

    bool operator<(Tagged64<T> x) const {
        return reinterpret_cast<flags_type>(p_) < reinterpret_cast<flags_type>(x.p_);
    }

  private:
    packed_type p_;
};
