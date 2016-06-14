#ifndef __QUEUE_FOR_FOR_SYNC2__
#define __QUEUE_FOR_FOR_SYNC2__

////////////////////////////////////////////////////////////////////////////////
// File    : QueueForLogSync2.h
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

#include "ITest.h"
#include "../framework/cpp_framework.h"

using namespace CCP;

class QueueForLogSync2 {
private:

	class MyTASLock {
	public:
		AtomicInteger _lock;

		MyTASLock() : _lock(0) {}
		~MyTASLock() {_lock.set(0);}

		inline_ void init() {_lock.set(0);}

		inline_ void lock(ITest::CasInfo& my_cas_info) {
			while (false == _lock.compareAndSet(0, 0xff)) {
				++(my_cas_info._failed);
			}
			++(my_cas_info._succ);
			Memory::read_barrier();
		}

		inline_ void unlock() {
			_lock.set(0);
		}
	};

	struct Node {
		Node* volatile	_next;
		int final		_value;

		Node(int final x) : _value(x) {}
	};


	MyTASLock			_tail_lock;
	VolatileType<Node*>	_head;
	VolatileType<Node*>	_tail;

public:
	QueueForLogSync2() {
		Node* const new_node = new Node(null);			// Allocate a free node
		new_node->_next = null;									// Make it the only node in the linked list
		_head = _tail = new_node;								// Both Head and Tail point to it
	}

	boolean enq(final int inValue, ITest::CasInfo& in_cas_info) {

		Node* const new_node = new Node(inValue);				// Allocate a new node from the free list
		new_node->_next = null;									// Set next pointer of node to NULL

		_tail_lock.lock(in_cas_info);							// Acquire T_lock in order to access Tail

		_tail->_next = new_node;								// Link node at the end of the linked list
		_tail = new_node;										// Swing Tail to node

		_tail_lock.unlock();									// Release T_lock
		return true;
	}

	int deq(ITest::CasInfo& in_cas_info) {
		Node* const old_head = _head;							// Read Head
		Node* const new_head = old_head->_next;					// Read next pointer

		if(null == new_head) {									// Is queue empty?
			return 0;											// Queue was empty
		}

		final int curr_value = new_head->_value;			// Queue not empty.  Read value before release
		_head = new_head;										// Swing Head to next node
		return curr_value;										// Queue was not empty, dequeue succeeded
	}

	int get_curr_tail() {
		Node* curr_tail = _tail;
		if(null == curr_tail)
			return null;
		else 
			return curr_tail->_value;
	}
};

////////////////////////////////////////////////////////////////////////////////
// File    : QueueForLogSync2.h
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
