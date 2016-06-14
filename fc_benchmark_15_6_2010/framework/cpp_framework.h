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
// TODO:
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
//include directives
////////////////////////////////////////////////////////////////////////////////
#include "portable_defns.h"

namespace CCP {
	/////////////////////////////////////////////////////////////////////////////
	//system 
	/////////////////////////////////////////////////////////////////////////////
	class System {
	public:
		inline_ static tick_t read_cpu_ticks() {return RDTICK();}

		inline_ static tick_t currentTimeMillis() {
			timeb tp;
			ftime(&tp);
			return 1000*(tp.time) + tp.millitm;
		}

		inline_ static void exit(const int exit_code) {
			::exit(exit_code);
		}
	};

	class Math {
	public:
		inline_ static double ceil(double a) {return ::ceil(a);}
		inline_ static double floor(double a) {return ::floor(a);}

		inline_ static int Max(final int a, final int b) {if(a >= b) return a; else return b;}
		inline_ static int Min(final int a, final int b) {if(a <= b) return a; else return b;}
	};

	class Memory {
	public:
		static const int CACHELINE_SIZE = CACHE_LINE_SIZE;

		inline_ static void* byte_malloc(const size_t size)	{return malloc(size);}
		inline_ static void  byte_free(void* mem)					{free(mem);}

		inline_ static void* byte_aligned_malloc(const size_t size) {return ALIGNED_MALLOC(size, CACHELINE_SIZE);}
		inline_ static void* byte_aligned_malloc(const size_t size, const size_t alignment) {return ALIGNED_MALLOC(size,alignment);}
		inline_ static void  byte_aligned_free(void* mem)	{ALIGNED_FREE(mem);}

		inline_ static void  read_write_barrier()	{ MB();	}
		inline_ static void  write_barrier()		{ WMB(); }
		inline_ static void  read_barrier()			{ RMB(); }

		inline_ static _u32  compare_and_set(_u32 volatile* _a, _u32 _o, _u32 _n)		{return CAS32(_a,_o,_n);}
		inline_ static void* compare_and_set(void* volatile * _a, void* _o, void* _n)	{return (void*)CASPO(_a,_o,_n);}
		inline_ static _u64  compare_and_set(_u64 volatile* _a, _u64 _o, _u64 _n)		{return CAS64(_a,_o,_n);}

		inline_ static _u32  exchange_and_set(_u32 volatile *   _a, _u32  _n)	{return SWAP32(_a,_n);}
		inline_ static void* exchange_and_set(void * volatile * _a, void* _n)	{return SWAPPO(_a,_n);}
	};

	/////////////////////////////////////////////////////////////////////////////
	//Volatile like Java memory model (use only for basic types)
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
	/////////////////////////////////////////////////////////////////////////////
	//locks - 
	/////////////////////////////////////////////////////////////////////////////
	class DummyLock {
	public:
		DummyLock() {}
		~DummyLock() {}

		inline_ void init() {}

		inline_ void lock() {}
		inline_ bool tryLock() {return true;}
		inline_ bool isLocked() {return false;}

		inline_ void unlock() {}
	};

	class TTASLock {
	public:
		VolatileType<_u32> _lock;

		TTASLock() : _lock(0) {}
		~TTASLock() {_lock=0;}

		inline_ void init() {_lock = 0;}

		inline_ void lock() {
			do {
				if(0 == _lock && (0 == MUTEX_ENTER(&_lock))) {
					return;
				}
			} while(true);
		}

		inline_ bool tryLock() {
			return ( 0 == _lock && 0 == MUTEX_ENTER(&_lock) );
		}

		inline_ bool isLocked() {
			return 0 != _lock;
		}

		inline_ void unlock() {
			MUTEX_EXIT(&_lock);
		}
	};

	class TASLock {
	public:
		VolatileType<_u32> _lock;

		TASLock() : _lock(0) {}
		~TASLock() {_lock=0;}

		inline_ void init() {_lock = 0;}

		inline_ void lock() {
			do {
				if (0 == MUTEX_ENTER(&_lock)) {
					return;
				}
			} while(true);
		}

		inline_ bool tryLock() {
			return ( 0 == MUTEX_ENTER(&_lock) );
		}

		inline_ bool isLocked() {
			return 0 != _lock;
		}

		inline_ void unlock() {
			MUTEX_EXIT(&_lock);
		}
	};

	class ReentrantLock {
	public:
		ReentrantLock()	{pthread_mutex_init(&_mutex,0);}
		~ReentrantLock()	{pthread_mutex_destroy(&_mutex);}

 		inline_ void lock()		{pthread_mutex_lock( &_mutex );}
		inline_ bool tryLock()	{return (0 == pthread_mutex_trylock(&_mutex));}
		inline_ void unlock()	{pthread_mutex_unlock( &_mutex );}
	private:
		pthread_mutex_t _mutex;
	};

	class ReentrantReadWriteLock {
		pthread_mutex_t	_lock;
		pthread_cond_t		_read, _write;
		VolatileType<int>	_readers;
		VolatileType<int>	_writers;
		VolatileType<int>	_read_waiters;
		VolatileType<int>	_write_waiters;

		ReentrantReadWriteLock() {
			_readers.set(0);
			_writers.set(0);
			_read_waiters.set(0);
			_write_waiters.set(0);
			pthread_mutex_init (&_lock,0);    
			pthread_cond_init  (&_read,0);    
			pthread_cond_init  (&_write,0);
		}
		
		inline_ void readerLock() {
			pthread_mutex_lock(&_lock);
			if (_write_waiters.get() > 0 || _writers.get() > 0) {
				++_read_waiters;
				do {
					pthread_cond_wait(&_read, &_lock);
				} while (_writers.get()>0 || _write_waiters.get()>0);
				--_read_waiters;
			}
			++_readers;
			pthread_mutex_unlock(&_lock);
		}

		inline_ void readerUnlock() {
			pthread_mutex_lock(&_lock);
			--_readers;
			if (_write_waiters.get()>0)
				pthread_cond_signal(&_write);
			pthread_mutex_unlock(&_lock);
		}

		inline_ void writerLock() {
			pthread_mutex_lock(&_lock);
			if (_writers.get()>0 || _readers.get()>0) {
				++_write_waiters;
				do {
					pthread_cond_wait(&_write, &_lock);
				} while (_writers.get()>0 || _readers.get()>0);
				--_write_waiters;
			}
			_writers.set(1);
			pthread_mutex_unlock(&_lock);
		}

		inline_ void writerUnlock() {
			pthread_mutex_lock(&_lock);
			_writers.set(0);
			if (_write_waiters.get() > 0)
				pthread_cond_signal(&_write);
			else if (_read_waiters.get() > 0)
				pthread_cond_broadcast(&_read);
			pthread_mutex_unlock(&_lock);
		}
	};

	class Semaphore {
		sem_t _sema;
	public:

		Semaphore(int permits) {
			 sem_init (&_sema, 0, permits);
		}

		~Semaphore() {
			sem_destroy (&_sema);
		}

		inline_ void acquire() {
			sem_wait (&_sema);
		}

		inline_ bool tryAcquire() {
			return 0 == sem_trywait (&_sema);
		}

		inline_ void release() {
			sem_post(&_sema);
		}

		inline_ int availablePermits() {
			int sval;
			sem_getvalue(&_sema, &sval);
			return sval;
		}
	};

	class Condition {
		pthread_mutex_t	_lock;
		pthread_cond_t		_cond;
	public:
		Condition() {
			pthread_mutex_init (&_lock,0);    
			pthread_cond_init (&_cond,0);    
		}

		inline_ void await() {
			pthread_mutex_lock(&_lock);
			pthread_cond_wait(&_cond, &_lock);
			pthread_mutex_unlock(&_lock);
		}

		inline_ long awaitNanos(long nanosTimeout) {
			pthread_mutex_lock(&_lock);
			const timespec time_wait = {0, nanosTimeout};
			int rc = pthread_cond_timedwait (&_cond, &_lock, &time_wait);
			pthread_mutex_unlock(&_lock);
			if(0==rc)
				return nanosTimeout;
			else return 0;
		}

		inline_ void signal() {
			pthread_mutex_lock(&_lock);
			pthread_cond_signal(&_cond);
			pthread_mutex_unlock(&_lock);
		}

		inline_ void signalAll() {
			pthread_mutex_lock(&_lock);
			pthread_cond_broadcast(&_cond);
			pthread_mutex_unlock(&_lock);
		}
	};

	/////////////////////////////////////////////////////////////////////////////
	//thread local
	/////////////////////////////////////////////////////////////////////////////
	template<typename T>
	class ThreadLocal {
		pthread_key_t	_key_handle;
	public:
		ThreadLocal() {
			pthread_key_create(&_key_handle, 0);
		}

		~ThreadLocal() {
			pthread_key_delete(_key_handle);
		}

		virtual T initialValue() = 0; //called if not exits on thread

		T get() {
			void* curr_key = pthread_getspecific(_key_handle);
			if(0 == curr_key) {
				pthread_setspecific(_key_handle, (void*)initialValue());
				curr_key = pthread_getspecific(_key_handle);
			}
			return (T)((ptr_t)curr_key);
		}

		void set(const T& value) {
			pthread_setspecific(_key_handle, (void*)value);
		}
	};

	/////////////////////////////////////////////////////////////////////////////
	//atomic 
	/////////////////////////////////////////////////////////////////////////////
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
			return (expect == CAS32(&_value, expect, update));
		}  
		inline_ int getAndIncrement() {
			do {
				const int value = _value;
				if (value == CAS32(&_value, value, value + 1))  {
					return value;		
				}
			} while(true);
		}
		inline_ int getAndDecrement() {
			do {
				const int value = _value;
				if (value == CAS32(&_value, value, value - 1)) {
					return value;		
				}
			} while(true);
		}
		inline_ int incrementAndGet() {
			do {
				const int value = _value;
				if (value == CAS32(&_value, value, value + 1)) {
					return value+1;
				}
			} while(true);
		}
		inline_ int decrementAndGet() {
			do {
				const int value = _value;
				if (value == CAS32(&_value, value, value - 1)) {
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
				if (value == CAS32(&_value, value, value+delta)) {
					return value;		
				}
			} while(true);
		} 
		inline_ int addAndGet(const int delta) {
			do {
				const int value = _value;
				if (value == CAS32(&_value, value, value+delta)) {
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

	class AtomicLong {
	private:
		VolatileType<_u64> _value;
	public:
		explicit AtomicLong() {
			_value=0;
		}
		explicit AtomicLong(const _u64 value) {
			_value = value;
		}

		inline_ void set(const _u64 newValue) {
			_value = newValue;
		}
		inline_ bool compareAndSet(const _u64 expect, const _u64 update) {
			return (expect == CAS64(&_value, expect, update));
		}  
		inline_ _u64 getAndIncrement() {
			do {
				const _u64 value = _value;
				if (value == CAS64(&_value, value, value + 1)) {
					return value;		
				}
			} while(true);
		}
		inline_ _u64 getAndDecrement() {
			do {
				const _u64 value = _value;
				if (value == CAS64(&_value, value, value - 1)) {
					return value;		
				}
			} while(true);
		}

		inline_ _u64 incrementAndGet() {
			do {
				const _u64 value = _value;

				if (value == CAS64(&_value, value, value + 1)) {
					return value+1;		
				}

			} while(true);
		}
		inline_ _u64 decrementAndGet() {
			do {
				const _u64 value = _value;
				if (value == CAS64(&_value, value, value - 1)) {
					return value-1;		
				}
			} while(true);
		}

		inline_ _u64 getAndSet(const _u64 newValue) {
			do {
				_u64 value = _value;
				if (value == CAS64(&_value, value, newValue)) {
					return value;		
				}
			} while(true);
		}
		inline_ _u64 getAndAdd(const _u64 delta) {
			do {
				const _u64 value = _value;
				if (value == CAS64(&_value, value, value+delta)) {
					return value;		
				}
			} while(true);
		} 
		inline_ _u64 addAndGet(const _u64 delta) {
			do {
				const _u64 value = _value;
				if (value == CAS64(&_value, value, value+delta)) {
					return value+delta;
				}
			} while(true);
		}
		inline_ int intValue() {
			return (int)_value;
		}
		inline_ _u64 longValue()  {
			return _value;
		}
		inline_ _u64 get() {
			return _value;
		} 
		inline_ _u64 getNotSafe() {
			return _value.getNotSafe();
		} 
	};

	class AtomicBoolean {
	private:
		VolatileType<_u32> _value;
	public:
		explicit AtomicBoolean() {_value=FALSE;}
		explicit AtomicBoolean(const bool value) {
			if(value)
				_value = TRUE;
			else
				_value = FALSE;
		}

		inline_ void set(const bool newValue) {
			if(newValue)
				_value = TRUE;
			else
				_value = FALSE;
		}

		inline_ bool compareAndSet(const bool expect, const bool update) {
			int i_expect, i_update;
			if(expect) i_expect=TRUE; else i_expect=FALSE; 
			if(update) i_update=TRUE; else i_update=FALSE; 
			return (i_expect == CAS32(&_value, i_expect, i_update));
		}  

		inline_ bool getAndSet(const bool newValue) {
			int i_newValue;
			if(newValue) i_newValue=TRUE; else i_newValue=FALSE; 

			do {
				int value = _value;
				if (value == CAS32(&_value, value, i_newValue)) {
					return (TRUE == value);		
				}
			} while(true);
		}

		inline_ bool get() {
			return (TRUE == _value);
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
			return (expect == CASPO((volatile void *)&_value, (void *)expect, (void *)update));
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

	template<typename V>
	class AtomicMarkableReference {
	private:
		VolatileType<V*> _value;
	public:
		explicit AtomicMarkableReference() {_value=0;}
		explicit AtomicMarkableReference(V* const initialRef, const bool initialMark) {
			if(initialMark) {
				_value = (V*)(get_marked_ref(initialRef));
			} else {
				_value = (V*)(get_unmarked_ref(initialRef));
			}
		}
		inline_ operator V*() {  //convention
			return getReference();
		}
		inline_ V* operator->() {
			return getReference();
		}
		inline_ bool attemptMark(V* const expectedReference, const bool newMark)  {
			V* exp_value;
			V* new_value;

			if(0 != is_marked_ref(_value.get())) {
				exp_value = (V * volatile)get_marked_ref(expectedReference);
			} else {
				exp_value = (V * volatile)get_unmarked_ref(expectedReference);
			}

			if(newMark) {
				new_value = (V * volatile)get_marked_ref(expectedReference);
			} else {
				new_value = (V * volatile)get_unmarked_ref(expectedReference);
			}
			bool rc = exp_value == CASPO((volatile void *)&_value, (void*)exp_value, (void*)new_value);
			return rc;
		}

		inline_ bool compareAndSet(V* const expectedReference, V* const newReference, const bool expectedMark, const bool newMark) {
			V* expexted_value;
			V* new_value;
			if(expectedMark) {
				expexted_value = (V * volatile)get_marked_ref(expectedReference);
			} else {
				expexted_value = (V * volatile)get_unmarked_ref(expectedReference);
			}

			if(newMark) {
				new_value = (V * volatile)get_marked_ref(newReference);
			} else {
				new_value = (V * volatile)get_unmarked_ref(newReference);
			}
			bool rc = (expexted_value == CASPO((volatile void *)&_value, (void*)expexted_value, (void*)new_value));
			return rc;
		}

		inline_ V* get(bool volatile * markHolder) const {
			V* save_value = _value.get();
			*markHolder = (0 != is_marked_ref(save_value));
			return (V*) get_unmarked_ref(save_value);
		}

		inline_ V* getReference() {
			return (V*)get_unmarked_ref(_value.get());
		}

		inline_ bool isMarked() const {
			return (0 != is_marked_ref(_value.get()));
		}

		inline_ _u32 getStamp() {
			if(0 == is_marked_ref(_value.get()))
				return 0;
			else return 1;
		}

		inline_ void set(V* newReference, bool newMark) {
			if(newMark) {
				_value = (V*)(get_marked_ref(newReference));
			} else {
				_value = (V*)(get_unmarked_ref(newReference));
			}
		}

		inline_ bool operator==(const AtomicMarkableReference& AMR)	{
			bool volatile otherMark;
			V* const otherPtr = AMR.get(&otherMark);
			bool volatile thisMark;
			V* const thisPtr = get(&thisMark);
			return (otherMark == thisMark && otherPtr == thisPtr);
		}

		inline_ void operator=(const AtomicMarkableReference& AMR) {/*error*/
			bool volatile otherMark;
			V* const otherPtr = AMR.get(&otherMark);
			set(otherPtr, otherMark);
		}

		inline_ bool isEqual(V* const in_ref, const _u32 in_stamp) const {
			bool	thisMark;
			void* const thisPtr = get(&thisMark);
			_u32	thisMarkInt;
			if(thisMark) thisMarkInt=1; else thisMarkInt=0;
			return (in_stamp == thisMarkInt && in_ref == thisPtr);
		}
	};
	
	#if defined(WIN32) || defined(INTEL) || defined(SPARC)
		template<typename V>
		class AtomicStampedReference {
		private:
			VolatileType<_u64> _value;
		public:
			explicit AtomicStampedReference() {
				_value = 0;
			}

			explicit AtomicStampedReference(V* const initialRef, const _u32 initialStamp) {
				_value = ((((_u64)initialRef) << 32) | ((_u64)initialStamp));
			}

			inline_ operator V*() {  //convention
				return getReference();
			}

			inline_ V* operator->() {
				return getReference();
			}

			inline_ V* getReference() {
				return ( (V*) ((ptr_t)(_value.get() >> 32)) );
			}

			inline_ V* get(_u32 volatile * stampHolder) const {
				_u64 save_value = _value.get();
				*stampHolder = ((_u32) (save_value & 0xFFFFFFFFUL));
				return (V*) ((ptr_t)(save_value >> 32));
			}

			inline_ V* getNotSafe(_u32 volatile * stampHolder) const {
				final _u64 save_value = _value.getNotSafe();
				*stampHolder = ((_u32) (save_value & 0xFFFFFFFFUL));
				return (V*) ((ptr_t)(save_value >> 32));
			}

			inline_ _u32 getStamp() {
				_u64 save_value = _value.get();
				return ((_u32) (save_value & 0xFFFFFFFFUL));
			}

			inline_ bool compareAndSet(V* const expectedReference, V* const newReference, const _u32 expectedStamp, const _u32 newStamp) {
				_u64 exp_value = ((((_u64)expectedReference) << 32) | ((_u64)expectedStamp));
				_u64 new_value = ((((_u64)newReference) << 32) | ((_u64)newStamp));
				return (exp_value == CAS64(&_value, exp_value, new_value));
			}

			inline_ void set(V* const newReference, const _u32 newStamp) {
				_value = ((((_u64)newReference) << 32) | ((_u64)newStamp));
			}
			inline_ void setNotSafe(V* const newReference, const _u32 newStamp) {
				_value.setNotSafe( ((((_u64)newReference) << 32) | ((_u64)newStamp)) );
			}

			inline_ bool attemptStamp(V* const expectedReference, const _u32 newStamp) {
				_u64 save_value = _value.get();
				_u64 exp_value = ((((_u64)expectedReference) << 32) | (save_value & 0xFFFFFFFFUL));
				_u64 new_value = ((((_u64)expectedReference) << 32) | ((_u64)newStamp));
				return (exp_value == CAS64(&_value, exp_value, new_value));
			}

			inline_ bool operator==(const AtomicStampedReference& ASR)	{
				_u32 volatile otherMark;
				V* const otherPtr = ASR.get(&otherMark);
				_u32 volatile thisMark;
				V* const thisPtr = get(&thisMark);
				return (otherMark == thisMark && otherPtr == thisPtr);
			}

			inline_ void operator=(const AtomicStampedReference& ASR) {/*error*/
				_u32 volatile otherMark;
				V* const otherPtr = ASR.get(&otherMark);
				this->set(otherPtr, otherMark);
			}

			inline_ bool isEqual(V* const in_ref, const _u32 in_stamp)	const {
				_u32 volatile thisMark;
				void* const thisPtr = get(&thisMark);
				return (in_stamp == thisMark && in_ref == thisPtr);
			}

		};

	#else

		template<typename V>
		class AtomicStampedReference {
		private:
			VolatileType<_u64> _value;
		public:
			explicit AtomicStampedReference() {
				_value = 0;
			}

			explicit AtomicStampedReference(V* const initialRef, const _u32 initialStamp) {
				_value = ((((_u64)initialRef) << 16) | ((_u64)(initialStamp & 0xFFFF)));
			}

			inline_ operator V*() {  //convention
				return getReference();
			}

			inline_ V* operator->() {
				return getReference();
			}

			inline_ V* getReference() {
				return ( (V*) ((ptr_t)(_value.get() >> 16)) );
			}

			inline_ V* get(_u32 volatile * stampHolder) const {
				final _u64 save_value( _value.get() );
				*stampHolder = ((_u32) (save_value & 0xFFFFUL));
				return (V*) ((ptr_t)(save_value >> 16));
			}

			inline_ V* getNotSafe(_u32 volatile * stampHolder) const {
				final _u64 save_value( _value.getNotSafe() );
				*stampHolder = ((_u32) (save_value & 0xFFFFUL));
				return (V*) ((ptr_t)(save_value >> 16));
			}

			inline_ _u32 getStamp() {
				_u64 save_value = _value.get();
				return ((_u32) (save_value & 0xFFFFUL));
			}

			inline_ bool compareAndSet(V* const expectedReference, V* const newReference, const _u32 expectedStamp, const _u32 newStamp) {
				_u64 exp_value = ((((_u64)expectedReference) << 16) | ((_u64)(expectedStamp & 0xFFFF)));
				_u64 new_value = ((((_u64)newReference) << 16) | ((_u64)(newStamp & 0xFFFF)));
				return (exp_value == CAS64(&_value, exp_value, new_value));
			}

			inline_ void set(V* const newReference, const _u32 newStamp) {
				_value = ((((_u64)newReference) << 16) | ((_u64)(newStamp & 0xFFFF)));
			}

			inline_ void setNotSafe(V* const newReference, const _u32 newStamp) {
				_value.setNotSafe( ((((_u64)newReference) << 16) | ((_u64)(newStamp & 0xFFFF))) );
			}

			inline_ bool attemptStamp(V* const expectedReference, const _u32 newStamp) {
				_u64 save_value = _value.get();
				_u64 exp_value = ((((_u64)expectedReference) << 16) | (save_value & 0xFFFFUL));
				_u64 new_value = ((((_u64)expectedReference) << 16) | ((_u64)(newStamp & 0xFFFF)));
				return (exp_value == CAS64(&_value, exp_value, new_value));
			}

			inline_ bool operator==(const AtomicStampedReference& ASR)	{
				_u32 volatile otherMark;
				V* const otherPtr = ASR.get(&otherMark);
				_u32 volatile thisMark;
				V* const thisPtr = get(&thisMark);
				return (otherMark == thisMark && otherPtr == thisPtr);
			}

			inline_ void operator=(const AtomicStampedReference& ASR) {/*error*/
				_u32 volatile otherMark;
				V* const otherPtr = ASR.get(&otherMark);
				this->set(otherPtr, otherMark);
			}

			inline_ bool isEqual(V* const in_ref, const _u32 in_stamp)	const {
				_u32 volatile thisMark;
				void* const thisPtr = get(&thisMark);
				return (in_stamp == thisMark && in_ref == thisPtr);
			}

		};

	#endif

	/////////////////////////////////////////////////////////////////////////////
	//thread class
	/////////////////////////////////////////////////////////////////////////////
	typedef void(*EndThreadFunc)(void*);

	class Thread;
	extern __thread__	Thread* _g_tls_current_thread;
	#define _MAX_END_CALLBACKS (16)

	#ifdef USE_WIN_THRADS

		class Thread {
		public:
			AtomicInteger	_is_start;

		public://ctor & dtor
			Thread(unsigned int stackSize = 0, bool is_create_only_on_start=false) : _is_start(0), _maxEndCallbacks(0), _is_create_only_on_start(is_create_only_on_start), _stackSize(stackSize) 
			{ 
				_hThread = 0;
				_threadId = 0;
				
				for (int iCallback=0; iCallback < _MAX_END_CALLBACKS; ++iCallback) {
					_endCallBacks[iCallback]		= null;
					_endCallBacksParam[iCallback]	= null;
				}

				if(!_is_create_only_on_start) {
					_hThread = reinterpret_cast<HANDLE>(_beginthreadex(0, _stackSize, threadFunc, this, 0/*CREATE_SUSPENDED*/, (unsigned int*)&_threadId));
					if (!_hThread)
						throw "Thread creation failed";
				}
			}

			virtual ~Thread() {
			}

			void destroy() {
				TerminateThread(_hThread, 0);
			}

		public://thread functionality
			void start() {
				if(!_is_create_only_on_start) {
					_is_start.set(1);
				} else {
					if(0 == _is_start.get()) {
						_start_lock.lock();
						if(0 == _is_start.get()) {
							_hThread = reinterpret_cast<HANDLE>(_beginthreadex(0, _stackSize, threadFunc, this, 0/*CREATE_SUSPENDED*/, (unsigned int*)&_threadId));

							if (!_hThread)
								throw "Thread creation failed";

							_is_start.set(1);
						}
						_start_lock.unlock();
					}
				}
			}

			bool is_started() {
				return (0 != _is_start.get());
			}

			void join() {
				WaitForSingleObject(_hThread, INFINITE);
			}
			static void yield() {
				Sleep(0);
			}
			static Thread* currentThread() {
				return _g_tls_current_thread;
			}
			int getPriority() {
				return GetThreadPriority(_hThread);
			}
			void setPriority(const int newPriority) {
				SetThreadPriority(_hThread, newPriority);
			}
			bool isAlive() {
				DWORD exitCode;
				GetExitCodeThread(_hThread, &exitCode);
				return (exitCode == STILL_ACTIVE);
			}
		public:
			static void sleep(const long millis) {
				timespec delta;
				const long num_sec = millis/1000;
				delta.tv_nsec = (millis - num_sec*1000) * 1000000;
				delta.tv_sec = num_sec;
				nanosleep(&delta, 0);
			}

			static void sleep(const long millis, const int nanos) {
				timespec delta;
				const long num_sec = millis/1000;
				delta.tv_nsec = ((millis - num_sec*1000) * 1000000) + nanos;
				delta.tv_sec = num_sec;
				nanosleep(&delta, 0);
			}

			static void set_concurency_level(const int num_threads) {
				return;
			}

		public:
			int GetMinPriority()		{return _MIN_PRIORITY;}
			int GetNormalPriority() {return _NORM_PRIORITY;}
			int GetMaxPriority()		{return _MAX_PRIORITY;}

		public:
			bool add_end_callback(EndThreadFunc endFunc, void* endFuncParam) {
				if(_maxEndCallbacks.get() < _MAX_END_CALLBACKS ) {

					const int my_index = _maxEndCallbacks.getAndIncrement();
					if(my_index < _MAX_END_CALLBACKS) {
						_endCallBacks[my_index]			= endFunc; 
						_endCallBacksParam[my_index]	= endFuncParam;
						return true;
					} else {
						return false;
					}
				} else {
					return false;
				}
			}

		protected:
			virtual void run() = 0;

		private:
			unsigned int threadId() const {
				return _threadId;
			}

		private:
			TASLock							_start_lock;
			const bool						_is_create_only_on_start;
			const unsigned int			_stackSize;
			VolatileType<HANDLE>			_hThread;
			VolatileType<unsigned int>	_threadId;

			AtomicInteger					_maxEndCallbacks;
			EndThreadFunc volatile		_endCallBacks[_MAX_END_CALLBACKS];
			void*	volatile 				_endCallBacksParam[_MAX_END_CALLBACKS];

			static const int _MIN_PRIORITY	= THREAD_PRIORITY_BELOW_NORMAL;
			static const int _NORM_PRIORITY	= THREAD_PRIORITY_NORMAL;
			static const int _MAX_PRIORITY	= THREAD_PRIORITY_TIME_CRITICAL;
		
			static unsigned int __stdcall threadFunc(void *args);
		};

		inline unsigned int __stdcall Thread::threadFunc(void *args) {
			Thread::yield();
			Thread *pThread = reinterpret_cast<Thread*>(args);
			_g_tls_current_thread = pThread;

			while (0 == pThread->_is_start.get()) { Memory::read_write_barrier(); }

			//run thread code
			pThread->run();

			//run end callbacks
			for (int iCallback=0; iCallback < pThread->_maxEndCallbacks.get(); ++iCallback) {
				if(null != pThread->_endCallBacks[iCallback]) {
					pThread->_endCallBacks[iCallback](pThread->_endCallBacksParam[iCallback]); 
				}
			}

			//exit thread
			_endthreadex(0);
			return 0;
		} 

	#else //------------------------------------------------------------------

		extern "C" {
			static void* threadFunc(void *args);
		}

		class Thread {
		public://fields
			pthread_t			_handle;
			AtomicInteger		_is_start;

		public:
			Thread(unsigned int stackSize = 0, bool is_create_only_on_start=false) : _is_start(0), _maxEndCallbacks(0), _is_create_only_on_start(is_create_only_on_start), _stackSize(stackSize) { 
				int ret;

 				//-initialize start condition --------------------

				if(!_is_create_only_on_start) {
					//-set thread attributes -------------------------
					pthread_attr_t tattr;
					ret = pthread_attr_init(&tattr);
					size_t size = stackSize + PTHREAD_STACK_MIN;
					if(size < 128*1024)
						size = 128*1024;
					ret = pthread_attr_setstacksize(&tattr, size);

					//-create the thread ------------------------------
					ret = pthread_create(&_handle, &tattr, threadFunc, (void*)this);
					ret = pthread_attr_destroy(&tattr);

					//-get/set priority consts -----------------------
					sched_param param;
					int policy;
					pthread_getschedparam (_handle, &policy, &param);
					_MIN_PRIORITY = sched_get_priority_min(policy);
					_MAX_PRIORITY = sched_get_priority_max(policy);
					_NORM_PRIORITY = (_MIN_PRIORITY + _MAX_PRIORITY)/2;
				}

				//init end callbacks -----------------------------
				for (int iCallback=0; iCallback < _MAX_END_CALLBACKS; ++iCallback) {
					_endCallBacks[iCallback]		= null;
					_endCallBacksParam[iCallback]	= null;
				}
			}

			virtual ~Thread() {
				//cleanup
			}

			void destroy() {
				pthread_kill(_handle, SIGTERM);
			}

		public:
			void start() {

				if(!_is_create_only_on_start) {
					_is_start.set(1);
				} else {
					if(0 == _is_start.get()) {
						_start_lock.lock();
						if(0 == _is_start.get()) {
							int ret;

							//-set thread attributes -------------------------
							pthread_attr_t tattr;
							ret = pthread_attr_init(&tattr);
							size_t size = _stackSize + PTHREAD_STACK_MIN;
							if(size < 128*1024)
								size = 128*1024;
							ret = pthread_attr_setstacksize(&tattr, size);

							//-create the thread ------------------------------
							ret = pthread_create(&_handle, &tattr, threadFunc, (void*)this);
							ret = pthread_attr_destroy(&tattr);

							//-get/set priority consts -----------------------
							sched_param param;
							int policy;
							pthread_getschedparam (_handle, &policy, &param);
							_MIN_PRIORITY = sched_get_priority_min(policy);
							_MAX_PRIORITY = sched_get_priority_max(policy);
							_NORM_PRIORITY = (_MIN_PRIORITY + _MAX_PRIORITY)/2;

							_is_start.set(1);
						}
						_start_lock.unlock();
					}
				}

			}

			bool is_started() {
				return (0 != _is_start.get());
			}

			void join() {
				void* tmp;
				pthread_join(_handle, &tmp);
			}

			static void yield() {
				sched_yield();
			}
			static Thread* currentThread() {
				return _g_tls_current_thread;
			}

			int getPriority() {
				sched_param param;
				int policy;
				pthread_getschedparam (_handle, &policy, &param);
				return param.sched_priority;
			}

			void setPriority(const int newPriority) {
				sched_param param;
				int policy;
				pthread_getschedparam (_handle, &policy, &param);
				param.sched_priority = newPriority;
				pthread_setschedparam(_handle, policy, &param);
			}

			bool isAlive() {
				return 0 == pthread_kill(_handle, 0);
			}


		public:
			static void sleep(const long millis) {
				timespec delta;
				const long num_sec = millis/1000;
				delta.tv_nsec = (millis - num_sec*1000) * 1000000;
				delta.tv_sec = num_sec;
				nanosleep(&delta, 0);
			}

			static void sleep(const long millis, const int nanos) {
				timespec delta;
				const long num_sec = millis/1000;
				delta.tv_nsec = ((millis - num_sec*1000) * 1000000) + nanos;
				delta.tv_sec = num_sec;
				nanosleep(&delta, 0);
			}

			static void set_concurency_level(const int num_threads) {pthread_setconcurrency(num_threads);}

		public:
			int GetMinPriority()		{return _MIN_PRIORITY;}
			int GetNormalPriority() {return _NORM_PRIORITY;}
			int GetMaxPriority()		{return _MAX_PRIORITY;}

		public:
			bool add_end_callback(EndThreadFunc endFunc, void* endFuncParam) {
				if(_maxEndCallbacks.get() < _MAX_END_CALLBACKS ) {

					const int my_index = _maxEndCallbacks.getAndIncrement();
					if(my_index < _MAX_END_CALLBACKS) {
						_endCallBacks[my_index]			= endFunc; 
						_endCallBacksParam[my_index]	= endFuncParam;
						return true;
					} else {
						return false;
					}
				} else {
					return false;
				}
			}

		public:
			virtual void run() = 0;

			AtomicInteger				_maxEndCallbacks;
			EndThreadFunc volatile	_endCallBacks[_MAX_END_CALLBACKS];
			void*	volatile 			_endCallBacksParam[_MAX_END_CALLBACKS];

		private:
			TASLock					_start_lock;
			const bool				_is_create_only_on_start;
			const unsigned int	_stackSize;

			int _MIN_PRIORITY;
			int _NORM_PRIORITY;
			int _MAX_PRIORITY;
		};   

		extern "C" {
			inline static void* threadFunc(void *args) {
				Thread::yield();
				Thread *pThread = reinterpret_cast<Thread*>(args);
				_g_tls_current_thread = pThread;

				//wait for the thread to start
				while (0 == pThread->_is_start.get()) { Memory::read_write_barrier(); }

				//set thread scheduling 
				//#if defined(INTEL) || defined(INTEL64)
				//sched_param param;
				//param.sched_priority = sched_get_priority_max(SCHED_FIFO);
				//pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
				//#endif

				//run thread code
				pThread->run();

				//run end callbacks
				for (int iCallback=0; iCallback < pThread->_maxEndCallbacks.get(); ++iCallback) {
					if(null != pThread->_endCallBacks[iCallback]) {
						pThread->_endCallBacks[iCallback](pThread->_endCallBacksParam[iCallback]); 
					}
				}

				//exit thread
				pthread_exit(0);

				return 0;
			} 
		}

	#endif

	/////////////////////////////////////////////////////////////////////////////
	//types 
	/////////////////////////////////////////////////////////////////////////////
	class Integer {
	private:
		int _value;

	public://constants
		static const int  MIN_VALUE;
		static const int  MAX_VALUE;
		static const int	SIZE;				//num bits

	public://constructor & destructor
		Integer(const int value) {
			_value = value;
		}

		Integer(const char* s) {
			_value = parseInt(s, 10);
		}

	public://int functionality
		inline_ char byteValue() {
			return (char)_value;
		}

		inline_ short shortValue() {
			return (short)_value;
		}

		inline_ int intValue() {
			return _value;
		}

		inline_ _u64 Value() {
			return (_u64)_value;
		}

		inline_ float floatValue() {
			return (float)_value;
		}

		inline_ double doubleValue() {
			return (double)_value;
		}

		inline_ std::string toString() {
			std::stringstream lStream; 
			lStream << _value;
			return lStream.str();
		}

		inline_ int hashCode() {
			return _value;
		}

		inline_ int compareTo(const Integer& anotherInteger) {
			int thisVal = _value;
			int anotherVal = anotherInteger._value;
			return (thisVal<anotherVal ? -1 : (thisVal==anotherVal ? 0 : 1));
		}

	public://general int functionality

		// Bit twiddling
		inline_ static std::string toString(int i, int radix) {
			char tmp_buf[256*1];
			Itoa (i, tmp_buf,radix);
			return std::string(tmp_buf);
		}

		inline_ static std::string toHexString(const int i) {
			std::stringstream lStream; 
			lStream.flags ( std::ios::hex );
			lStream << i;
			return lStream.str();
		}

		inline_ static std::string toOctalString(const int i) {
			std::stringstream lStream; 
			lStream.flags ( std::ios::oct );
			lStream << i;
			return lStream.str();
		}

		inline_ static std::string toBinaryString(const int i) {
			char tmp_buf[256*1];
			Itoa (i,tmp_buf,2);
			return std::string(tmp_buf);
		}

		inline_ static std::string toString(const int i) {
			std::stringstream lStream;
			lStream << i;
			return lStream.str();
		}

		// Requires positive x
		inline_ static int parseInt(const char* s, const int radix) {
			return strtol(s,null,radix);
		}

		inline_ static int parseInt(const char* s) {
			return parseInt(s,10);
		}

		inline_ static int highestOneBit(int i) {
			// HD, Figure 3-1
			i |= (i >>  1);
			i |= (i >>  2);
			i |= (i >>  4);
			i |= (i >>  8);
			i |= (i >> 16);
			return i - (i >> 1);
		}

		inline_ static int lowestOneBit(const int i) {
			// HD, Section 2-1
			return i & -i;
		}

		inline_ static int numberOfLeadingZeros(int i) {
			// HD, Figure 5-6
			if (i == 0)
				return 32;
			int n = 1;
			if (i >> 16 == 0) { n += 16; i <<= 16; }
			if (i >> 24 == 0) { n +=  8; i <<=  8; }
			if (i >> 28 == 0) { n +=  4; i <<=  4; }
			if (i >> 30 == 0) { n +=  2; i <<=  2; }
			n -= i >> 31;
			return n;
		}

		inline_ static int numberOfTrailingZeros(int i) {
			// HD, Figure 5-14
			int y;
			if (i == 0) return 32;
			int n = 31;
			y = i <<16; if (y != 0) { n = n -16; i = y; }
			y = i << 8; if (y != 0) { n = n - 8; i = y; }
			y = i << 4; if (y != 0) { n = n - 4; i = y; }
			y = i << 2; if (y != 0) { n = n - 2; i = y; }
			return n - ((i << 1) >> 31);
		}

		inline_ static int rotateLeft(const int i, const int distance) {
			return (i << distance) | (i >> -distance);
		}

		inline_ static int rotateRight(const int i, const int distance) {
			return (i >> distance) | (i << -distance);
		}

		inline_ static int reverse(int i) {
			// HD, Figure 7-1
			i = (i & 0x55555555) << 1 | (i >> 1) & 0x55555555;
			i = (i & 0x33333333) << 2 | (i >> 2) & 0x33333333;
			i = (i & 0x0f0f0f0f) << 4 | (i >> 4) & 0x0f0f0f0f;
			i = (i << 24) | ((i & 0xff00) << 8) |
				((i >> 8) & 0xff00) | (i >> 24);
			return i;
		}

		inline_ static int signum(const int i) {
			// HD, Section 2-7
			return (i >> 31) | (-i >> 31);
		}

		inline_ static int reverseBytes(const int i) {
			return ((i >> 24)           )  |
				((i >>   8) &   0xFF00)  |
				((i <<   8) & 0xFF0000)  |
				((i << 24));
		}


		inline_ static _u32 nearestPowerOfTwo(const _u32 i)	{
			_u32 rc( 1 );
			while (rc < i) {
				rc <<= 1;
			}
			return rc;
		}

		inline_ static _u32 log2(const _u32 i) {
			_u32 mask = 1;
			_u32 count=0;
			while(mask < i) {
				mask <<= 1;
				++count;
			}
			return count;
		}

		inline_ static _u32 bitCount(_u32 i) {
			return bit_count(i);
		}

		inline_ static _u32 lsbBitIndx(_u32 x) {
			return first_lsb_bit_indx(x);
		}

		inline_ static _u32 msbBitIndx(_u32 x) {
			return first_msb_bit_indx(x);
		}

	private:
		inline_ static char* Itoa(int value, char* str, int radix) {
			static char dig[] = "0123456789abcdefghijklmnopqrstuvwxyz";
			int n = 0, neg = 0;
			unsigned int v;
			char* p, *q;
			char c;

			if (radix == 10 && value < 0) {
				value = -value;
				neg = 1;
			}
			v = value;
			do {
				str[n++] = dig[v%radix];
				v /= radix;
			} while (v);
			if (neg)
				str[n++] = '-';
			str[n] = '\0';
			for (p = str, q = p + n/2; p != q; ++p, --q)
				c = *p, *p = *q, *q = c;
			return str;
		}
	};

	/////////////////////////////////////////////////////////////////////////////
	//random
	/////////////////////////////////////////////////////////////////////////////
	class Random {
	private:	
		double u[97],c,cd,cm;
		int i97,j97;

	public://ctor
		Random() {
			int ij = (System::read_cpu_ticks() * time(0)) % 31329;
			Thread::sleep(3);
			int kl = (System::read_cpu_ticks() * time(0) * clock()) % 30082;

			double s,t;
			int ii,i,j,k,l,jj,m;

			//Handle the seed range errors
			//First random number seed must be between 0 and 31328
			//Second seed must have a value between 0 and 30081
			if (ij < 0 || ij > 31328 || kl < 0 || kl > 30081) {
				ij = 1802;
				kl = 9373;
			}

			i = (ij / 177) % 177 + 2;
			j = (ij % 177)       + 2;
			k = (kl / 169) % 178 + 1;
			l = (kl % 169);

			for (ii=0; ii<97; ii++) {
				s = 0.0;
				t = 0.5;
				for (jj=0; jj<24; jj++) {
					m = (((i * j) % 179) * k) % 179;
					i = j;
					j = k;
					k = m;
					l = (53 * l + 1) % 169;
					if (((l * m % 64)) >= 32)
						s += t;
					t *= 0.5;
				}
				u[ii] = s;
			}

			c    = 362436.0 / 16777216.0;
			cd   = 7654321.0 / 16777216.0;
			cm   = 16777213.0 / 16777216.0;
			i97  = 97;
			j97  = 33;
		}

	public://NOT THREAD SAFE, EACH THREAD SHOULD USE DIFFRENT OBJECT !!
		inline_ double nextUniform(void) {
			double uni;

			uni = u[i97-1] - u[j97-1];
			if (uni <= 0.0)
				uni++;
			u[i97-1] = uni;
			i97--;
			if (i97 == 0)
				i97 = 97;
			j97--;
			if (j97 == 0)
				j97 = 97;
			c -= cd;
			if (c < 0.0)
				c += cm;
			uni -= c;
			if (uni < 0.0)
				uni++;

			return(uni);
		}

		inline_ double nextGaussian(double mean, double stddev) {
			double  q,u2,v,x,y;

			//Generate P = (u,v) uniform in rect. enclosing acceptance region 
			//Make sure that any random numbers <= 0 are rejected, since
			//Gaussian() requires uniforms > 0, but RandomUniform() delivers >= 0.
			do {
				u2 = nextUniform();
				v = nextUniform();
				if (u2 <= 0.0 || v <= 0.0) {
					u2 = 1.0;
					v = 1.0;
				}
				v = 1.7156 * (v - 0.5);

				//Evaluate the quadratic form
				x = u2 - 0.449871;
				y = fabs(v) + 0.386595;
				q = x * x + y * (0.19600 * y - 0.25472 * x);

				//Accept P if inside inner ellipse
				if (q < 0.27597)
					break;

				//Reject P if outside outer ellipse, or outside acceptance region
			} while ((q > 0.27846) || (v * v > -4.0 * log(u2) * u2 * u2));

			//Return ratio of P's coordinates as the normal deviate
			return (mean + stddev * v / u2);
		}

		inline_  int nextInt(int lower,int upper) {
			//Return random integer within a range, lower -> upper INCLUSIVE
			int tmp = ((unsigned int)(nextUniform() * (upper - lower)) + lower);

			if(tmp >= upper)
				tmp = upper-1;
			return tmp;
		}
		inline_ unsigned int nextInt(int upper) {
			return nextInt(0, upper);
		}

		inline_ _u64 nextLong(_u64 lower,_u64 upper) {
			//Return random integer within a range, lower -> upper INCLUSIVE
			_u64 tmp = ((_u64)(nextUniform() * (upper - lower)) + lower);
			if(tmp >= upper)
				tmp = upper-1;
			return tmp;
		}

		inline_ _u64 nextLong( _u64 upper) {
			return nextLong(0, upper);
		}

		inline_ bool nextBoolean() {return (0 != (nextInt(1024) & 1));}
		inline_ double nextDouble(double lower,double upper) {
			//Return random float within a range, lower -> upper
			return((upper - lower) * nextUniform() + lower);
		}
		inline_ double nextDouble(double upper) {
			return nextDouble(0, upper);
		}

	public://THREAD SAFE !!
		inline_ static _u64 getSeed() {
			//get random the seed
			return (time(0)*System::read_cpu_ticks()) ^ (clock() * System::read_cpu_ticks());
		}
		inline_ static _u64 getRandom(_u64 seed) {
			if(0==seed)
				seed = getSeed();
			seed = seed * 196314165 + 907633515;
			return seed;
		}
		inline_ static int getRandom(_u64 volatile& seed, int inValue) {
			// Return a random number between 0 and value
			if(0==seed)
				seed = getSeed();
			seed = seed * 196314165 + 907633515;
			if (inValue>0)
				return (int)(seed%(inValue));
			else
				return (0);
		}
		inline_ static int getRandomScatter(_u64 seed, int inValue) {
			// Return a scattered random number so inputting value would return -value to +value
			return (int)(getRandom(seed, inValue<<1))-inValue;
		}
	};

	/////////////////////////////////////////////////////////////////////////////
	//Snapshot Counter
	/////////////////////////////////////////////////////////////////////////////
	class SnapshotCounter {
		//constants -----------------------------------
		const int	_NUM_PROCESS;

		//helper functions ----------------------------
		inline_ static int get_size(const _u64 counterAndValue) {
			return (int)(counterAndValue & 0xFFFFFFFFL);
		}
		inline_ static int get_seq(const _u64 counterAndValue) {
			return (int)(counterAndValue >> 32);
		}
		inline_ static _u64 build_seq_value(const int seq, const int value) {
			return (((_u64)seq << 32) | (_u64)value);
		}

		//inner classes -------------------------------
		struct ProcessData {
			_u64 volatile _recent_seq_value;
			_u64 volatile _prev_seq_value;

			ProcessData() {
				_recent_seq_value = 0;
				_prev_seq_value = 0;
			}

			inline_ int recent_seq() const  {
				return get_seq(_recent_seq_value);
			}
			inline_ int recent_seq_int() const  {
				return get_seq(_recent_seq_value);
			}
			inline_ int prev_seq() const  {
				return get_seq(_prev_seq_value);
			}
			inline_ int recent_value() const {
				return get_size(_recent_seq_value);
			}
			inline_ int prev_value() const {
				return get_size(_prev_seq_value);
			}

			inline_ void update(const int counter, const int value) {
				_recent_seq_value = build_seq_value(counter, value);
			}
			inline_ void update_prev(const int counter, const int value) {
				_prev_seq_value = build_seq_value(counter, value);
			}
			inline_ void set_prev_to_curr() {
				_prev_seq_value = _recent_seq_value;
			}
		};

		//fields --------------------------------------
		ProcessData* const _gMem;
		AtomicInteger		 _gSeq;
		AtomicLong			 _gView;

	public:
		//public operations ---------------------------
		SnapshotCounter(const int num_process) 
		: _NUM_PROCESS(num_process),
		  _gMem(new ProcessData[num_process]),
		  _gSeq(1),
		  _gView(0)
		{	}

		~SnapshotCounter() {
			delete [] _gMem;
		}

		inline_ void update(const int iProcess, const int new_value) {
			ProcessData& processData = _gMem[iProcess];			
			const int update_seq = _gSeq.get();
			if (update_seq != processData.recent_seq())
				processData.set_prev_to_curr();
			processData.update(update_seq, new_value);
		}
		inline_ void inc(const int iProcess) {
			ProcessData& processData = _gMem[iProcess];			
			const int update_seq = _gSeq.get();
			if (update_seq != processData.recent_seq())
				processData.set_prev_to_curr();
			processData.update(update_seq, processData.recent_value()+1);
		}
		inline_ void dec(const int iProcess) {
			ProcessData& processData = _gMem[iProcess];			
			const int update_seq = _gSeq.get();
			if (update_seq != processData.recent_seq())
				processData.set_prev_to_curr();
			processData.update(update_seq, processData.recent_value()-1);
		}
		inline_ void add(const int iProcess, const int x) {
			ProcessData& processData = _gMem[iProcess];			
			const int update_seq = _gSeq.get();
			if (update_seq != processData.recent_seq())
				processData.set_prev_to_curr();
			processData.update(update_seq, processData.recent_value()+x);
		}
		inline_ int valueRequest(const int iProcess) {
			return (int)(_gMem[iProcess].recent_value());
		}
		inline_ int scan() {
			//Let others finish the scan (and take their view)
			const int init_seq(_gSeq.get());
			if(init_seq > 2) {
				if(init_seq > (get_seq(_gView.get()))) {
					int iTry=1;
					while((init_seq >= (get_seq(_gView.get()))) && iTry < 1000000)  {++iTry;--iTry; ++iTry;--iTry; ++iTry;}
					if(init_seq < get_seq(_gView.get()))
						return (int)get_size(_gView.get());
				}
			}

			//start scan 
			_u64 start_view(_gView.get());
			int scan_seq = _gSeq.incrementAndGet();
			const int first_seq = scan_seq;
			do {
				int size=0;
				bool scan_ok=true;
				for(int iProcess=0; iProcess < _NUM_PROCESS; ++iProcess) {
					const ProcessData& processData(_gMem[iProcess]);
					const _u64 prev( processData._prev_seq_value );
					const _u64 recent( processData._recent_seq_value );

					if (get_seq(recent) < scan_seq)
						size += processData.recent_value();
					else if (get_seq(recent) == scan_seq)
						size += get_size(prev);
					else {
						scan_ok = false;
						start_view = _gView.get();
						scan_seq = processData.recent_seq();
						break;
					}
				}

				if(first_seq <= get_seq(_gView.get()))
					return get_size(_gView.get());
				if(scan_ok) {
					if(_gView.compareAndSet(start_view, build_seq_value(scan_seq, size)))
						return size;
					start_view = _gView.get();
				}
			} while (true);
		}

		inline_ int scan_sum() {
			return scan();
		}
	};

	/////////////////////////////////////////////////////////////////////////////
	//Hash
	/////////////////////////////////////////////////////////////////////////////
	#if defined(WIN32) || defined(INTEL) || defined(SPARC)

		inline_ _u32 ptr_hash_func(ptr_t key) {
			key = ~key + (key << 15);
			key = key ^ (key >> 12);
			key = key + (key << 2);
			key = key ^ (key >> 4);
			key = (key + (key << 3)) + (key << 11);
			key = key ^ (key >> 16);
			return key;
		}

	#else

		inline_ _u32 ptr_hash_func(ptr_t key) {
			key = (~key) + (key << 18);
			key = key ^ (key >> 31);
			key = key * 21; 
			key = key ^ (key >> 11);
			key = key + (key << 6);
			key = key ^ (key >> 22);
			return (_u32) key;
		}

	#endif

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
