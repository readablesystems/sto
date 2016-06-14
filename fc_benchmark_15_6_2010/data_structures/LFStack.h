#ifndef __LF_STACK__
#define __LF_STACK__

////////////////////////////////////////////////////////////////////////////////
// File    : LFStack.h
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

class LFStack : public ITest{
private:

	struct Node {
		int volatile	_value;
		int				d1, d2, d3, d4, d5, d6, d7;
		int				d21, d22, d23, d24, d25, d26, d27;
		Node* volatile	_next;

		Node(final int inValue) : _value(inValue), _next(null) {}
	};

	AtomicReference<Node>	_top;

public:
	LFStack() 
	: _top(null) 
	{ 
	}

	boolean add(final int iThread, final int inValue) {
		Node* final nd = new Node(inValue);
		int backoff = 256;
		do {
			Memory::write_barrier();
			register Node* final curr_top = _top.getReference();
			nd->_next = curr_top;
			if( _top.compareAndSet(curr_top, nd)) {
				return true;
			} 

			for(int w=0; w<backoff; ++ w) {++w; --w;}
			backoff<<=1;

		} while(true);
	}

	int remove(final int iThread, final int inValue) {
		int backoff = 256;
		do {
			Memory::write_barrier();
			register Node* final curr_top = _top.getReference();
			if(null == curr_top)
				return _NULL_VALUE;

			if( _top.compareAndSet(curr_top, curr_top->_next)) {
				Memory::read_barrier();
				return curr_top->_value;
			} 

			for(int w=0; w<backoff; ++ w) {++w; --w;}
			backoff<<=1;

		} while(true);

	}

	int contain(final int iThread, final int inValue) {
		return 0;
	}

	const char* name() {
		return "LFStack";
	}

	int size() {
		return 0;
	}

	boolean isEmpty() {
		return (null == _top.get());
	}

};

#endif
