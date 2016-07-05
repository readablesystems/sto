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

#include "FCQueueOriginalHelpers.hh"

using namespace CCP;

template <typename T>
class FCQueue {

private:
	static const int _MAX_THREADS	= 1024;
	static const int _NULL_VALUE	= 0;
	static const int _DEQ_VALUE		= (INT_MIN+2);
	static const int _OK_DEQ		= (INT_MIN+3);
	const int		_NUM_THREADS    = 8;

	//list inner types ------------------------------
	struct SlotInfo {
		int volatile		_req_ans;		//here 1 can post the request and wait for answer
		int volatile		_time_stamp;	//when 0 not connected
		SlotInfo* volatile	_next;			//when NULL not connected
		void*				_custem_info;

		SlotInfo() {
			_req_ans	 = _NULL_VALUE;
			_time_stamp  = 0;
			_next		 = NULL;
			_custem_info = NULL;
		}
	};

	//list fields -----------------------------------
	static thread_local SlotInfo*   _tls_slot_info;
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
		if(NULL == p_slot->_next) {
			enq_slot(p_slot);
		}
	}

	struct Node {
		Node* volatile	_next;
		int	volatile	_values[256];

		static Node* get_new(const int in_num_values) {
			const size_t new_size = (sizeof(Node) + (in_num_values + 2 - 256) * sizeof(int));

			Node* const new_node = (Node*) malloc(new_size);
			new_node->_next = NULL;
			return new_node;
		}
	};

	AtomicInteger	    _fc_lock;
	char							_pad1[CACHE_LINE_SIZE];
	const int		_NUM_REP;
	const int		_REP_THRESHOLD;
	Node* volatile	_head;
	Node* volatile	_tail;
	int volatile	_NODE_SIZE;
	Node* volatile	_new_node;

	inline void flat_combining() {
		// prepare for enq
		int volatile* enq_value_ary;
		if(NULL == _new_node) 
			_new_node = Node::get_new(_NODE_SIZE);
		enq_value_ary = _new_node->_values;
		*enq_value_ary = 1;
		++enq_value_ary;

		// prepare for deq
		int volatile * deq_value_ary = _tail->_values;
		deq_value_ary += deq_value_ary[0];

		//
		int num_added = 0;
		for (int iTry=0; iTry<_NUM_REP; ++iTry) {
			Memory::read_barrier();

			int num_changes=0;
			SlotInfo* curr_slot = _tail_slot.get();
			while(NULL != curr_slot->_next) {
				const int curr_value = curr_slot->_req_ans;
				if(curr_value > _NULL_VALUE) {
					++num_changes;
					*enq_value_ary = curr_value;
					++enq_value_ary;
					curr_slot->_req_ans = _NULL_VALUE;
					curr_slot->_time_stamp = _NULL_VALUE;

					++num_added;
					if(num_added >= _NODE_SIZE) {
						Node* const new_node2 = Node::get_new(_NODE_SIZE+4);
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
					const int curr_deq = *deq_value_ary;
					if(0 != curr_deq) {
						curr_slot->_req_ans = -curr_deq;
						curr_slot->_time_stamp = _NULL_VALUE;
						++deq_value_ary;
					} else if(NULL != _tail->_next) {
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

			if(num_changes < _REP_THRESHOLD)
				break;
		}//for repetition

		if(0 == *deq_value_ary && NULL != _tail->_next) {
			_tail = _tail->_next;
		} else {
			_tail->_values[0] = (deq_value_ary -  _tail->_values);
		}

		if(enq_value_ary != (_new_node->_values + 1)) {
			*enq_value_ary = 0;
			_head->_next = _new_node;
			_head = _new_node;
			_new_node  = NULL;
		} 
	}

public:
	//public operations ---------------------------
	FCQueue() 
	:	_NUM_REP(_NUM_THREADS),
		_REP_THRESHOLD((int)(Math::ceil(_NUM_THREADS/(1.7))))
	{
		_head = Node::get_new(_NUM_THREADS);
		_tail = _head;
		_head->_values[0] = 1;
		_head->_values[1] = 0;

		_tail_slot.set(new SlotInfo());
		_timestamp = 0;
		_NODE_SIZE = 4;
		_new_node = NULL;
	}
	virtual ~FCQueue() { }

	bool push(const int inValue) {

		SlotInfo* my_slot = _tls_slot_info;
		if(NULL == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = inValue;

		do {
			if (NULL == my_next)
				enq_slot(my_slot);

			bool is_cas = true;
			if(lock_fc(_fc_lock, is_cas)) {
				flat_combining();
				_fc_lock.set(0);
				return true;
			} else {
				Memory::write_barrier();
				if(!is_cas)
				while(_NULL_VALUE != my_re_ans && 0 != _fc_lock.getNotSafe()) {
                    sched_yield();
				} 
				Memory::read_barrier();
				if(_NULL_VALUE == my_re_ans) {
					return true;
				}
			}
		} while(true);
	}

	bool pop(int& ret) {
		SlotInfo* my_slot = _tls_slot_info;
		if(NULL == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = _DEQ_VALUE;

		do {
			if(NULL == my_next)
				enq_slot(my_slot);

			bool is_cas = true;
			if(lock_fc(_fc_lock, is_cas)) {
				flat_combining();
				fc_lock.set(0);
                ret = -(my_re_ans);
				return (ret != _NULL_VALUE); 
			} else {
				Memory::write_barrier();
				if(!is_cas)
				while(_DEQ_VALUE == my_re_ans && 0 != _fc_lock.getNotSafe()) {
                    sched_yield();
				}
				Memory::read_barrier();
				if(_DEQ_VALUE != my_re_ans) {
                    ret = -(my_re_ans);
				    return (ret != _NULL_VALUE); 
				}
			}
		} while(true);
	}

	//general .....................................................
	int size() {
		return 0;
	}

    void print_statistics() {
        return;
    }
};

template <typename T>
thread_local typename FCQueue<T>::SlotInfo* FCQueue<T>::_tls_slot_info = NULL;

#endif
