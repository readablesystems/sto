#ifndef __OYAMA_QUEUE_COM__
#define __OYAMA_QUEUE_COM__

////////////////////////////////////////////////////////////////////////////////
// File    : OyamaQueueCom.h
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
// MERCHANTABILITY99 or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
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
#include "../data_structures/QueueForLogSync.h"

using namespace CCP;

class OyamaQueueCom : public ITest {
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

	struct Req {
		VolatileType<int> _req_ans;
		int _d1,d2,d3,d4;

		Req(final int in_req) : _req_ans(in_req) { }
	};

	//fields --------------------------------------
	AtomicInteger	_log_lock;
	char						_pad1[CACHE_LINE_SIZE];
	QueueForLogSync<Req>	_log;
	Node* volatile	_head;
	Node* volatile	_tail;
	int volatile	_NODE_SIZE;
	Node* volatile	_new_node;

	//helper function -----------------------------
	void execute_log(CasInfo& my_cas_info) {

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

		Req* curr_req = _log.deq(my_cas_info);
		for (int i=0; i<_NUM_THREADS && null != curr_req; ++i) {

			final int curr_value = curr_req->_req_ans;

			if(curr_value > _NULL_VALUE) {
				*enq_value_ary = curr_value;
				++enq_value_ary;
				curr_req->_req_ans = _NULL_VALUE;

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
				final int curr_deq = *deq_value_ary;
				if(0 != curr_deq) {
					curr_req->_req_ans = -curr_deq;
					++deq_value_ary;
				} else if(null != _tail->_next) {
					_tail = _tail->_next;
					deq_value_ary = _tail->_values;
					deq_value_ary += deq_value_ary[0];
					continue;
				} else {
					curr_req->_req_ans = _NULL_VALUE;
				} 
			}
			curr_req = _log.deq(my_cas_info);
		}//while on slots

		////////////////////////

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
	OyamaQueueCom() {
		_head = Node::get_new(_NUM_THREADS);
		_tail = _head;
		_head->_values[0] = 1;
		_head->_values[1] = 0;

		_tail_slot.set(new SlotInfo());
		_timestamp = 0;
		_NODE_SIZE = 4;
		_new_node = null;
	}

	//enq ......................................................
	boolean add(final int iThread, final int inValue) {
		Req* final my_req( new Req(inValue) );
		CasInfo& my_cas_info = _cas_info_ary[iThread];
		_log.enq(my_req, my_cas_info);

		do {
			boolean is_cas = true;
			if(lock_fc(_log_lock, is_cas)) {
				++(my_cas_info._succ);
				execute_log(my_cas_info);
				_log_lock.set(0);
				++(my_cas_info._ops);
				return true;
			} else {
				Memory::write_barrier();
				if(!is_cas)
					++(my_cas_info._failed);
				while(_NULL_VALUE != my_req->_req_ans && 0 != _log_lock.getNotSafe()) {
					thread_wait(iThread);
				} 
				Memory::read_barrier();
				if(_NULL_VALUE !=  my_req->_req_ans) {
					++(my_cas_info._ops);
					return true;
				}
			}
		} while(true);
	}

	//deq ......................................................
	int remove(final int iThread, final int inValue) {
		Req* final my_req( new Req(_DEQ_VALUE) );
		CasInfo& my_cas_info = _cas_info_ary[iThread];
		_log.enq(my_req, my_cas_info);

		do {
			boolean is_cas = true;
			if(lock_fc(_log_lock, is_cas)) {
				++(my_cas_info._succ);
				execute_log(my_cas_info);
				_log_lock.set(0);
				++(my_cas_info._ops);
				return -(my_req->_req_ans);
			} else {
				Memory::write_barrier();
				if(!is_cas)
					++(my_cas_info._failed);
				while(_DEQ_VALUE == (my_req->_req_ans) && 0 != _log_lock.getNotSafe()) {
					thread_wait(iThread);
				} 
				Memory::read_barrier();
				if(_DEQ_VALUE !=  my_req->_req_ans) {
					++(my_cas_info._ops);
					return -(my_req->_req_ans);
				}
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

	const char* name() {
		return "OyamaQueueCom";
	}

};

////////////////////////////////////////////////////////////////////////////////
// File    : OyamaQueueCom.h
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
// MERCHANTABILITY99 or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License 
// along with this program; if not, write to the Free Software Foundation
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
////////////////////////////////////////////////////////////////////////////////

#endif

