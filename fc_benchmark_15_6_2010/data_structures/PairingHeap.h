
#ifndef __PAIRING_HEAP__
#define __PAIRING_HEAP__

////////////////////////////////////////////////////////////////////////////////
// File    : PairHeap.h
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

using namespace CCP;

class PairHeap {
private:
	struct PairNode {
		int			_element;
		PairNode*	_leftChild;
		PairNode*	_nextSibling;
		PairNode*	_prev;

        PairNode( final int theElement ) {
			_element     = theElement;
			_leftChild   = null;
			_nextSibling = null;
			_prev        = null;
        }
    };

	PairNode*	_root;
	PairNode**	_treeArray;
	int			_tree_ary_size;

	PairNode* compareAndLink( PairNode* first, PairNode* second ) {
		if( null == second )
			return first;

		if( (second->_element) < (first->_element) ) {
			second->_prev = first->_prev;
			first->_prev = second;
			first->_nextSibling = second->_leftChild;
			if( null != (first->_nextSibling) )
				first->_nextSibling->_prev = first;
			second->_leftChild = first;
			return second;
		} else {
			second->_prev = first;
			first->_nextSibling = second->_nextSibling;
			if( null != (first->_nextSibling) )
				first->_nextSibling->_prev = first;
			second->_nextSibling = first->_leftChild;
			if( null != (second->_nextSibling) )
				second->_nextSibling->_prev = second;
			first->_leftChild = second;
			return first;
		}
	}

	PairNode** doubleIfFull( PairNode** inAry, int index ) {
		if ( index == _tree_ary_size ) {
			PairNode** oldArray = inAry;

			_tree_ary_size = (2 * index);
			inAry = new PairNode*[ 2 * index ];
			for( int i = 0; i < index; ++i )
				inAry[i] = oldArray[i];
		}
		return inAry;
	}

	PairNode* combineSiblings( PairNode* firstSibling ) {
		if( null == firstSibling->_nextSibling )
			return firstSibling;

		int numSiblings = 0;
		for( ; null != firstSibling; ++numSiblings ) {
			_treeArray = doubleIfFull( _treeArray, numSiblings );
			_treeArray[ numSiblings ] = firstSibling;
			firstSibling->_prev->_nextSibling = null;  // break links
			firstSibling = firstSibling->_nextSibling;
		}

		_treeArray = doubleIfFull( _treeArray, numSiblings );
		_treeArray[ numSiblings ] = null;

		int i = 0;
		for( ; i + 1 < numSiblings; i += 2 ) {
			_treeArray[ i ] = compareAndLink( _treeArray[ i ], _treeArray[ i + 1 ] );
		}

		int j = i - 2;
		if( j == (numSiblings - 3) ) {
			_treeArray[ j ] = compareAndLink( _treeArray[ j ], _treeArray[ j + 2 ] );
		}

		for( ; j >= 2; j -= 2 ) {
			_treeArray[ j - 2 ] = compareAndLink( _treeArray[ j - 2 ], _treeArray[ j ] );
		}

		return _treeArray[ 0 ];
	}

	int findMin() {
		if( isEmpty() )
			return null;
		return _root->_element;
	}

	void decreaseKey( PairNode* p, final int newVal ) {
		if( p->_element < newVal )
			return;

		p->_element = newVal;
		if( p != _root ) {
			if( null != p->_nextSibling )
				p->_nextSibling->_prev = p->_prev;
			if( p == p->_prev->_leftChild )
				p->_prev->_leftChild = p->_nextSibling;
			else
				p->_prev->_nextSibling = p->_nextSibling;

			p->_nextSibling = null;
			_root = compareAndLink( _root, p );
		}
	}

public:

    PairHeap() {
		_root = null;
		_tree_ary_size = 5; 
		_treeArray = new PairNode*[ _tree_ary_size ];
		for (int i=0; i<_tree_ary_size; ++i) {
			_treeArray[i]= null; 
		}
    }

    PairNode* insert( final int x ) {
		PairNode* newNode = new PairNode( x );

		if( null ==_root )
			_root = newNode;
		else
			_root = compareAndLink( _root, newNode );
		return newNode;
	}

    int deleteMin() {
		if( isEmpty() )
			return null;

		final int x = findMin();
		if( null == _root->_leftChild )
			_root = null;
		else
			_root = combineSiblings( _root->_leftChild );
		return x;
    }

    boolean isEmpty() {
		return (null == _root);
    }

    void makeEmpty() {
		_root = null;
    }


};

#endif
