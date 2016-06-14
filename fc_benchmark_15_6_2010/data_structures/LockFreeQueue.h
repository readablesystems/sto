#ifndef __LOCK_FREE_QUEUE__
#define __LOCK_FREE_QUEUE__

////////////////////////////////////////////////////////////////////////////////
// File    : LockFreeQueue.h
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
#include <iostream>

using namespace CMDR;

template<typename T, T _NULL_VALUE=0>
class LockFreeQueue {
private:

	class Node : public HazardMemory::Object {
	public:
		T						_value;
		AtomicReference<Node>	_next;

		Node(T inValue) {
			_value = inValue;
			_next.set(null);
		}

		virtual ~Node() {}

	public:
		virtual bool isValid() { return true; }
		virtual HazardMemory::Object* getNextValid(const int indx) {
			return _next;
		}
		virtual HazardMemory::Object* getNextValid(const int indx, int& stamp) {
			stamp = 0;
			return _next;
		}

	};

	AtomicReference<Node> _head;
	AtomicReference<Node> _tail;
	SnapshotCounter		 _counter;

public:
	LockFreeQueue(int num_processes) : _counter(num_processes) {
		Node* sentinel = new Node(null);
		_head.set(sentinel);
		_tail.set(sentinel);
	}

	void enq(T item, int id) {
		Node* new_node = new Node(item);		// allocate & initialize new node

		while (true) {							// keep trying
			HazardMemory::Ptr<Node>	pLast;
			HazardMemory::Ptr<Node>	pNext;
			pLast = _tail;						// read _tail
			pNext.extractNext(pLast);		// read next

			if (pLast.getReference() == _tail.get()) {		// are they consistent?
				if (null == pNext.getReference()) {
					if (pLast->_next.compareAndSet(pNext, new_node)) {
						_counter.inc(id);
						_tail.compareAndSet(pLast, new_node);
						return;
					}
				} else {
					_tail.compareAndSet(pLast, pNext);
				}
			}
		}
	}

	T deq(int id) {

		while (true) {
			if(_tail == _head)
				return _NULL_VALUE;
			HazardMemory::Ptr<Node>	pFirst;
			HazardMemory::Ptr<Node>	pLast;
			HazardMemory::Ptr<Node>	pNext;
			pFirst = _head;
			pLast = _tail;
			if(pFirst == pLast)
				return _NULL_VALUE;
			pNext.extractNext(pFirst);

			if (pFirst.getReference() == _head.get()) {		// are they consistent?
				if (pFirst.getReference() == pLast.getReference()) {	// is queue empty or _tail falling behind?
					if (null == pNext.getReference()) {	// is queue empty?
						return _NULL_VALUE;
					}
					// _tail is behind, try to advance
					_tail.compareAndSet(pLast, pNext);
				} else {
					T currValue = pNext->_value; // read value before dequeuing
					if (_head.compareAndSet(pFirst, pNext)) {
						_counter.dec(id);
						pFirst.retire();
						return currValue;
					}
				}
			}
		}
	}

	T peek() {
		HazardMemory::Ptr<Node>	pFirst;
		HazardMemory::Ptr<Node>	pNext;
		pFirst = _head;
		pNext.extractNext(pFirst);
		if (null != pNext.getReference()) {
			return pNext->_value;
		}
		return _NULL_VALUE;
	}

	void clear() {
	}

	bool isEmpty() {
		return false;
	}

	long size() {
		return 0;
	}
};

#endif
