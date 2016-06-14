#ifndef __WIN32_DEFNS_H__
#define __WIN32_DEFNS_H__

#pragma warning(disable : 4996)
#pragma warning(disable : 4197)
#pragma warning(disable : 4100)
#pragma warning(disable : 4127)

#ifndef WIN32
#define WIN32
#endif

//////////////////////////////////////////////////////////////////////////
//include directives
//////////////////////////////////////////////////////////////////////////

#include <time.h>
#include <intrin.h>
#include <Windows.h>
#include <process.h>

#define USE_WIN_THRADS

#ifndef USE_WIN_THRADS
	#include <pthread.h>
	#include <sched.h>
	#include <semaphore.h>
#endif

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
#define inline_ __forceinline
#define CACHE_LINE_SIZE 64

#define get_marked_ref(_p)      ((void *)(((ptr_t)(_p)) | 1U))
#define get_unmarked_ref(_p)    ((void *)(((ptr_t)(_p)) & ~1U))
#define is_marked_ref(_p)       (((ptr_t)(_p)) & 1U)

#define ALIGNED_MALLOC(_s, _a)	_aligned_malloc(_s, _a)
#define ALIGNED_FREE(_p)			_aligned_free(_p)

//////////////////////////////////////////////////////////////////////////
//compare and set 
//////////////////////////////////////////////////////////////////////////
#define CAS32(_d, _c, _x)	_InterlockedCompareExchange  ((long volatile *)_d, _x, _c)
#define CAS64(_d, _c, _x)	_InterlockedCompareExchange64((long long volatile *)_d, _x, _c)
#define CASPO(_d, _c, _x)	(void*)_InterlockedCompareExchange  ((long volatile *)_d, (long)_x, (long)_c)

#define SWAP32(_a, _n)		_InterlockedExchange((long*)_a, _n)
#define SWAPPO(_a, _n)		(void*)_InterlockedExchange((long*)_a, (long)_n)

////////////////////////////////////////////////////////////////////////////////
//memory management
//------------------------------------------------------------------------------
//	WMB(): All preceding write operations must commit before any later writes.
//	RMB(): All preceding read operations must commit before any later reads.
//	MB():  All preceding memory accesses must commit before any later accesses.
////////////////////////////////////////////////////////////////////////////////

#pragma intrinsic(_WriteBarrier)
#pragma intrinsic(_ReadBarrier)
#pragma intrinsic(_ReadWriteBarrier)

#define RMB() _ReadBarrier()
#define WMB() _WriteBarrier()
#define MB()  _ReadWriteBarrier()

inline unsigned MUTEX_ENTER(unsigned volatile* x) {
	if(0 == SWAP32(x, 0xFF)) {
		RMB();
		return 0;
	} else
		return 0xFF;
}

inline void MUTEX_EXIT(unsigned volatile* x) {
	WMB();
	SWAP32(x, 0);
}

//////////////////////////////////////////////////////////////////////////
//CPU counters
//////////////////////////////////////////////////////////////////////////
inline_ tick_t RDTICK() {return (tick_t)GetTickCount() | (((tick_t)GetTickCount()) << 32);}

//////////////////////////////////////////////////////////////////////////
//bit operations
//////////////////////////////////////////////////////////////////////////
inline_ _u32 bit_count(const _u32 x) {
	_u32 r = x - ((x >> 1) & 033333333333) - ((x >> 2) & 011111111111);
	return ((r + (r >> 3)) & 030707070707) & 63;
}

inline_ _u32 bit_count64(const _u64 x) {
	return bit_count((_u32)x) + bit_count((_u32)(x>>32));
}

inline_ int first_lsb_bit_indx(_u32 x) {
	if(0==x)
		return -1;
	register unsigned long i;
	_BitScanForward(&i ,x);
	return (int)i;
}

inline_ int first_msb_bit_indx(_u32 x) {
	if(0==x)
		return -1;
	register unsigned long i;
	_BitScanReverse(&i ,x);
	return (int)i;
}

inline_ int first_lsb_bit_indx64(_u64 x) {
	if(0==x)
		return -1;
	else if(0 == (x & 0xFFFFFFFFULL)) {
		x >>= 32;
		register unsigned long i;
		_BitScanForward(&i ,(_u32)x);
		return (int)i+32;
	} else {
		register unsigned long i;
		_BitScanForward(&i ,(_u32)x);
		return (int)i;
	}
}

inline_ int first_msb_bit_indx64(_u64 x) {
	if(0==x)
		return -1;
	else if(0 == (x & 0xFFFFFFFFULL)) {
		x >>= 32;
		register unsigned long i;
		_BitScanReverse(&i ,(_u32)x);
		return (int)i+32;
	} else {
		register unsigned long i;
		_BitScanReverse(&i ,(_u32)x);
		return (int)i;
	}

}

//////////////////////////////////////////////////////////////////////////
//pthread emulation
//////////////////////////////////////////////////////////////////////////

#ifdef USE_WIN_THRADS

struct timespec {
	long tv_sec;
	long tv_nsec;
};

//-mutex----------------------------------------
typedef CRITICAL_SECTION pthread_mutex_t; 
typedef int pthread_mutexattr_t;
inline_ int pthread_mutex_init(pthread_mutex_t* p_mutex, const pthread_mutexattr_t *attr) { 
	InitializeCriticalSection(p_mutex);
	return 0;
} 

inline_ int pthread_mutex_destroy(pthread_mutex_t* p_mutex) { 
	DeleteCriticalSection(p_mutex);
	return 0;
} 

inline_ int pthread_mutex_lock(pthread_mutex_t* p_mutex) { 
	EnterCriticalSection(p_mutex);
	return 0;
} 


inline_ int pthread_mutex_trylock(pthread_mutex_t *p_mutex) {
	if(TRUE == TryEnterCriticalSection(p_mutex))
		return 0;
	else 
		return -1;
}


inline_ int pthread_mutex_unlock(pthread_mutex_t* p_mutex) { 
	LeaveCriticalSection(p_mutex);
	return 0;
} 

//-condition-----------------------------------------
enum COND_TYPE {_COND_TYPE_SIGNAL = 0, _COND_TYPE_BROADCAST = 1};
typedef int pthread_condattr_t;

struct pthread_cond_t { 
	int volatile	_threads_counter; 
	HANDLE			_cond_lock; 
	HANDLE			_cond_event_ary[2]; 
}; 

inline_ int pthread_cond_init(pthread_cond_t *pCond, const pthread_condattr_t *attr) { 
	pCond->_threads_counter = 0; 
	pCond->_cond_lock = CreateSemaphore(NULL,1,1,NULL); 
	pCond->_cond_event_ary[_COND_TYPE_BROADCAST] = CreateEvent(NULL,TRUE,FALSE,NULL); 
	pCond->_cond_event_ary[_COND_TYPE_SIGNAL] = CreateEvent(NULL,FALSE,FALSE,NULL); 
	return 0;
} 

inline_ int pthread_cond_destroy(pthread_cond_t* pCond) { 
	CloseHandle(pCond->_cond_lock); 
	CloseHandle(pCond->_cond_event_ary[_COND_TYPE_SIGNAL]); 
	CloseHandle(pCond->_cond_event_ary[_COND_TYPE_BROADCAST]); 
	return 0;
} 

inline_ int pthread_cond_wait(pthread_cond_t* pCond, pthread_mutex_t* pMutex) { 
	WaitForSingleObject(pCond->_cond_lock, INFINITE); 
	_ReadWriteBarrier();
	++(pCond->_threads_counter); 
	_ReadWriteBarrier();
	ReleaseSemaphore(pCond->_cond_lock,1,NULL); 

	pthread_mutex_unlock(pMutex); 

	unsigned long event_type = WaitForMultipleObjects(2, pCond->_cond_event_ary,FALSE,INFINITE);
	if(WAIT_FAILED == event_type) {
		pthread_mutex_lock(pMutex); 
		return -1;
	}

	event_type -= WAIT_OBJECT_0;
	if(event_type == (_COND_TYPE_SIGNAL)) {
		_ReadWriteBarrier();
		--(pCond->_threads_counter);
		_ReadWriteBarrier();
		ReleaseSemaphore(pCond->_cond_lock, 1, NULL);
	} else if(event_type == (_COND_TYPE_BROADCAST)) {
		if (0 == --(pCond->_threads_counter)) { 
			_ReadWriteBarrier();
			ResetEvent(pCond->_cond_event_ary[_COND_TYPE_BROADCAST]); 
			ReleaseSemaphore(pCond->_cond_lock,1,NULL); 
		} 
	}
	pthread_mutex_lock(pMutex); 
	return 0;
} 

inline_ int pthread_cond_timedwait(pthread_cond_t* pCond, pthread_mutex_t* pMutex, const struct timespec *abstime) { 
	WaitForSingleObject(pCond->_cond_lock, INFINITE); 
	_ReadWriteBarrier();
	++(pCond->_threads_counter); 
	_ReadWriteBarrier();
	ReleaseSemaphore(pCond->_cond_lock,1,NULL); 

	pthread_mutex_unlock(pMutex); 
	unsigned long event_type = WaitForMultipleObjects(2, pCond->_cond_event_ary,FALSE,(abstime->tv_sec) * 1000 + (abstime->tv_nsec)/1000000);
	if(WAIT_FAILED == event_type || WAIT_TIMEOUT == event_type) {
		pthread_mutex_lock(pMutex); 
		return -1;
	}

	event_type -= WAIT_OBJECT_0;
	if(event_type == (_COND_TYPE_SIGNAL)) {
		WaitForSingleObject(pCond->_cond_lock, INFINITE); 
		_ReadWriteBarrier();
		--(pCond->_threads_counter);
		_ReadWriteBarrier();
		ReleaseSemaphore(pCond->_cond_lock, 1, NULL);
	} else if(event_type == (_COND_TYPE_BROADCAST)) {
		if (0 == --(pCond->_threads_counter)) { 
			WaitForSingleObject(pCond->_cond_lock, INFINITE); 
			_ReadWriteBarrier();
			ResetEvent(pCond->_cond_event_ary[_COND_TYPE_BROADCAST]); 
			ReleaseSemaphore(pCond->_cond_lock,1,NULL); 
		} 
	}
	pthread_mutex_lock(pMutex); 
	return 0;
} 

inline_ int pthread_cond_broadcast (pthread_cond_t *pCond) { 
	WaitForSingleObject(pCond->_cond_lock, INFINITE);
	_ReadWriteBarrier();
	if (pCond->_threads_counter) 
		SetEvent (pCond->_cond_event_ary[_COND_TYPE_BROADCAST]);
	else 
		ReleaseSemaphore(pCond->_cond_lock,1,NULL); 
	return 0;
} 

inline_ int pthread_cond_signal (pthread_cond_t *pCond) { 
	WaitForSingleObject(pCond->_cond_lock, INFINITE);
	_ReadWriteBarrier();
	if (pCond->_threads_counter) 
		SetEvent (pCond->_cond_event_ary[_COND_TYPE_SIGNAL]);
	else 
		ReleaseSemaphore(pCond->_cond_lock,1,NULL); 
	return 0;
} 

//-semaphore-----------------------------------------
struct sem_t {
	pthread_mutex_t			_mutex;
	pthread_cond_t				_change_cond;
	unsigned int volatile	_permits;
	unsigned int volatile	_threads_counter;
};

inline_ int sem_init (sem_t *pSem, int pshared, unsigned int value) {
	pthread_mutex_init (&pSem->_mutex,0);
	pthread_cond_init (&pSem->_change_cond,0);
	pSem->_permits = value;
	pSem->_threads_counter = 0;
	return 0;
}

inline_ int sem_destroy(sem_t *pSem) {
	pthread_cond_destroy (&pSem->_change_cond);
	pthread_mutex_destroy (&pSem->_mutex);
	return 0;
}

inline_ int sem_wait (sem_t *pSem) {
	pthread_mutex_lock (&pSem->_mutex);
	++(pSem->_threads_counter); 
	while (0 == (pSem->_permits)) {
		_ReadWriteBarrier();
		pthread_cond_wait (&pSem->_change_cond, &pSem->_mutex);
	}
	--(pSem->_threads_counter);
	--(pSem->_permits);
	_ReadWriteBarrier();
	pthread_mutex_unlock (&pSem->_mutex);
	return 0;
}

inline_ int sem_trywait(sem_t *pSem) {
	pthread_mutex_lock (&pSem->_mutex);
	++(pSem->_threads_counter);
	_ReadWriteBarrier();
	if ( 0 == (pSem->_permits) ) {
		--(pSem->_threads_counter);
		_ReadWriteBarrier();
		pthread_mutex_unlock (&pSem->_mutex);
		return -1;
	}
	--(pSem->_threads_counter);
	--(pSem->_permits);
	_ReadWriteBarrier();
	pthread_mutex_unlock (&pSem->_mutex);
	return 0;
}

inline_ int sem_post (sem_t *pSem) {
	pthread_mutex_lock (&pSem->_mutex);
	_ReadWriteBarrier();
	if ((pSem->_threads_counter) > 0)
		pthread_cond_signal (&pSem->_change_cond);
	++(pSem->_permits);
	_ReadWriteBarrier();
	pthread_mutex_unlock (&pSem->_mutex);
	return 0;
}

inline_ int sem_getvalue(sem_t *pSem, int *sval) {
	_ReadWriteBarrier();
	*sval = (pSem->_permits); 
	return 0;
}


//-thread local -------------------------------------
typedef DWORD pthread_key_t;
inline_ int pthread_key_create(pthread_key_t *key, void (*destructor)(void*)) {
	*key = TlsAlloc();
	return 0;
}

inline_ int pthread_key_delete(pthread_key_t key) {
	TlsFree(key);
	return 0;
}

inline_ void *pthread_getspecific(pthread_key_t key) {
	return TlsGetValue(key); 
}

inline_ int pthread_setspecific(pthread_key_t key, const void *value) {
	if(TRUE == TlsSetValue(key, (LPVOID)value))
		return 0;
	else return -1;
}

#endif

//////////////////////////////////////////////////////////////////////////
//sleep
//////////////////////////////////////////////////////////////////////////
inline_ void nanosleep(const struct timespec *req, struct timespec *rem) {
	Sleep((req->tv_sec) * 1000 + ((req->tv_nsec) / 1000000));
}


#endif /* __WIN32_DEFNS_H__ */
