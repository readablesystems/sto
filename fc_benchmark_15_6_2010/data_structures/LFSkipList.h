#ifndef __LF_SKIP_LIST__
#define __LF_SKIP_LIST__

//------------------------------------------------------------------------------
// File    : LFSkipList.h
// Author  : Ms.Moran Tzafrir; email: morantza@gmail.com; phone: +972-505-779961
// Written : 13 April 2009
// 
// Lock Free Skip List
//
// Copyright (C) 2009 Moran Tzafrir.
// You can use this file only by explicit written approval from Moran Tzafrir.
//------------------------------------------------------------------------------
// TODO: 
//------------------------------------------------------------------------------
#include "ITest.h"
#include "../framework/cpp_framework.h"

using namespace CCP;

class LFSkipList : public ITest {
protected://static
	static final int _MAX_LEVEL = 30;

protected://types

	class Node {
	public:
		final int								_key;
		final int								_topLevel;
		AtomicMarkableReference<Node>* final	_next;

		Node(final int key) 
		:	_key(key), 
			_topLevel(_MAX_LEVEL), 
			_next(new AtomicMarkableReference<Node>[_MAX_LEVEL + 1])
		{}// constructor for sentinel nodes

		Node(final int key, final int height) 
		:	_key(key), 
			_topLevel(height), 
			_next(new AtomicMarkableReference<Node>[height + 1])
		{}// constructor for regular nodes
	};

protected://fields
	VolatileType<_u64>	_random_seed;
	VolatileType<Node*>	_head;
	VolatileType<Node*>	_tail;
	TTASLock			_lock_removemin;

protected://methods
	int randomLevel() {
		int x = (int)(_random_seed.get()  & 0xFFFFFFUL);
		x ^= x << 13;
		x ^= x >> 17;
		_random_seed.set( x ^= x << 5 );
		if ((x & 0x80000001) != 0) {// test highest and lowest bits
			//printf("0\n");  fflush(stdout);
			return 1;
		}
		int level = 2;
		while (((x >>= 1) & 1) != 0) 
			++level;
		//printf("%d\n",level);  fflush(stdout);
		if(level > (_MAX_LEVEL-1))
			return (_MAX_LEVEL-1);
		else
			return level;
	}

	Node* find(final int key, Node** preds, Node** succs) {
		boolean marked = false;
		boolean snip;
		Node* pPred;
		Node* pCurr;
		Node* pSucc;

	find_retry:
		int iLevel;
		while (true) {
			pPred = _head;
			for (iLevel = _MAX_LEVEL-1; iLevel >= 0; --iLevel) {
				pCurr = pPred->_next[iLevel].getReference();
				while (true) {
					pSucc = pCurr->_next[iLevel].get(&marked);
					while (marked) {           // replace curr if marked
						snip = pPred->_next[iLevel].compareAndSet(pCurr, pSucc, false, false);
						if (!snip) 
							goto find_retry;

						//if(++(pCurr->_level_removed) == pCurr->_topLevel)
						//	pCurr.retire();

						pCurr = pPred->_next[iLevel].getReference();
						pSucc = pCurr->_next[iLevel].get(&marked);
					}
					if (pCurr->_key < key) { // move forward same iLevel
						pPred = pCurr;
						pCurr = pSucc;
					} else {
						break; // move to _next iLevel
					}
				}
				if(null != preds)
					preds[iLevel] = pPred;
				if(null != succs)
					succs[iLevel] = pCurr;
			}

			if (pCurr->_key == key) // bottom iLevel curr._key == key
				return pCurr;
			else 
				return null;
		}
	}

public://methods

	LFSkipList() 
	:	_random_seed( Random::getSeed() ),
		_head( new Node(INT_MIN) ),
		_tail( new Node(INT_MAX) )
	{
		for (int iLevel = 0; iLevel < _head->_topLevel; ++iLevel) {
			_head->_next[iLevel].set(_tail, false);
		}
	}

	~LFSkipList() {
		delete _head;
		delete _tail;
		//need to delete the rest
	}

	//enq ......................................................
	boolean add(final int iThread, final int inValue) {
		Node* preds[_MAX_LEVEL + 1];
		Node* succs[_MAX_LEVEL + 1];
		Node* pPred;
		Node* pSucc;
		Node* new_node = null;
		int topLevel = 0;

		while (true) {
			Node* final found_node = find(inValue, preds, succs);
			//if(null != found_node) {
			//	return true;
			//}

			// try to splice in new node in 0 going up
			pPred = preds[0];
			pSucc = succs[0];

			if (null == new_node) {
				topLevel = randomLevel();
				new_node = new Node(inValue, topLevel);
			}

			// prepare new node
			for (int iLevel = 1; iLevel < topLevel; ++iLevel) {
				new_node->_next[iLevel].set(succs[iLevel], false);
			}
			new_node->_next[0].set(pSucc, false);

			if (false == (pPred->_next[0].compareAndSet(pSucc, new_node, false, false))) {// lin point
				continue; // retry from start
			}

			// splice in remaining levels going up
			for (int iLevel = 1; iLevel < topLevel; ++iLevel) {
				while (true) {
					pPred = preds[iLevel];
					pSucc = succs[iLevel];
					if (pPred->_next[iLevel].compareAndSet(pSucc, new_node, false, false)) {
						break;
					}
					find(inValue, preds, succs); // find new preds and succs
				}
			}

			return true;
		}
	}

	//deq ......................................................
	int remove(final int iThread, final int inValue) {
		Node* succs[_MAX_LEVEL + 1];
		Node* pSucc;
		_lock_removemin.lock();
		while (true) {

			Node* pNodeToRemove = _head->_next[0];

			if(_tail == pNodeToRemove) {
				_lock_removemin.unlock();
				return _NULL_VALUE;
			}
			find(pNodeToRemove->_key, null, succs);
			pNodeToRemove = succs[0];

			//logically remove node
			for (int iLevel = pNodeToRemove->_topLevel-1; iLevel > 0; --iLevel) {
				boolean marked = false;
				pSucc = pNodeToRemove->_next[iLevel].get(&marked);
				while (!marked) { // until I succeed in marking
					pNodeToRemove->_next[iLevel].attemptMark(pSucc, true);
					pSucc = pNodeToRemove->_next[iLevel].get(&marked);
				}
			}

			// proceed to remove from bottom iLevel ............................
			boolean marked = false;
			pSucc = pNodeToRemove->_next[0].get(&marked);
			while (true) { // until someone succeeded in marking
				final boolean iMarkedIt = pNodeToRemove->_next[0].compareAndSet(pSucc, pSucc, false, true);
				pSucc = succs[0]->_next[0].get(&marked);
				if (iMarkedIt) {
					// run find to remove links of the logically removed node
					find(inValue, null, succs);
					_lock_removemin.unlock();
					return pNodeToRemove->_key;
				} else if (marked) {
					_lock_removemin.unlock();
					return _NULL_VALUE; // someone else removed node
				}
				// else only pSucc changed so repeat
			}
			_lock_removemin.unlock();
			return pNodeToRemove->_key;
		}
	}

	//peek .....................................................
	int contain(final int iThread, final int inKey) {
		Node* pPred;
		Node* pCurr;
		Node* pSucc;

		pPred = _head;
		boolean marked = false;
		for (int iLevel = _MAX_LEVEL; iLevel >= 0; --iLevel) {
			pCurr = pPred->_next[iLevel].getReference();
			//if(pCurr == _tail)
			//	return _NULL_VALUE;
			while (true) {
				pSucc = pCurr->_next[iLevel].get(&marked);
				while ( marked) {
					pCurr = pCurr->_next[iLevel];
					pSucc = pCurr->_next[iLevel].get(&marked);
				}

				if (inKey > pCurr->_key) { // move forward same iLevel
					pPred = pCurr;
					pCurr = pSucc;
				} else {
					break; // move to _next iLevel
				}
			}
		}
		if (inKey == pCurr->_key) // bottom iLevel curr._key == key
			return pCurr->_key;
		else return _NULL_VALUE;
	}

	//general .....................................................
	int size() {
		return 0;
	}

	final char* name() {
		return "LFSkipList";
	}
};

/*
int remove(final int iThread, final int inValue) {
	final int 0 = 0;
	Node* succs[_MAX_LEVEL + 1];
	Node* pSucc;
	Node* pNodeToRemove;

	while (true) {
		final boolean found = find(inValue, null, succs);
		if (!found) { //if found it's not marked
			return _NULL_VALUE;
		} else {
			//logically remove node
			pNodeToRemove = succs[0];

			for (int iLevel = pNodeToRemove->_topLevel; iLevel > 0; --iLevel) {
				boolean marked = false;
				pSucc = pNodeToRemove->_next[iLevel].get(&marked);
				while (!marked) { // until I succeed in marking
					pNodeToRemove->_next[iLevel].attemptMark(pSucc, true);
					pSucc = pNodeToRemove->_next[iLevel].get(&marked);
				}
			}

			// proceed to remove from bottom iLevel
			boolean marked = false;
			pSucc = pNodeToRemove->_next[0].get(&marked);
			while (true) { // until someone succeeded in marking
				final boolean iMarkedIt = pNodeToRemove->_next[0].compareAndSet(pSucc, pSucc, false, true);
				pSucc = succs[0]->_next[0].get(&marked);
				if (iMarkedIt) {
					// run find to remove links of the logically removed node
					find(inValue, null, succs);
					return pNodeToRemove->_key;
				} else if (marked) 
					return _NULL_VALUE; // someone else removed node
				// else only pSucc changed so repeat
			}
		}
	}
}
*/

//------------------------------------------------------------------------------
// File    : LFSkipList.h
// Author  : Ms.Moran Tzafrir; email: morantza@gmail.com; phone: +972-505-779961
// Written : 13 April 2009
// 
// Lock Free Skip List
//
// Copyright (C) 2009 Moran Tzafrir.
// You can use this file only by explicit written approval from Moran Tzafrir.
//------------------------------------------------------------------------------

#endif
