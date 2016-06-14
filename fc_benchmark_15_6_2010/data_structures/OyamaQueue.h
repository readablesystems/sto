#ifndef __OYAMA_QUEUE__
#define __OYAMA_QUEUE__

////////////////////////////////////////////////////////////////////////////////
// File    : OyamaQueue.h
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

class OyamaQueue : public ITest {
private:

	struct Node {
		const int		_value;
		Node* volatile	_next;

		Node(final int x) : _value(x) {
			_next = null;
		}
	};

	struct Req {
		VolatileType<int> _req_ans;
		int _d1,d2,d3,d4;

		Req(final int in_req) : _req_ans(in_req) { }
	};

	AtomicInteger			_log_lock;
	QueueForLogSync<Req>	_log;
	Node* volatile			_head;
	Node* volatile			_tail;

	void execute_log(CasInfo& my_cas_info) {
		for (int i=0; i<_NUM_THREADS; ++i) {
			Req* final curr_req = _log.deq(my_cas_info);
			if(null == curr_req)
				break;

			final int req_ans = curr_req->_req_ans;
			if(req_ans > _NULL_VALUE) {
				Node* const new_node = new Node(req_ans);				// Allocate a new node from the free list
				new_node->_next = null;									// Set next pointer of node to NULL

				_tail->_next = new_node;								// Link node at the end of the linked list
				_tail = new_node;										// Swing Tail to node

				 curr_req->_req_ans = _NULL_VALUE;
				 //delete curr_req;

			} else if(_DEQ_VALUE == req_ans) {

				Node* const old_head = _head;							// Read Head
				Node* const new_head = old_head->_next;					// Read next pointer

				if(null == new_head) {									// Is queue empty?
					curr_req->_req_ans = _NULL_VALUE;
				} else {
					final int curr_value = new_head->_value;			// Queue not empty.  Read value before release
					_head = new_head;									// Swing Head to next node
					curr_req->_req_ans = -(curr_value);
				}
				//delete curr_req;
			}
		}
	}

public:
	OyamaQueue() {
		Node* const new_node = new Node(_NULL_VALUE);
		new_node->_next = null;
		_head = _tail = new_node;
	}

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
				if(!is_cas)
					++(my_cas_info._failed);
				Memory::write_barrier();
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
				if(!is_cas)
					++(my_cas_info._failed);
				Memory::write_barrier();
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

	int contain(final int iThread, final int inValue) {
		return 0;
	}

	int size() {
		return 0;
	}

	const char* name() {
		return "OyamaQueue";
	}

};

////////////////////////////////////////////////////////////////////////////////
// File    : OyamaQueue.h
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

