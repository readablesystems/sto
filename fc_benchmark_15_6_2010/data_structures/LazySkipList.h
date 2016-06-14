#ifndef __LAZY_SKIPLIST_MULTI__
#define __LAZY_SKIPLIST_MULTI__

//------------------------------------------------------------------------------
// START
// File    : LazySkipListMulti.h
// Author  : Ms.Moran Tzafrir; email: morantza@gmail.com; phone: +972-505-779961
// Written : 13 April 2009
// 
// Lazy SkipLists
//
// Copyright (C) 2009 Moran Tzafrir.
// You can use this file only by explicit written approval from Moran Tzafrir.
//------------------------------------------------------------------------------

#include "../framework/cpp_framework.h"
using namespace CCP;

class LazySkipList : public ITest{ 
protected://static
	static final int _MAX_LEVEL  = 30;

protected://types

	class Node {
	public:
		final int					_key;
		final int					_top_level;
		TTASLock					_lock;
		VolatileType<Node*> * final	_next;
		VolatileType<bool>			_is_marked;
		VolatileType<bool>			_is_fully_linked;

	public:
		Node(final int key) 
		:	_key(key), 
			_next( new VolatileType<Node*>[_MAX_LEVEL + 1]),
			_top_level( _MAX_LEVEL ),
			_is_marked( false ),
			_is_fully_linked( true )
		{
			for (int i=0; i<_top_level; ++i)
				_next[i] = null;
		}

		Node(final int key, final int height) 
		:	_key(key), 
			_next( new VolatileType<Node*>[height + 1]),
			_top_level( height ),
			_is_marked( false ),
			_is_fully_linked( false )
		{
			for (int i=0; i<_top_level; ++i)
				_next[i] = null;
		}

	public:
		~Node() {delete [] _next;}
		void Lock() {
			_lock.lock();
		}
		void Unlock() {
			_lock.unlock();
		}
	public:
		bool isValid() { 
			return (_is_fully_linked && !(_is_marked));
		}
	};

	typedef Node*	HPtr;

protected://fields
	VolatileType<_u64>	_random_seed;
	Node* final			_head;
	Node* final			_tail;

protected://methods
	inline_ int randomLevel() {
		int x = (int)(_random_seed.get()  & 0xFFFFFFUL);
		x ^= x << 13;
		x ^= x >> 17;
		_random_seed.set( x ^= x << 5 );
		if ((x & 0x80000001) != 0) {
			return 1;
		}
		int level = 2;
		while (((x >>= 1) & 1) != 0) 
			++level;
		if(level > (_MAX_LEVEL-1))
			return (_MAX_LEVEL-1);
		else
			return level;
	}

	inline_ int find(final int key, HPtr* preds, HPtr* succs, HPtr* pFound) {
		while (true) {
			int level_found = -1;
			HPtr pPred, pCurr;
			pPred = _head;

			int isMarked = 0;
			for (int iLevel = _MAX_LEVEL-1; iLevel >= 0; --iLevel) {
				pCurr = pPred->_next[iLevel];
				isMarked = pPred->_is_marked;

				if(1==isMarked)
					break;

				while (key > pCurr->_key && 0==isMarked) {
					pPred = pCurr; 
					pCurr = pPred->_next[iLevel];
					isMarked = pPred->_is_marked;
				}
				if(1==isMarked)
					break;

				if (-1 == level_found && key == pCurr->_key) {
					level_found = iLevel;
					if(null != pFound) {
						*pFound = pCurr;
					}
				}

				if(null != preds)
					preds[iLevel] = pPred;
				if(null != succs)
					succs[iLevel] = pCurr;
			}
			if(0==isMarked)
				return level_found;
		}
	}

public:
	LazySkipList()
	:	_random_seed( Random::getSeed() | 0x010000 ),
		_head( new Node(INT_MIN) ),
		_tail( new Node(INT_MAX) ) 
	{
		for (int iLevel = 0; iLevel < _head->_top_level; ++iLevel)
			_head->_next[iLevel] = _tail;
	}

	~LazySkipList() {
		delete _head;
		delete _tail;
	}

	//general .....................................................
	boolean add(final int iThread, final int inValue) {
		HPtr preds[_MAX_LEVEL + 1];
		HPtr succs[_MAX_LEVEL + 1];

		while (true) {
			final int level_found = find(inValue, preds, succs, null);

			//if need to add new node
			int highest_locked = -1;
			HPtr pPred, pSucc;
			Node* prev_node = null;
			bool is_valid = true;
			final int top_level = randomLevel();
			for (int level = 0; is_valid && (level < top_level); ++level) {
				pPred = preds[level];
				pSucc = succs[level];
				if(prev_node != pPred) {
					prev_node = pPred;
					pPred->Lock();
				}
				highest_locked = level;
				is_valid = pPred->isValid() && pSucc->isValid() && (pPred->_next[level] == pSucc);
			}

			if (!is_valid) {
				prev_node=null;
				for (int level = 0; level <= highest_locked; ++level) {
					if(prev_node != preds[level]) {
						prev_node = preds[level];
						preds[level]->Unlock();
					}
				}
				continue;
			}
			Node* new_node = new Node(inValue, top_level);

			// first link succs
			for (int level = 0; level < top_level; ++level) {
				new_node->_next[level] = succs[level];
			}

			// then link next fields of preds
			for (int level = 0; level < top_level; ++level) {
				preds[level]->_next[level] = new_node;
			}
			new_node->_is_fully_linked = true;

			prev_node=null;
			for (int level = 0; level <= highest_locked; ++level) {
				if(prev_node != preds[level]) {
					prev_node = preds[level];
					preds[level]->Unlock();
				}
			}
			return true;
		}
	}

	int remove(final int iThread, final int inValue) {
		while (true) {
			HPtr remove_node = _head->_next[0];
			remove_node->Lock();

			if(_tail == remove_node) {
				remove_node->Unlock();
				return _NULL_VALUE;
			}

			if(!(remove_node->_is_fully_linked)) {
				remove_node->Unlock();
				continue;
			}

			if (remove_node->_is_marked) {
				remove_node->Unlock();
				continue;
			}

			_head->Lock();
			if(_head->_next[0] != remove_node){
				_head->Unlock();
				remove_node->Unlock();
				continue;
			}

			bool is_con=false;
			for (int level = 0; level < (remove_node->_top_level); ++level) {
				if(false == (remove_node->_next[level]->_is_fully_linked) ||  (remove_node->_next[level]->_is_marked)) {
					is_con=true;
					break;
				}
			}
			if(is_con) {
				_head->Unlock();
				remove_node->Unlock();
				continue;
			}

			remove_node->_is_marked = true;
			for (int level = (remove_node->_top_level-1); level >= 0; --level) {
				_head->_next[level] = remove_node->_next[level];
			}

			_head->Unlock();
			remove_node->Unlock();

			return remove_node->_key;
		}
	}

	int contain(final int iThread, final int inValue) {
		while (true) {
			HPtr remove_node;
			remove_node = _head->_next[0];

			if(_tail == remove_node )
				return _NULL_VALUE;

			if(!(remove_node->_is_fully_linked) || remove_node->_is_marked)
				continue;

			return remove_node->_key;
		}
	}

	//general .....................................................
	int size() {
		return 0;
	}

	final char* name() {
		return "LazySkipList";
	}
};

//------------------------------------------------------------------------------
// END
// File    : LazySkipList.h
// Author  : Ms.Moran Tzafrir; email: morantza@gmail.com; phone: +972-505-779961
// Written : 13 April 2009
// 
// Lazy SkipList
//
// Copyright (C) 2009 Moran Tzafrir.
// You can use this file only by explicit written approval from Moran Tzafrir.
//------------------------------------------------------------------------------

#endif

