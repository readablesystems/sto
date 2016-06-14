#ifndef __FC_QUEUE__
#define __FC_QUEUE__

////////////////////////////////////////////////////////////////////////////////
// File    : FCQueue.h
// Author  : Ms.Moran Tzafrir;  email: morantza@gmail.com; tel: 0505-779961
// Written : 27 October 2009
//
// Copyright (C) 2009 Moran Tzafrir.
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
#include "ITest.h"

using namespace CCP;

class FCQueue : public ITest {
private:

	//constants -----------------------------------

	//inner classes -------------------------------
	struct Node {
		Node* volatile	_next;
		int	volatile	_values[256];

		static Node* get_new(final int in_num_values) {
			final size_t new_size = (sizeof(Node) + (in_num_values + 2 - 256) * sizeof(int));

			Node* final new_node = (Node*) malloc(new_size);
			new_node->_next = null;
			return new_node;
		}
	};

	//fields --------------------------------------
	AtomicInteger	_fc_lock;
	char							_pad1[CACHE_LINE_SIZE];
	final int		_NUM_REP;
	final int		_REP_THRESHOLD;
	Node* volatile	_head;
	Node* volatile	_tail;
	int volatile	_NODE_SIZE;
	Node* volatile	_new_node;

	//helper function -----------------------------
	inline_ void flat_combining() {
		
		// prepare for enq
		int volatile* enq_value_ary;
		if(null == _new_node) 
			_new_node = Node::get_new(_NODE_SIZE);
		enq_value_ary = _new_node->_values;
		*enq_value_ary = 1;
		++enq_value_ary;

		// prepare for deq
		int volatile * deq_value_ary = _tail->_values;
		deq_value_ary += deq_value_ary[0];

		//
		int num_added = 0;
		for (int iTry=0;iTry<_NUM_REP; ++iTry) {
			Memory::read_barrier();

			int num_changes=0;
			SlotInfo* curr_slot = _tail_slot.get();
			while(null != curr_slot->_next) {
				final int curr_value = curr_slot->_req_ans;
				if(curr_value > _NULL_VALUE) {
					++num_changes;
					*enq_value_ary = curr_value;
					++enq_value_ary;
					curr_slot->_req_ans = _NULL_VALUE;
					curr_slot->_time_stamp = _NULL_VALUE;

					++num_added;
					if(num_added >= _NODE_SIZE) {
						Node* final new_node2 = Node::get_new(_NODE_SIZE+4);
						memcpy((void*)(new_node2->_values), (void*)(_new_node->_values), (_NODE_SIZE+2)*sizeof(int) );
						//free(_new_node);
						_new_node = new_node2; 
						enq_value_ary = _new_node->_values;
						*enq_value_ary = 1;
						++enq_value_ary;
						enq_value_ary += _NODE_SIZE;
						_NODE_SIZE += 4;
					}
				} else if(_DEQ_VALUE == curr_value) {
					++num_changes;
					final int curr_deq = *deq_value_ary;
					if(0 != curr_deq) {
						curr_slot->_req_ans = -curr_deq;
						curr_slot->_time_stamp = _NULL_VALUE;
						++deq_value_ary;
					} else if(null != _tail->_next) {
						_tail = _tail->_next;
						deq_value_ary = _tail->_values;
						deq_value_ary += deq_value_ary[0];
						continue;
					} else {
						curr_slot->_req_ans = _NULL_VALUE;
						curr_slot->_time_stamp = _NULL_VALUE;
					} 
				}
				curr_slot = curr_slot->_next;
			}//while on slots

			//Memory::write_barrier();
			if(num_changes < _REP_THRESHOLD)
				break;
		}//for repetition

		if(0 == *deq_value_ary && null != _tail->_next) {
			_tail = _tail->_next;
		} else {
			_tail->_values[0] = (deq_value_ary -  _tail->_values);
		}

		if(enq_value_ary != (_new_node->_values + 1)) {
			*enq_value_ary = 0;
			_head->_next = _new_node;
			_head = _new_node;
			_new_node  = null;
		} 
	}

public:
	//public operations ---------------------------
	FCQueue() 
	:	_NUM_REP(_NUM_THREADS),
		//_NUM_REP( Math::Min(2, _NUM_THREADS)),
		_REP_THRESHOLD((int)(Math::ceil(_NUM_THREADS/(1.7))))
	{
		_head = Node::get_new(_NUM_THREADS);
		_tail = _head;
		_head->_values[0] = 1;
		_head->_values[1] = 0;

		_tail_slot.set(new SlotInfo());
		_timestamp = 0;
		_NODE_SIZE = 4;
		_new_node = null;
	}

	virtual ~FCQueue() { }

	//enq ......................................................
	boolean add(final int iThread, final int inValue) {
		CasInfo& my_cas_info = _cas_info_ary[iThread];

		SlotInfo* my_slot = _tls_slot_info;
		if(null == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = inValue;

		do {
			if (null == my_next)
				enq_slot(my_slot);

			boolean is_cas = true;
			if(lock_fc(_fc_lock, is_cas)) {
				++(my_cas_info._succ);
				//machine_start_fc(iThread);
				flat_combining();
				_fc_lock.set(0);
				//machine_end_fc(iThread);
				++(my_cas_info._ops);
				return true;
			} else {
				Memory::write_barrier();
				if(!is_cas)
					++(my_cas_info._failed);
				while(_NULL_VALUE != my_re_ans && 0 != _fc_lock.getNotSafe()) {
					thread_wait(iThread);
				} 
				Memory::read_barrier();
				if(_NULL_VALUE == my_re_ans) {
					++(my_cas_info._ops);
					return true;
				}
			}
		} while(true);
	}

	//deq ......................................................
	int remove(final int iThread, final int inValue) {
		CasInfo& my_cas_info = _cas_info_ary[iThread];

		SlotInfo* my_slot = _tls_slot_info;
		if(null == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = _DEQ_VALUE;

		do {
			if(null == my_next)
				enq_slot(my_slot);

			boolean is_cas = true;
			if(lock_fc(_fc_lock, is_cas)) {
				++(my_cas_info._succ);
				//machine_start_fc(iThread);
				flat_combining();
				_fc_lock.set(0);
				//machine_end_fc(iThread);
				++(my_cas_info._ops);
				return -(my_re_ans);
			} else {
				Memory::write_barrier();
				if(!is_cas)
					++(my_cas_info._failed);
				while(_DEQ_VALUE == my_re_ans && 0 != _fc_lock.getNotSafe()) {
					thread_wait(iThread);
				}
				Memory::read_barrier();
				if(_DEQ_VALUE != my_re_ans) {
					++(my_cas_info._ops);
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
		return "FCQueue";
	}

};

////////////////////////////////////////////////////////////////////////////////
// File    : FCQueue.h
// Author  : Ms.Moran Tzafrir;  email: morantza@gmail.com; tel: 0505-779961
// Written : 27 October 2009
//
// Copyright (C) 2009 Moran Tzafrir.
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

#endif
