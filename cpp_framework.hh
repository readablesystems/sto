#ifndef __CMDR_CPP_FRAMEWORK__
#define __CMDR_CPP_FRAMEWORK__

//------------------------------------------------------------------------------
// START
// File    : cpp_framework.h
// Author  : Ms.Moran Tzafrir; email: morantza@gmail.com; phone: +972-505-779961
// Written : 13 April 2009
// 
// Multi-Platform C++ framework
//
// Copyright (C) 2009 Moran Tzafrir.
// You can use this file only by explicit written approval from Moran Tzafrir.
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#ifndef __INTEL64_DEFNS_H__
#define __INTEL64_DEFNS_H__

#ifndef INTEL64
#define INTEL64
#endif

////////////////////////////////////////////////////////////////////////////////
//include directives 
////////////////////////////////////////////////////////////////////////////////
#include <limits.h>
#include <sys/types.h>
#include <malloc.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <alloca.h>
#include <strings.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////////
//types 
//////////////////////////////////////////////////////////////////////////
typedef unsigned char      _u8;
typedef unsigned short     _u16;
typedef unsigned int       _u32;
typedef unsigned long long _u64;
typedef unsigned long long tick_t;
typedef _u64               ptr_t;

////////////////////////////////////////////////////////////////////////////////
//constants
////////////////////////////////////////////////////////////////////////////////
#define inline_ __inline__
#define null (0)
#define CACHE_LINE_SIZE 64
#define lock_fc(x,b) ( 0 == x.getNotSafe() && 0 == x.getNotSafe() && 0 == x.getNotSafe() && (b=x.compareAndSet(0, 0xFF)) )

#define get_marked_ref(_p)      ((void *)(((ptr_t)(_p)) | 1UL))
#define get_unmarked_ref(_p)    ((void *)(((ptr_t)(_p)) & ~1UL))
#define is_marked_ref(_p)       (((ptr_t)(_p)) & 1UL)

#define ALIGNED_MALLOC(_s,_a)       memalign(_a, _s)
#define ALIGNED_FREE(_p)            free(_p)

//////////////////////////////////////////////////////////////////////////
//compare and set 
//////////////////////////////////////////////////////////////////////////
#define CAS32(_a,_o,_n)     __sync_val_compare_and_swap ((_u32*)_a,(_u32)_o,(_u32)_n)
#define CAS64(_a,_o,_n)     __sync_val_compare_and_swap ((_u64*)_a,(_u64)_o,(_u64)_n)
#define CASPO(_a,_o,_n)     __sync_val_compare_and_swap ((void**)_a,(void*)_o,(void*)_n)

#define SWAP32(_a,_n)       __sync_lock_test_and_set(_a, _n)
#define SWAP64(_a,_n)       __sync_lock_test_and_set(_a, _n)
#define SWAPPO(_a,_n)       __sync_lock_test_and_set(_a, _n)


////////////////////////////////////////////////////////////////////////////////
//memory management
//------------------------------------------------------------------------------
//  WMB(): All preceding write operations must commit before any later writes.
//  RMB(): All preceding read operations must commit before any later reads.
//  MB():  All preceding memory accesses must commit before any later accesses.
////////////////////////////////////////////////////////////////////////////////

#define RMB() _GLIBCXX_READ_MEM_BARRIER
#define WMB() _GLIBCXX_WRITE_MEM_BARRIER
#define MB()  __sync_synchronize()

inline unsigned MUTEX_ENTER(unsigned volatile* x) {
    return __sync_lock_test_and_set(x, 0xFF);
}

inline void MUTEX_EXIT(unsigned volatile* x) {
    return __sync_lock_release (x);
}

//////////////////////////////////////////////////////////////////////////
//CPU counters
//////////////////////////////////////////////////////////////////////////
#define RDTICK() \
    ({ tick_t __t; __asm__ __volatile__ ("rdtsc" : "=A" (__t)); __t; })

//////////////////////////////////////////////////////////////////////////
//bit operations
//////////////////////////////////////////////////////////////////////////
#define bit_count(x) __builtin_popcount (x)
#define bit_count64(x) __builtin_popcountll (x)

inline_ int first_lsb_bit_indx(_u32 x) {
    if(0==x) 
        return -1;
    return __builtin_ffs(x)-1;
}

inline_ int first_lsb_bit_indx64(_u64 x) {
    if(0==x) 
        return -1;
    return __builtin_ffsll(x)-1;
}

inline_ int first_msb_bit_indx(_u32 x) {
    if(0==x) 
        return -1;
    return  __builtin_clz(x)-1;
}

inline_ int first_msb_bit_indx64(_u64 x) {
    if(0==x) 
        return -1;
    return  __builtin_clzll(x)-1;
}

#endif /* __INTEL_DEFNS_H__ */

namespace CCP {
    class Memory {
    public:
        static const int CACHELINE_SIZE = CACHE_LINE_SIZE;

        inline_ static void* byte_malloc(const size_t size) {return malloc(size);}
        inline_ static void  byte_free(void* mem)                   {free(mem);}

        inline_ static void* byte_aligned_malloc(const size_t size) {return ALIGNED_MALLOC(size, CACHELINE_SIZE);}
        inline_ static void* byte_aligned_malloc(const size_t size, const size_t alignment) {return ALIGNED_MALLOC(size,alignment);}
        inline_ static void  byte_aligned_free(void* mem)   {ALIGNED_FREE(mem);}

        inline_ static void  read_write_barrier()   { MB(); }
        inline_ static void  write_barrier()        { WMB(); }
        inline_ static void  read_barrier()         { RMB(); }

        inline_ static _u32  compare_and_set(_u32 volatile* _a, _u32 _o, _u32 _n)       {return CAS32(_a,_o,_n);}
        inline_ static void* compare_and_set(void* volatile * _a, void* _o, void* _n)   {return (void*)CASPO(_a,_o,_n);}
        inline_ static _u64  compare_and_set(_u64 volatile* _a, _u64 _o, _u64 _n)       {return CAS64(_a,_o,_n);}

        inline_ static _u32  exchange_and_set(_u32 volatile *   _a, _u32  _n)   {return SWAP32(_a,_n);}
        inline_ static void* exchange_and_set(void * volatile * _a, void* _n)   {return SWAPPO(_a,_n);}
    };

    /////////////////////////////////////////////////////////////////////////////
    //atomic 
    /////////////////////////////////////////////////////////////////////////////
    template<typename V>
    class VolatileType {
        V volatile _value;
    public:
        inline_ explicit VolatileType() { }
        inline_ explicit VolatileType(const V& x) {
            _value = x;
            Memory::write_barrier();
        }
        inline_ V get() const {
            Memory::read_barrier();
            return _value;
        }

        inline_ V getNotSafe() const {
            return _value;
        }

        inline_ void set(const V& x) {
            _value = x;
            Memory::write_barrier();
        }
        inline_ void setNotSafe(const V& x) {
            _value = x;
        }

        //--------------
        inline_ operator V() const {  //convention
            return get();
        }
        inline_ V operator->() const {
            return get();
        }
        inline_ V volatile * operator&() {
            Memory::read_barrier();
            return &_value;
        }

        //--------------
        inline_ VolatileType<V>& operator=(const V x) { 
            _value = x;
            Memory::write_barrier();
            return *this;
        }
        //--------------
        inline_ bool operator== (const V& right) const {
            Memory::read_barrier();
            return _value == right;
        }
        inline_ bool operator!= (const V& right) const {
            Memory::read_barrier();
            return _value != right;
        }
        inline_ bool operator== (const VolatileType<V>& right) const {
            Memory::read_barrier();
            return _value == right._value;
        }
        inline_ bool operator!= (const VolatileType<V>& right) const {
            Memory::read_barrier();
            return _value != right._value;
        }

        //--------------
        inline_ VolatileType<V>& operator++ () { //prefix ++
            ++_value;
            Memory::write_barrier();
            return *this;
        }
        inline_ V operator++ (int) { //postfix ++
            const V tmp(_value);
            ++_value;
            Memory::write_barrier();
            return tmp;
        }
        inline_ VolatileType<V>& operator-- () {// prefix --
            --_value;
            Memory::write_barrier();
            return *this;
        }
        inline_ V operator-- (int) {// postfix --
            const V tmp(_value);
            --_value;
            Memory::write_barrier();
            return tmp;
        }

        //--------------
        inline_ VolatileType<V>& operator+=(const V& x) { 
            _value += x;
            Memory::write_barrier();
            return *this;
        }
        inline_ VolatileType<V>& operator-=(const V& x) { 
            _value -= x;
            Memory::write_barrier();
            return *this;
        }
        inline_ VolatileType<V>& operator*=(const V& x) { 
            _value *= x;
            Memory::write_barrier();
            return *this;
        }
        inline_ VolatileType<V>& operator/=(const V& x) { 
            _value /= x;
            Memory::write_barrier();
            return *this;
        }
        //--------------
        inline_ V operator+(const V& x) { 
            Memory::read_barrier();
            return _value + x;
        }
        inline_ V operator-(const V& x) { 
            Memory::read_barrier();
            return _value - x;
        }
        inline_ V operator*(const V& x) { 
            Memory::read_barrier();
            return _value * x;
        }
        inline_ V operator/(const V& x) { 
            Memory::read_barrier();
            return _value / x;
        }
    };

    class AtomicInteger {
    private:
        VolatileType<_u32> _value;
    public:
        explicit AtomicInteger() {
            _value=0;
        }
        explicit AtomicInteger(const int value) {
            _value = value;
        }

        inline_ void set(const int newValue) {
            _value = newValue;
        }

        inline_ void setNotSafe(const int newValue) {
            _value.setNotSafe(newValue);
        }

        inline_ bool compareAndSet(const int expect, const int update) {
            return (expect == int(CAS32(&_value, expect, update)));
        }  
        inline_ int getAndIncrement() {
            do {
                const int value = _value;
                if (value == int(CAS32(&_value, value, value + 1)))  {
                    return value;       
                }
            } while(true);
        }
        inline_ int getAndDecrement() {
            do {
                const int value = _value;
                if (value == int(CAS32(&_value, value, value - 1)))  {
                    return value;       
                }
            } while(true);
        }
        inline_ int incrementAndGet() {
            do {
                const int value = _value;
                if (value == int(CAS32(&_value, value, value + 1)))  {
                    return value+1;
                }
            } while(true);
        }
        inline_ int decrementAndGet() {
            do {
                const int value = _value;
                if (value == int(CAS32(&_value, value, value - 1)))  {
                    return value-1;     
                }
            } while(true);
        }
        inline_ int getAndSet(const _u32 newValue) {
            return Memory::exchange_and_set((_u32 volatile*)&_value, newValue);
        }
        inline_ int getAndAdd(const int delta) {
            do {
                const int value = _value;
                if (value == int(CAS32(&_value, value, value+delta))) {
                    return value;       
                }
            } while(true);
        } 
        inline_ int addAndGet(const int delta) {
            do {
                const int value = _value;
                if (value == int(CAS32(&_value, value, value+delta))) {
                    return value+delta;
                }
            } while(true);
        }

        inline_ int intValue() {
            return _value;
        }
        inline_ _u64 longValue()  {
            return (_u64)_value;
        }
        inline_ int get() {
            return _value;
        } 
        inline_ int getNotSafe() {
            return _value.getNotSafe();
        } 
    };

    template<typename V>
    class AtomicReference {
    private:
        VolatileType<V*> _value;
    public:
        explicit AtomicReference() {_value=null;}
        explicit AtomicReference(V* value) {_value.set(value);}

        inline_ operator V*() const {  //convention
            return get();
        }

        inline_ void set(V* newValue) {
            _value.set(newValue);
        }

        inline_ V* getAndSet(const V* newValue) {
            return (V*) Memory::exchange_and_set((void * volatile *)&_value, (void*)newValue);
        }

        inline_ bool compareAndSet(const V* expect, const V* update) {
            return ((void*)expect == (CASPO((volatile void *)&_value, (void *)expect, (void *)update)));
        }  

        inline_ V* get() const {
            return _value.get();
        }
        inline_ V* getReference() const {
            return _value.get();
        }
        inline_ V* getRefNotSafe() const {
            return _value.getNotSafe();
        }
    };

};

#endif

//------------------------------------------------------------------------------
// END
// File    : cpp_framework.h
// Author  : Ms.Moran Tzafrir; email: morantza@gmail.com; phone: +972-505-779961
// Written : 13 April 2009
// 
// Multi-Platform C++ framework
//
// Copyright (C) 2009 Moran Tzafrir.
// You can use this file only by explicit written approval from Moran Tzafrir.
//------------------------------------------------------------------------------

