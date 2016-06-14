#ifndef __MS_QUEUE__
#define __MS_QUEUE__

////////////////////////////////////////////////////////////////////////////////
// File    : MSQueue.h
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

class MSQueue  : public ITest {
private:

	struct Node {
		AtomicStampedReference<Node>	_next;
		int volatile					_value;

		Node(final int x) : _value(x) {}
	};

	AtomicStampedReference<Node>	_head;
	AtomicStampedReference<Node>	_tail;

public:
	MSQueue() {
		Node* const new_node = new Node(_NULL_VALUE);		// Allocate a free node

		new_node->_next.set(null,0);								// Make it the only node in the linked list
		_head.set(new_node,0);
		_tail.set(new_node,0);
	}

	boolean add(final int iThread, final int inValue) {
		CasInfo& my_cas_info = _cas_info_ary[iThread];

		Node* new_node = new Node(inValue);				// Allocate a new node from the free list
		new_node->_next.set(null, 0);					// Set next pointer of node to NULL
		
		AtomicStampedReference<Node> tail;
		AtomicStampedReference<Node> next;

		do {												// Keep trying until Enqueue is done
			Memory::read_write_barrier();
			tail = _tail;									// Read Tail.ptr and Tail.count together
			next = tail.getReference()->_next;				// Read next ptr and count fields together

			if(tail == _tail)	{							// Are tail and next consistent?
				// Was Tail pointing to the last node?
				if (null == next.getReference()) {
					// Try to link node at the end of the linked list
					if(tail.getReference()->_next.compareAndSet(next.getReference(), new_node, next.getStamp(), next.getStamp()+1)) {
						++(my_cas_info._succ);
						break;	// Enqueue is done.  Exit loop
					} else {
						++(my_cas_info._failed);
					}
				} else {		// Tail was not pointing to the last node Try to swing Tail to the next node
					if(_tail.compareAndSet(tail.getReference(), next.getReference(), tail.getStamp(), next.getStamp()+1))
						++(my_cas_info._succ);
					else
						++(my_cas_info._failed);
				}
			}
		} while(true);

		// Enqueue is done.  Try to swing Tail to the inserted node
		if (_tail.compareAndSet(tail, new_node, tail.getStamp(), tail.getStamp()+1))
			++(my_cas_info._succ);
		else 
			++(my_cas_info._failed);

		++(my_cas_info._ops);
		return true;
	}

	int remove(final int iThread, final int inValue) {
		CasInfo& my_cas_info = _cas_info_ary[iThread];

		AtomicStampedReference<Node> head;
		AtomicStampedReference<Node> tail;
		AtomicStampedReference<Node> next;

		do {													// Keep trying until Dequeue is done
			Memory::read_write_barrier();
			head = _head;									// Read Head
			tail = _tail;									// Read Tail
			next = head.getReference()->_next;		// Read Head.ptr->next

			if(head == _head) {												// Are head, tail, and next consistent?
				if(head.getReference() == tail.getReference()) {	// Is queue empty or Tail falling behind?
					if (null == next.getReference())	{					// Is queue empty?
						++(my_cas_info._ops);
						return _NULL_VALUE;									// Queue is empty, couldn't dequeue
					}
					// Tail is falling behind.  Try to advance it
					if(_tail.compareAndSet(tail.getReference(), next.getReference(), tail.getStamp(), tail.getStamp()+1))
						++(my_cas_info._succ);
					else
						++(my_cas_info._failed);

				} else {		     // No need to deal with Tail
					 // Read value before CAS Otherwise, another dequeue might free the next node
					final int rtrn_value = next.getReference()->_value;

					// Try to swing Head to the next node
					if (_head.compareAndSet(head.getReference(), next.getReference(), head.getStamp(), head.getStamp()+1)) {
						++(my_cas_info._succ);
						//free(head.ptr)		     // It is safe now to free the old node
						++(my_cas_info._ops);
						return rtrn_value;     // Queue was not empty, dequeue succeeded	
					} else {
						++(my_cas_info._failed);
					}
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
		return "MSQueue";
	}

};

#endif

