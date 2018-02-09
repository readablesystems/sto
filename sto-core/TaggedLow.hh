#pragma once
#include "compiler.hh"

// Tagged pointer which uses lower 3 bits of pointer for tag bits
// (so, requires pointer is one returned by malloc, i.e. 8-byte aligned)
// does not require extra space on 32-bit architectures
template <typename T>
class TaggedLow {
    typedef T* packed_type;
    typedef uintptr_t flags_type;

    static packed_type pack(T* ptr, uint8_t flags) {
        assert(!(reinterpret_cast<flags_type>(ptr) & 7) && !(flags & ~7));
        return reinterpret_cast<packed_type>(reinterpret_cast<flags_type>(ptr) | flags);
    }

  public:
    TaggedLow(T* ptr, uint8_t flags)
        : p_(pack(ptr, flags)) {
    }

    operator T*() {
        return (T*) ptr();
    }

    operator const T*() const {
        return ptr();
    }

    const T* ptr() const {
        return reinterpret_cast<T*>(reinterpret_cast<flags_type>(p_) & ~flags_type(7));
    }

    uint8_t flags() const {
        return reinterpret_cast<flags_type>(p_) & 7;
    }

    void assign_ptr(T* ptr) {
        fence();
        p_ = reinterpret_cast<packed_type>(reinterpret_cast<flags_type>(ptr) | flags());
    }

    void assign_flags(uint8_t flags) {
        assert((flags & ~7) == 0);
        fence();
        p_ = reinterpret_cast<packed_type>((reinterpret_cast<flags_type>(p_) & ~flags_type(7)) | flags);
    }

    void or_flags(uint8_t flags) {
        assert((flags & ~7) == 0);
        fence();
        p_ = reinterpret_cast<packed_type>(reinterpret_cast<flags_type>(p_) | flags);
    }

    void atomic_add_flags(uint8_t flags) {
        assert((flags & ~7) == 0);
        while (1) {
            auto cur = reinterpret_cast<flags_type>(p_);
            fence();
            if ((cur & flags) == 0 && bool_cmpxchg(&p_, reinterpret_cast<packed_type>(cur),
                                                   reinterpret_cast<packed_type>(cur | flags))) {
                break;
            }
            relax_fence();
        }
    }

  private:
    T* p_;
};
