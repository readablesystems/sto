#ifndef __SPARC_DEFNS_H__
#define __SPARC_DEFNS_H__

#ifndef SPARC
#define SPARC
#endif

//////////////////////////////////////////////////////////////////////////
//include directives 
//////////////////////////////////////////////////////////////////////////
#include <thread.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <alloca.h>
#include <strings.h>
#include <atomic.h>

//////////////////////////////////////////////////////////////////////////
//types 
//////////////////////////////////////////////////////////////////////////
typedef unsigned char      _u8;
typedef unsigned short     _u16;
typedef unsigned int       _u32;
typedef unsigned long long _u64;
typedef unsigned long long tick_t;
typedef _u32               ptr_t;

//////////////////////////////////////////////////////////////////////////
//constants
//////////////////////////////////////////////////////////////////////////
#define inline_						inline
#define CACHE_LINE_SIZE				(64)
#define PTHREAD_STACK_MIN			((size_t)_sysconf(_SC_THREAD_STACK_MIN))

#define get_marked_ref(_p)			((void *)(((ptr_t)(_p)) | 1U))
#define get_unmarked_ref(_p)		((void *)(((ptr_t)(_p)) & ~1U))
#define is_marked_ref(_p)			(((ptr_t)(_p)) & 1U)

#define ALIGNED_MALLOC(_s,_a)		memalign(_a, _s)
#define ALIGNED_FREE(_p)			free(_p)

//////////////////////////////////////////////////////////////////////////
//compare and set
//////////////////////////////////////////////////////////////////////////
extern "C" int CASIO_internal(_u32 volatile*, _u32, _u32);
extern "C" void * CASPO_internal(void volatile*, void *, void *);
extern "C" _u64 CAS64O_internal(_u64 volatile*, _u64, _u64);

#define CAS32(_a,_o,_n) (CASIO_internal((_u32 volatile*)(_a),(_u32)(_o),(_u32)(_n)))
//#define CAS64(_a,_o,_n) (CAS64O_internal((_u64 volatile*)_a,(_u64)_o,(_u64)_n))
#define CASPO(_a,_o,_n) ((void*)(CASIO_internal((_u32 volatile*)(_a),(_u32)(_o),(_u32)(_n))))

//#define CAS32(_a,_o,_n)		atomic_cas_32(_a, _o, _n)
#define CAS64(_a,_o,_n)		atomic_cas_64(_a, _o, _n)
//#define CASPO(_a,_o,_n)		atomic_cas_ptr(_a, _o, _n)

/*
#define SWAP32(_a,_n)		atomic_swap_32(_a, _n)
#define SWAP64(_a,_n)		atomic_swap_64(_a, _n)
#define SWAPPO(_a,_n)		atomic_swap_ptr(_a, _n)
*/

inline _u32 SWAP32(_u32 volatile* a, _u32 n) {
	_u32 no, o = *a;
	while ( (no = CAS32(a, o, n)) != o ) o = no;
	return o;
}

inline void* SWAPPO(void* volatile* a, void* n) {
	void *no, *o = *(void **)a;
	while ( (no = CASPO(a, o, n)) != o ) o = no;
	return o;
}

////////////////////////////////////////////////////////////////////////////////
//memory management
//------------------------------------------------------------------------------
//	WMB(): All preceding write operations must commit before any later writes.
//	RMB(): All preceding read operations must commit before any later reads.
//	MB():  All preceding memory accesses must commit before any later accesses.
////////////////////////////////////////////////////////////////////////////////

//#define RMB()	
//#define WMB()	membar_producer()
//#define MB()	membar_producer()

extern "C" void MEMBAR_ALL(void);
extern "C" void MEMBAR_STORESTORE(void);
extern "C" void MEMBAR_LOADLOAD(void);

#define MB()  MEMBAR_ALL()
#define WMB() MEMBAR_STORESTORE()
#define RMB() MEMBAR_LOADLOAD()

inline unsigned MUTEX_ENTER(unsigned volatile* x) {
	if(0==SWAP32(x, 0xFF)) {
		MB();
		return 0;
	} else
		return 0xFF;
}

inline void MUTEX_EXIT(unsigned volatile* x) {
	MB();
	*x = 0;
	MB();
}

//////////////////////////////////////////////////////////////////////////
//CPU counters
//////////////////////////////////////////////////////////////////////////
extern "C" tick_t RDTICK(void);

//////////////////////////////////////////////////////////////////////////
//bit operations
//////////////////////////////////////////////////////////////////////////

extern "C" _u32 POPC(_u32 x);

inline_ _u32 bit_count(const _u32 x) {
	return POPC(x);
}

inline_ _u32 bit_count64(const _u64 x) {
	return bit_count((_u32)x) + bit_count((_u32)(x>>32));
}

inline_ int fls(_u32 x) {
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return POPC(x);
}

inline_ int first_lsb_bit_indx(_u32 x) {
	if(0==x) 
		return -1;
   return ffs(x)-1;
}

inline_ int first_msb_bit_indx(_u32 x) {
	if(0==x) 
		return -1;
	return fls(x)-1;
}

inline_ int first_lsb_bit_indx64(_u64 x) {
	if(0==x)
		return -1;
	else if(0 == (x & 0xFFFFFFFFULL)) {
		x >>= 32;
		register unsigned long i;
		i=ffs((_u32)x);
		return (int)i+31;
	} else {
		register unsigned long i;
		i=ffs((_u32)x);
		return (int)i-1;
	}
}

inline_ int first_msb_bit_indx64(_u64 x) {
	if(0==x)
		return -1;
	else if(0 == (x & 0xFFFFFFFFULL)) {
		x >>= 32;
		register unsigned long i;
		i=fls((_u32)x);
		return (int)i+31;
	} else {
		register unsigned long i;
		i=fls((_u32)x);
		return (int)i-1;
	}

}

#endif /* __SPARC_DEFNS_H__ */
