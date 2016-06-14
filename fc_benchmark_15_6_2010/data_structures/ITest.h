#ifndef __ITEST__
#define __ITEST__

////////////////////////////////////////////////////////////////////////////////
// File    : ITest.h
// Author  : Ms.Moran Tzafrir  email: morantza@gmail.com
// Written : 27 October 2009
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of 
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License 
// along with this program; if not, write to the Free Software Foundation
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
////////////////////////////////////////////////////////////////////////////////
// TODO:
//
////////////////////////////////////////////////////////////////////////////////

#include "../framework/cpp_framework.h"

#if defined(SPARC64) || defined(SPARC)		
	#include <schedctl.h>
#endif

extern int _gNumThreads;

class ITest {
public:

	struct CasInfo {
		int _failed;
		int _succ;
		int _ops;

		CasInfo() {
			_failed	= 0;
			_succ	= 0;
			_ops	= 0;
		}

		void reset() {
			_failed	= 0;
			_succ	= 0;
			_ops	= 0;
		}
	};

	static int _num_post_read_write;

protected:

	static final int _MAX_THREADS	= 1024;
	static final int _NULL_VALUE	= 0;
	static final int _DEQ_VALUE		= (INT_MIN+2);
	static final int _OK_DEQ		= (INT_MIN+3);

	//constants -----------------------------------
	final int		_NUM_THREADS;
	final boolean	_IS_USE_CONDITION;

	CasInfo			_cas_info_ary[_MAX_THREADS];
	int				_cpu_cash_contamination[8*1024*1024];
	int				_iTry[_MAX_THREADS];

	//helper function -----------------------------

	#if defined(SPARC) || defined(SPARC64)		
		schedctl_t*	_schedctl_ary[_MAX_THREADS];

		inline void init_architecture_specific() {
			for(int iReq=0; iReq < _MAX_THREADS; ++iReq)
				_schedctl_ary[iReq] = null;
		}

		inline void machine_start_fc(final int iThread) {
			if(null == _schedctl_ary[iThread])
				_schedctl_ary[iThread] = schedctl_init();
			schedctl_start(_schedctl_ary[iThread]);
		}

		inline void machine_end_fc(final int iThread) {
			schedctl_stop(_schedctl_ary[iThread]);
		}

		inline void thread_wait(final int iThread, final boolean is_short=false) {
			CCP::Memory::read_write_barrier();
			CCP::Memory::read_write_barrier();
			CCP::Memory::read_write_barrier();
			CCP::Memory::read_write_barrier();
			CCP::Memory::read_write_barrier();
			CCP::Memory::read_write_barrier();
		}

		#define lock_fc(x,b) ( 0 == x.getNotSafe() && (b=x.compareAndSet(0, 0xFF)) )

	#else

		inline void init_architecture_specific() { }

		inline void machine_start_fc(final int iThread) { }

		inline void machine_end_fc(final int iThread) { }

		inline void thread_wait(final int iThread, final boolean is_short=false) { 
			if(_NUM_THREADS < 8) {
				CCP::Thread::sleep(0,1);
			} else {
				CCP::Thread::yield();
			}
		}

		#define lock_fc(x,b) ( 0 == x.getNotSafe() &&  0 == x.getNotSafe() &&  0 == x.getNotSafe() && (b=x.compareAndSet(0, 0xFF)) )


	#endif

	//list inner types ------------------------------
	struct SlotInfo {
		int volatile		_req_ans;		//here 1 can post the request and wait for answer
		int volatile		_time_stamp;	//when 0 not connected
		SlotInfo* volatile	_next;			//when null not connected
		void*				_custem_info;

		SlotInfo() {
			_req_ans	 = _NULL_VALUE;
			_time_stamp  = 0;
			_next		 = null;
			_custem_info = null;
		}
	};

	//list fields -----------------------------------
	static __thread__ SlotInfo*		_tls_slot_info;
	CCP::AtomicReference<SlotInfo>	_tail_slot;
	int volatile					_timestamp;

	//list helper function --------------------------
	SlotInfo* get_new_slot() {
		SlotInfo* my_slot= new SlotInfo();
		_tls_slot_info = my_slot;

		SlotInfo* curr_tail;
		do {
			curr_tail = _tail_slot.get();
			my_slot->_next = curr_tail;
		} while(false == _tail_slot.compareAndSet(curr_tail, my_slot));

		return my_slot;
	}

	void enq_slot(SlotInfo* p_slot) {
		SlotInfo* curr_tail;
		do {
			curr_tail = _tail_slot.get();
			p_slot->_next = curr_tail;
		} while(false == _tail_slot.compareAndSet(curr_tail, p_slot));
	}

	void enq_slot_if_needed(SlotInfo* p_slot) {
		if(null == p_slot->_next) {
			enq_slot(p_slot);
		}
	}

public:

	ITest(	final int num_threads = _gNumThreads, 
			final boolean is_use_condition = false) 
	:	_NUM_THREADS(num_threads), 
		_IS_USE_CONDITION(is_use_condition) 
	{
		//init sparc specific
		init_architecture_specific();
	}

	virtual ~ITest() {}
	
	virtual void cas_reset(final int iThread) {
		_cas_info_ary[_MAX_THREADS].reset();;
	}

	virtual void print_cas() {
	}

	virtual void print_custom() {
		int failed = 0;
		int succ = 0;
		int ops = 0;

		for (int i=0; i<_NUM_THREADS; ++i) {
			failed += _cas_info_ary[i]._failed;
			succ += _cas_info_ary[i]._succ;
			ops += _cas_info_ary[i]._ops;
		}
		printf(" 0 0 0 0 0 0 ( %d, %d, %d, %d )", ops, succ, failed, failed+succ);
	}

	virtual int post_computation(final int iThread) {
		int sum=1;
		if(_num_post_read_write > 0) {
			++_iTry[iThread];
			unsigned long start_indx = ((unsigned long)(_iTry[iThread] * (iThread+1) * 17777675))%(7*1024*1024);

			for (unsigned long i=start_indx; i<start_indx+_num_post_read_write; ++i) {
				sum += _cpu_cash_contamination[i];
				_cpu_cash_contamination[i] =  sum;
			}
		}
		return sum;
	}

	//..........................................................................
	virtual boolean add(final int iThread, final int inValue) = 0;
	virtual int remove(final int iThread, final int inValue) = 0;
	virtual int contain(final int iThread, final int inValue) = 0;

	//..........................................................................
	virtual int size() = 0;
	virtual final char* name() = 0;
};

#endif
