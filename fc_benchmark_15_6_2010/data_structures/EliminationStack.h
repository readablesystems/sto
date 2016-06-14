#ifndef __ELIMINATION_STACK__
#define __ELIMINATION_STACK__

//------------------------------------------------------------------------------
// START
// File    : EliminationStack.h
// Author  : Ms.Moran Tzafrir; email: morantza@gmail.com; phone: +972-505-779961
// Written : 13 April 2009
// 
// Elimination Stack
//
// Copyright (C) 2009 Moran Tzafrir.
// You can use this file only by explicit written approval from Moran Tzafrir.
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// TODO:
//------------------------------------------------------------------------------

#include "ITest.h"
#include "../framework/cpp_framework.h"

using namespace CCP;

class EliminationStack : public ITest{
private:
	static final int _NULL_VALUE = 0;

	struct Node {
		int volatile	_value;
		int				d1, d2, d3, d4, d5, d6, d7;
		int				d21, d22, d23, d24, d25, d26, d27;
		Node* volatile	_next;

		Node(final int inValue) : _value(inValue), _next(null) {}
	};

	final int				_ELIMINATION_SIZE;
	AtomicReference<Node>	_top;
	AtomicReference<Node>*	_elimination_ary;
	VolatileType<int>		_hint_last_waiter_index;

public:
	EliminationStack() 
	: _top(null), _ELIMINATION_SIZE(_NUM_THREADS/2) 
	{ 
		if(_ELIMINATION_SIZE > 0)
			_elimination_ary = new AtomicReference<Node>[_ELIMINATION_SIZE];
		_hint_last_waiter_index = 0;
	}

	boolean add(final int iThread, final int inValue) {
		Node* nd = new Node(inValue);

		do {
			register Node* final curr_top = _top.getRefNotSafe();
			nd->_next = curr_top;
			Memory::write_barrier();
			if( _top.compareAndSet(curr_top, nd))
				return true;
			else {
				int iCheck = _hint_last_waiter_index;
				for (int i=0; i<_ELIMINATION_SIZE; ++i) {
					final Node* elimination_node = _elimination_ary[iCheck].get();
					if(null == elimination_node && _elimination_ary[iCheck].compareAndSet(null, nd)) {
						_hint_last_waiter_index = iCheck; 
						Thread::yield();
						if(_elimination_ary[iCheck].compareAndSet(nd, null))
							break;
						else 
							return true;
					}
					++iCheck;
					if(iCheck >= _ELIMINATION_SIZE)
						iCheck = 0;
				}
			}
		} while(true);
	}

	int remove(final int iThread, final int inValue) {
		do {
			Memory::write_barrier();
			register Node* final curr_top = _top.getRefNotSafe();
			if(null == curr_top)
				return _NULL_VALUE;

			if( _top.compareAndSet(curr_top, curr_top->_next)) {
				Memory::read_barrier();
				return curr_top->_value;
			} else {
				int iCheck = _hint_last_waiter_index;
				for (int i=0; i<_ELIMINATION_SIZE; ++i) {
					final Node* elimination_node = _elimination_ary[iCheck].getRefNotSafe();
					if(null != elimination_node && _elimination_ary[iCheck].compareAndSet(elimination_node, null)) {
						Memory::read_barrier();
						return elimination_node->_value;
					}
					++iCheck;
					if(iCheck >= _ELIMINATION_SIZE)
						iCheck = 0;
				}
			}
		} while(true);
	}

	int contain(final int iThread, final int inValue) {
		return 0;
	}

	const char* name() {
		return "EliminationStack";
	}

	int size() {
		return 0;
	}

	boolean isEmpty() {
		return (null == _top.get());
	}

	virtual void print_custom() {
	}
};

//------------------------------------------------------------------------------
// END
// File    : EliminationStack.h
// Author  : Ms.Moran Tzafrir; email: morantza@gmail.com; phone: +972-505-779961
// Written : 13 April 2009
// 
// Elimination Stack
//
// Copyright (C) 2009 Moran Tzafrir.
// You can use this file only by explicit written approval from Moran Tzafrir.
//------------------------------------------------------------------------------

#endif
