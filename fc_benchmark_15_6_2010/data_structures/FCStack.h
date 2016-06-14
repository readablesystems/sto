#ifndef __FC_STACK__
#define __FC_STACK__

////////////////////////////////////////////////////////////////////////////////
// File    : FCStack.h
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

#include "ITest.h"
#include "../framework/cpp_framework.h"

using namespace CCP;

class FCStack : public ITest {
private:

	//inner classes -------------------------------
	struct Node {
		Node* volatile _next;
		int	_values[_MAX_THREADS];

		static Node* getNewNode(final int num_threads) {
			return (Node*) malloc(sizeof(Node) - (_MAX_THREADS-num_threads-2)*sizeof(int));
		}
	};

	//fields -------------------------------------- 
	int volatile	_req_ary[_MAX_THREADS];

	AtomicInteger	_fc_lock;

	int volatile	_top_indx;
	int volatile	_stack_ary_size;
	int volatile*	_stack_ary;		

	//helper function -----------------------------
	inline_ void flat_combining() {
		//
		int volatile* final stack_ary_end = _stack_ary + _stack_ary_size;		
		register int volatile* stack_ary = &(_stack_ary[_top_indx]);
		register int volatile* req_ary;

		for (register int iTry=0; iTry<3; ++iTry) {
			req_ary = _req_ary;
			for(register int iReq=0; iReq < _NUM_THREADS;) {
				register final int curr_value = *req_ary;
				if(curr_value > _NULL_VALUE) {
					if(stack_ary_end != stack_ary) {
						*stack_ary = curr_value;
						++stack_ary;
						*req_ary = _NULL_VALUE;
					}
				} else if(_DEQ_VALUE == curr_value) {
					if(stack_ary  != _stack_ary) {
						--stack_ary;
						*req_ary = -(*stack_ary);
					} else {
						*req_ary = _NULL_VALUE;
					}
				} 
				++req_ary;
				++iReq;
			}
		}

		//
		_top_indx = (stack_ary - _stack_ary);
	}

public:
	//public operations ---------------------------
	FCStack() 
	{
		for (int iReq=0; iReq<_MAX_THREADS; ++iReq) {
			_req_ary[iReq] = 0;
		}

		_stack_ary_size = 1024;
		_stack_ary = new int[_stack_ary_size];
		_top_indx = 0;
	}

	//push ......................................................
	boolean add(final int iThread, final int inValue) {
		int volatile* final my_value = &(_req_ary[iThread]);
		*my_value = inValue;

		do {
			boolean is_cas = false;
			if(lock_fc(_fc_lock, is_cas)) {
				//sched_start(iThread);
				flat_combining();
				_fc_lock.set(0);
				//sched_stop(iThread);
				return true;
			} else {
				Memory::write_barrier();
				while(_NULL_VALUE != *my_value && 0 != _fc_lock.getNotSafe()) {
					thread_wait(_NUM_THREADS, true);
				} 
				Memory::read_barrier();
				if(_NULL_VALUE == *my_value)
					return true;
			}
		} while(true);
	}

	//pop ......................................................
	int remove(final int iThread, final int inValue) {
		int volatile* final my_value = &(_req_ary[iThread]);
		*my_value = _DEQ_VALUE;

		do {
			boolean is_cas = false;
			if(lock_fc(_fc_lock, is_cas)) {
				//sched_start(iThread);
				flat_combining();
				_fc_lock.set(0);
				//sched_stop(iThread);
				return -*my_value;
			} else {
				Memory::write_barrier();
				while (_DEQ_VALUE == *my_value && 0 != _fc_lock.getNotSafe()) {
					thread_wait(_NUM_THREADS, true);
				} 
				Memory::read_barrier();
				if(_DEQ_VALUE != *my_value)
					return -*my_value;
			}
		} while(true);
	}

	//peek .....................................................
	int contain(final int iThread, final int inValue) {
		return 0;
	}

	//general .....................................................
	int size() {
		return 0;
	}

	final char* name() {
		return "FCStack";
	}


};

#endif
