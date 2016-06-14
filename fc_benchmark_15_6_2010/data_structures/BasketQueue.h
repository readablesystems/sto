#ifndef __BASKETS_QUEUE__
#define __BASKETS_QUEUE__

////////////////////////////////////////////////////////////////////////////////
// File    : BasketQueue.h
// Author  : Ms.Moran Tzafrir  email: morantza@gmail.com
// Written : 27 October 2009
// 
// Basket Queue based on the paper "The Baskets Queue"
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

using namespace CCP;

class BasketsQueue : public ITest {
private:
	static final int _MAX_HOPS = 3;

private:

	struct Node {
		AtomicStampedReference<Node>	_next;
		final int						_value;

		Node() : _value(0), _next(null, 0) {}
		Node(int x) : _value(x), _next(null, 0) {}
	};

	AtomicStampedReference<Node>	_head;
	AtomicStampedReference<Node>	_tail;
	int volatile					_backoff;

	inline_ static _u32 getTag(AtomicStampedReference<Node>& inRef) {
		return ((inRef.getStamp() & ~0x8000) & 0xFFFF);
	}

	inline_ static bool getIsDel(AtomicStampedReference<Node>& inRef) {
		return 0 != (inRef.getStamp() & 0x8000);
	}

	inline_ static int addTag(AtomicStampedReference<Node>&	inRef, final int tagDelta) {
		return ((getTag(inRef) + tagDelta) & ~0x8000) & 0xFFFF;
	}

	inline_ static _u32 addTag(final int tag, final int tagDelta) {
		return ((tag + tagDelta) & ~0x8000) & 0xFFFF;
	}

	inline_ static _u32 createStamp(AtomicStampedReference<Node>&	inRef, final int tagDelta, final boolean isDeleted) {
		int new_stamp = addTag(inRef, tagDelta);
		if(isDeleted)
			new_stamp |= 0x8000;
		return new_stamp;
	}

	inline_ void free_chain(CasInfo& my_cas_info, AtomicStampedReference<Node> head, AtomicStampedReference<Node> new_head) {
		AtomicStampedReference<Node> next;

		if (_head.compareAndSet( head, new_head.getReference(), head.getStamp(), createStamp(head, 1, false))) {
			++(my_cas_info._succ);
			while (head.getReference() != new_head.getReference() ) {
				next = head->_next;
				//reclaim_node(head.ptr) 
				head = next;
			}
		} else {
			++(my_cas_info._failed);
		}
	}

	inline_ static void countBackOff(final int n) {
		for (int i = 0; i < n; i++) {;}
	}

public:
	BasketsQueue(int backoff_start_value=0) 
	: _backoff(backoff_start_value)
	{
		Node* sentinel = new Node();
		_head.set(sentinel, 0);
		_tail.set(sentinel, 0);
	}

	boolean add(final int iThread, final int inValue) {
		CasInfo& my_cas_info = _cas_info_ary[iThread];
		Node* nd = new Node(inValue);
		int backoff = 1;
		AtomicStampedReference<Node> tail, next;

		while (true) {
			Memory::read_write_barrier();
			tail = _tail;
			next = tail->_next;
			if (tail == _tail) {
				if (null == next.getReference()) {
					nd->_next.set(null, createStamp(tail, 2, false));
					if (tail->_next.compareAndSet(next.getReference(), nd, next.getStamp(), createStamp(tail, 1, false) )) {
						++(my_cas_info._succ);
						if(_tail.compareAndSet(tail.getReference(), nd, tail.getStamp(), createStamp(tail, 1, false)))
							++(my_cas_info._succ);
						else 
							++(my_cas_info._failed);

						++(my_cas_info._ops);
						return true;
					} else {
						++(my_cas_info._failed);
					}

					next = tail->_next;
					while( (getTag(next) == addTag(getTag(tail), 1)) && (!getIsDel(next)) ) {
						countBackOff(backoff * (iThread+1)); backoff*=2;
						nd->_next = next;

						if (tail->_next.compareAndSet(next.getReference(), nd, next.getStamp(), createStamp(tail, 1, false))) {
							++(my_cas_info._succ);
							++(my_cas_info._ops);
							return true;
						} else {
							++(my_cas_info._failed);
						}
						next = tail->_next;
					}
				} else {
					while (null != next->_next.getReference() && _tail == tail) {
						next = next->_next;
					}
					if(_tail.compareAndSet(tail.getReference(), next.getReference(), tail.getStamp(), createStamp(tail, 1, false) ))
						++(my_cas_info._succ);
					else 
						++(my_cas_info._failed);
				}
			}
		}
	}

	int remove(final int iThread, final int inValue) {
		CasInfo& my_cas_info = _cas_info_ary[iThread];

		AtomicStampedReference<Node> head, tail, next, iter;
		int backoff = 1;

		while (true) {
			Memory::read_write_barrier();
			head = _head;
			tail = _tail;
			next = head->_next;

			if (head == _head) {
				if (head.getReference() == tail.getReference()) {
					if (null == next.getReference()) {
						++(my_cas_info._ops);
						return _NULL_VALUE;
					}
					while ( (null != next->_next.getReference())  && _tail == tail) {
						next = next->_next;
					}
					if(_tail.compareAndSet(tail.getReference(), next.getReference(), tail.getStamp(), createStamp(tail, 1, false)))
						++(my_cas_info._succ);
					else 
						++(my_cas_info._failed);

				} else {
					iter = head;
					int hops = 0;

					while (getIsDel(next) && (iter.getReference() != tail.getReference()) && _head == head) {
						iter = next;
						next = iter->_next;
						++hops;
					}

					if (_head != head) {
						continue;
					} else if (iter.getReference() == tail.getReference()) {
						free_chain(my_cas_info, head, iter);
					} else {
						final int rc_value = next->_value;
						if (iter->_next.compareAndSet(next, next.getReference(), next.getStamp(), createStamp(next, 1, true)) ) {
							++(my_cas_info._succ);

							if (hops >= _MAX_HOPS) {
								free_chain(my_cas_info, head, next);
							}
							++(my_cas_info._ops);
							return rc_value;
						} else {
							++(my_cas_info._failed);
						}
						countBackOff(backoff * (iThread+1)); backoff*=2;
					}
				}
			}
		}
	}

	int contain(final int iThread, final int inValue) {
		return 0;
	}

public:
	int size() {
		return 0;
	}

	const char* name() {
		return "BasketsQueue";
	}

};

#endif
