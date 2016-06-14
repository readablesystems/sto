#ifndef __FC_PAIRING_HEAP__
#define __FC_PAIRING_HEAP__

////////////////////////////////////////////////////////////////////////////////
// File    : FCPairHeap.h
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

#include "../data_structures/PairingHeap.h"

using namespace CCP;

class FCPairHeap : public ITest {
private:

	//constants -----------------------------------

	//inner classes -------------------------------

	// Private variables --------------------------

	//fields --------------------------------------
	AtomicInteger	_fc_lock;
	char							_pad1[CACHE_LINE_SIZE];
	final int		_NUM_REP;
	final int		_REP_THRESHOLD;
	PairHeap		_heap;

	//helper function -----------------------------
	inline_ void flat_combining() {
		for (int iTry=0;iTry<_NUM_REP; ++iTry) {
			Memory::read_barrier();
			int num_changes=0;
			SlotInfo* curr_slot = _tail_slot.get();
			while(null != curr_slot->_next) {
				final int curr_value = curr_slot->_req_ans;
				if(curr_value > _NULL_VALUE) {
					++num_changes;
					_heap.insert(curr_value);
					curr_slot->_req_ans = _NULL_VALUE;
					curr_slot->_time_stamp = _NULL_VALUE;
				} else if(_DEQ_VALUE == curr_value) {
					++num_changes;
					curr_slot->_req_ans = -(_heap.deleteMin());
					curr_slot->_time_stamp = _NULL_VALUE;
				}
				curr_slot = curr_slot->_next;
			}//while on slots
			if(num_changes < _REP_THRESHOLD)
				break;
			//Memory::write_barrier();
		}//for repetition
	}	
	
public:

    FCPairHeap()
	:	_NUM_REP(_NUM_THREADS),
		_REP_THRESHOLD((int)(Math::ceil(_NUM_THREADS/(1.7))))
	{
		_tail_slot.set(new SlotInfo());
    }

	//enq ......................................................
	boolean add(final int iThread, final int inValue) {
		SlotInfo* my_slot = _tls_slot_info;
		if(null == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = inValue;

		do {
			if (null == my_next)
				enq_slot(my_slot);

			boolean is_cas = false;
			if(lock_fc(_fc_lock, is_cas)) {
				machine_start_fc(iThread);
				flat_combining();
				_fc_lock.set(0);
				machine_end_fc(iThread);
				return true;
			} else {
				Memory::write_barrier();
				while(_NULL_VALUE != my_re_ans && 0 != _fc_lock.getNotSafe()) {
					thread_wait(iThread);
				} 
				Memory::read_barrier();
				if(_NULL_VALUE == my_re_ans) {
					return true;
				}
			}
		} while(true);
	}

	//deq ......................................................
	int remove(final int iThread, final int inValue) {
		SlotInfo* my_slot = _tls_slot_info;
		if(null == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = _DEQ_VALUE;

		do {
			if(null == my_next)
				enq_slot(my_slot);

			boolean is_cas = false;
			if(lock_fc(_fc_lock, is_cas)) {
				machine_start_fc(iThread);
				flat_combining();
				_fc_lock.set(0);
				machine_end_fc(iThread);
				return -(my_re_ans);
			} else {
				Memory::write_barrier();
				while(_DEQ_VALUE == my_re_ans && 0 != _fc_lock.getNotSafe()) {
					thread_wait(iThread);
				}
				Memory::read_barrier();
				if(_DEQ_VALUE != my_re_ans) {
					return -(my_re_ans);
				}
			}
		} while(true);
	}

	//peek .....................................................
	int contain(final int iThread, final int inValue) {
		return _NULL_VALUE;
	}

	//general .....................................................
	int size() {
		return 0;
	}

	final char* name() {
		return "FCPairHeap";
	}

};

#endif
