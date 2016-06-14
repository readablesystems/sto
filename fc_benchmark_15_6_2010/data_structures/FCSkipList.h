#ifndef __FC_SKIP_LIST__
#define __FC_SKIP_LIST__

////////////////////////////////////////////////////////////////////////////////
// File    : FCSkipList.h
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

class FCSkipList : public ITest { 
protected://consts
	static final int _MAX_LEVEL	= 20;

protected://types

	class Node {
	public:
		int		_key;
		int		_top_level;
		int		_counter;
		Node* 	_next[_MAX_LEVEL+1];

	public:

		static Node* getNewNode(final int inKey) {
			Node* final new_node = (Node*) malloc(sizeof(Node));
			new_node->_key			= inKey;
			new_node->_top_level	= _MAX_LEVEL;
			new_node->_counter		= 1;
			return new_node;
		}

		static Node* getNewNode(final int inKey, final int height) {
			Node* final new_node = (Node*) malloc(sizeof(Node) + height + 1 - _MAX_LEVEL);
			new_node->_key			= inKey;
			new_node->_top_level	= height;
			new_node->_counter		= 1;
			return new_node;
		}
	};

protected://SkipList fields
	VolatileType<_u64>	_random_seed;
	Node*	final		_head;
	Node* final			_tail;

protected://Flat Combining fields

	AtomicInteger	_fc_lock;
	char							_pad1[CACHE_LINE_SIZE];
	final int		_NUM_REP;
	final int		_REP_THRESHOLD;
	Node*			preds[_MAX_LEVEL + 1];
	Node*			succs[_MAX_LEVEL + 1];
	//SlotInfo*		_saved_remove_node[1024];

protected://methods
	inline_ int randomLevel() {
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

	inline_ Node* find(final int key) {
		Node* pPred;
		Node* pCurr;
		pPred = _head;
		Node* found_node = null;

		for (int iLevel = _MAX_LEVEL-1; iLevel >= 0; --iLevel) {

			pCurr = pPred->_next[iLevel];

			while (key > pCurr->_key) {
				pPred = pCurr; 
				pCurr = pPred->_next[iLevel];
			}

			if (null == found_node && key == pCurr->_key) {
				found_node = pCurr;
			}

			preds[iLevel] = pPred;
			succs[iLevel] = pCurr;
		}

		return found_node;
	}


	inline_ void flat_combining() {

		int num_removed = 0;
		final int top_level = randomLevel();
		for (int iTry=0;iTry<_NUM_REP; ++iTry) {

			int num_changes=0;
			SlotInfo* curr_slot = _tail_slot.get();
			Memory::read_barrier();

			while(null != curr_slot->_next) {

				final int inValue = curr_slot->_req_ans;
				if(inValue > _NULL_VALUE) {
					++num_changes;
					curr_slot->_req_ans = _NULL_VALUE;
					curr_slot->_time_stamp = _NULL_VALUE;

					//ADD ......................................................
					Node* node_found = find(inValue);
					if (null != node_found) {
						++(node_found->_counter);
						curr_slot = curr_slot->_next;
						continue;
					}

					// first link succs ........................................
					// then link next fields of preds ..........................
					Node* new_node = Node::getNewNode(inValue, top_level);
					Node** new_node_next = new_node->_next;
					Node** curr_succ = succs;
					Node** curr_preds = preds;

					for (int level = 0; level < top_level; ++level) {
						*new_node_next = *curr_succ;
						(*curr_preds)->_next[level] = new_node;
						++new_node_next;
						++curr_succ;
						++curr_preds;
					}

					//..........................................................
					curr_slot = curr_slot->_next;
					continue;

				} else if(_DEQ_VALUE == inValue) {
					++num_changes;

					//REMOVE ...................................................
					//_saved_remove_node[num_removed] = curr_slot;
					curr_slot->_req_ans = -5;
					curr_slot->_time_stamp = _NULL_VALUE;

					++num_removed;
					curr_slot = curr_slot->_next;
					continue;
				} else {
					curr_slot = curr_slot->_next;
					continue;
				}

			} //while on slots

			//if(num_changes < _REP_THRESHOLD)
			//	break;
		}

		//..................................................................
		Node* remove_node = (_head->_next[0]);
		int max_level = -1;
		for (int iRemove=0; iRemove<num_removed; ++iRemove) {

			if ( _tail != remove_node ) {
				--(remove_node->_counter);
				if(0 == remove_node->_counter) {
					if(remove_node->_top_level > max_level) {
						max_level = remove_node->_top_level;
					}
					remove_node = remove_node->_next[0];
				} 
			}
		}

		if(-1 != max_level) {
			Node* pred = _head;
			Node* curr;

			for (int iLevel = (max_level-1); iLevel >= 0; ) {
				curr = pred->_next[iLevel];

				if(0 != curr->_counter) {
					_head->_next[iLevel] = curr;
					--iLevel;
				} else {
					pred = curr; 
					curr = pred->_next[iLevel];
				}
			}
		}

	}

public://methods

	FCSkipList()
	: _head( Node::getNewNode(INT_MIN) ),
	  _tail( Node::getNewNode(INT_MAX) ),
	  _NUM_REP( Math::Min(2, _NUM_THREADS)),
	  _REP_THRESHOLD((int)(Math::ceil(_NUM_THREADS/(1.7))))
	{
		//initialize head to point to tail .....................................
		for (int iLevel = 0; iLevel < _head->_top_level; ++iLevel)
			_head->_next[iLevel] = _tail;

		//initialize the slots .........................................
		_tail_slot.set(new SlotInfo());
	}

	~FCSkipList() {	}

public://methods

	//deq ......................................................
	boolean add(final int iThread, final int inValue) {
		SlotInfo* my_slot = _tls_slot_info;
		if(null == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = inValue;

		do {
			if (null == my_next)
				enq_slot(my_slot);

			boolean is_cas = false;
			if(lock_fc(_fc_lock, is_cas)) {
				machine_start_fc(iThread);
				flat_combining();
				_fc_lock.set(0);
				machine_end_fc(iThread);
				return true;
			} else {
				Memory::write_barrier();
				while(_NULL_VALUE != my_re_ans && 0 != _fc_lock.getNotSafe()) {
					thread_wait(iThread);
				} 
				Memory::read_barrier();
				if(_NULL_VALUE == my_re_ans) {
					return true;
				}
			}
		} while(true);
	}

	//deq ......................................................
	int remove(final int iThread, final int inValue) {
		SlotInfo* my_slot = _tls_slot_info;
		if(null == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = _DEQ_VALUE;

		do {
			if(null == my_next)
				enq_slot(my_slot);

			boolean is_cas = false;
			if(lock_fc(_fc_lock, is_cas)) {
				machine_start_fc(iThread);
				flat_combining();
				_fc_lock.set(0);
				machine_end_fc(iThread);
				return -(my_re_ans);
			} else {
				Memory::write_barrier();
				while(_DEQ_VALUE == my_re_ans && 0 != _fc_lock.getNotSafe()) {
					thread_wait(iThread);
				}
				Memory::read_barrier();
				if(_DEQ_VALUE != my_re_ans) {
					return -(my_re_ans);
				}
			}
		} while(true);
	}

	//peek ......................................................................
	int contain(final int iThread, final int inValue) {
		return _NULL_VALUE;
	}

public://methods

	int size() {
		return 0;
	}

	final char* name() {
		return "FCSkipList";
	}

};

#endif
