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
#define CACHE_LINE_SIZE 64

#define get_marked_ref(_p)      ((void *)(((ptr_t)(_p)) | 1UL))
#define get_unmarked_ref(_p)    ((void *)(((ptr_t)(_p)) & ~1UL))
#define is_marked_ref(_p)       (((ptr_t)(_p)) & 1UL)

#define ALIGNED_MALLOC(_s,_a)		memalign(_a, _s)
#define ALIGNED_FREE(_p)			free(_p)

//////////////////////////////////////////////////////////////////////////
//compare and set 
//////////////////////////////////////////////////////////////////////////
#define CAS32(_a,_o,_n)		__sync_val_compare_and_swap ((_u32*)_a,(_u32)_o,(_u32)_n)
#define CAS64(_a,_o,_n)		__sync_val_compare_and_swap ((_u64*)_a,(_u64)_o,(_u64)_n)
#define CASPO(_a,_o,_n)		__sync_val_compare_and_swap ((void**)_a,(void*)_o,(void*)_n)

#define SWAP32(_a,_n)		__sync_lock_test_and_set(_a, _n)
#define SWAP64(_a,_n)		__sync_lock_test_and_set(_a, _n)
#define SWAPPO(_a,_n)		__sync_lock_test_and_set(_a, _n)


////////////////////////////////////////////////////////////////////////////////
//memory management
//------------------------------------------------------------------------------
//	WMB(): All preceding write operations must commit before any later writes.
//	RMB(): All preceding read operations must commit before any later reads.
//	MB():  All preceding memory accesses must commit before any later accesses.
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
