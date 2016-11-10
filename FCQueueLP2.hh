/*
    This file is a part of libcds - Concurrent Data Structures library

    (C) Copyright Maxim Khizhinsky (libcds.dev@gmail.com) 2006-2016

    Source code repo: http://github.com/khizmax/libcds/
    Download: http://sourceforge.net/projects/libcds/files/
    
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.     
*/

#ifndef FCQUEUELP2_H 
#define FCQUEUELP2_H

#include <deque>
#include <list>
#include <math.h>
#include "FlatCombining.hh"
#include "Transaction.hh"
#include "cpp_framework.hh"
#include "LazyPop.hh"

using namespace CCP;

template <typename T>
class FCQueueLP2 : public Shared {

private:

	static const int _MAX_THREADS	= 1024;
	static const int _NULL_VALUE	= 0;
	static const int _DEQ_VALUE		= (INT_MIN+2);
	static const int _OK_DEQ		= (INT_MIN+3);
	const int		_NUM_THREADS    = 8;

	//list inner types ------------------------------
	struct SlotInfo {
		int volatile		_req_ans;		//here 1 can post the request and wait for answer
		int volatile		_time_stamp;	//when 0 not connected
		SlotInfo* volatile	_next;			//when NULL not connected

		SlotInfo() {
			_req_ans	 = _NULL_VALUE;
			_time_stamp  = 0;
			_next		 = NULL;
		}
	};

	//list fields -----------------------------------
	static thread_local SlotInfo*   _tls_slot_info;
    AtomicReference<SlotInfo>       _tail_slot;
	int volatile					_timestamp;

	//list helper function --------------------------
	SlotInfo* get_new_slot() {
		SlotInfo* my_slot= new SlotInfo();
		_tls_slot_info = my_slot;

		SlotInfo* curr_tail;
		do {
			curr_tail = _tail_slot.get();
			my_slot->_next = curr_tail;
		} while(false == _tail_slot.compareAndSet(curr_tail, my_slot));

		return my_slot;
	}

	void enq_slot(SlotInfo* p_slot) {
		SlotInfo* curr_tail;
		do {
			curr_tail = _tail_slot.get();
			p_slot->_next = curr_tail;
		} while(false == _tail_slot.compareAndSet(curr_tail, p_slot));
	}

	void enq_slot_if_needed(SlotInfo* p_slot) {
		if(NULL == p_slot->_next) {
			enq_slot(p_slot);
		}
	}

	struct Node {
		Node* volatile	_next;
		int	volatile	_values[256];

		static Node* get_new(const int in_num_values) {
			const size_t new_size = (sizeof(Node) + (in_num_values + 2 - 256) * sizeof(int));

			Node* const new_node = (Node*) malloc(new_size);
			new_node->_next = NULL;
			return new_node;
		}
	};

    AtomicInteger   _fc_lock;
	char			_pad1[CACHE_LINE_SIZE];
	const int		_NUM_REP;
	const int		_REP_THRESHOLD;
	Node* volatile	_head;
	Node* volatile	_tail;
	int volatile	_NODE_SIZE;
	Node* volatile	_new_node;

	inline void flat_combining() {
		// prepare for enq
		int volatile* enq_value_ary;
		if(NULL == _new_node) 
			_new_node = Node::get_new(_NODE_SIZE);
		enq_value_ary = _new_node->_values;
		*enq_value_ary = 1;
		++enq_value_ary;

		// prepare for deq
		int volatile * deq_value_ary = _tail->_values;
		deq_value_ary += deq_value_ary[0];

		int num_added = 0;
		for (int iTry=0; iTry<_NUM_REP; ++iTry) {
			Memory::read_barrier();

			int num_changes=0;
			SlotInfo* curr_slot = _tail_slot.get();
			while(NULL != curr_slot->_next) {
				const int curr_value = curr_slot->_req_ans;
				if(curr_value > _NULL_VALUE) {
					++num_changes;
					*enq_value_ary = curr_value;
					++enq_value_ary;
					curr_slot->_req_ans = _NULL_VALUE;
					curr_slot->_time_stamp = _NULL_VALUE;

					++num_added;
					if(num_added >= _NODE_SIZE) {
						Node* const new_node2 = Node::get_new(_NODE_SIZE+4);
						memcpy((void*)(new_node2->_values), (void*)(_new_node->_values), (_NODE_SIZE+2)*sizeof(int) );
						//free(_new_node);
						_new_node = new_node2; 
						enq_value_ary = _new_node->_values;
						*enq_value_ary = 1;
						++enq_value_ary;
						enq_value_ary += _NODE_SIZE;
						_NODE_SIZE += 4;
					}
				} else if(_DEQ_VALUE == curr_value) {
					++num_changes;
					const int curr_deq = *deq_value_ary;
					if(0 != curr_deq) {
						curr_slot->_req_ans = -curr_deq;
						curr_slot->_time_stamp = _NULL_VALUE;
						++deq_value_ary;
					} else if(NULL != _tail->_next) {
						_tail = _tail->_next;
						deq_value_ary = _tail->_values;
						deq_value_ary += deq_value_ary[0];
						continue;
					} else {
						curr_slot->_req_ans = _NULL_VALUE;
						curr_slot->_time_stamp = _NULL_VALUE;
					} 
				}
				curr_slot = curr_slot->_next;
			}//while on slots

			if(num_changes < _REP_THRESHOLD)
				break;
		}//for repetition

		if(0 == *deq_value_ary && NULL != _tail->_next) {
			_tail = _tail->_next;
		} else {
			_tail->_values[0] = (deq_value_ary -  _tail->_values);
		}

		if(enq_value_ary != (_new_node->_values + 1)) {
			*enq_value_ary = 0;
			_head->_next = _new_node;
			_head = _new_node;
			_new_node  = NULL;
		} 
	}


public:
    typedef T           value_type;     ///< Value type
    typedef LazyPop<T, FCQueueLP2<T>> pop_type;

private:
    // STO
    static constexpr TransItem::flags_type list_bit = TransItem::user0_bit<<2;
    static constexpr TransItem::flags_type pop_bit = TransItem::user0_bit<<3;
    static constexpr int pushitem_key = -1;
    static constexpr int popitem_key = -2;

public:
	FCQueueLP2() 
	:	_NUM_REP(_NUM_THREADS),
		_REP_THRESHOLD((int)(ceil(_NUM_THREADS/(1.7))))
	{
		_head = Node::get_new(_NUM_THREADS);
		_tail = _head;
		_head->_values[0] = 1;
		_head->_values[1] = 0;

		_tail_slot.set(new SlotInfo());
		_timestamp = 0;
		_NODE_SIZE = 4;
		_new_node = NULL;
	}

	virtual ~FCQueueLP2() { }

    void push(const T& v) {
        auto item = Sto::item(this, pushitem_key);
        if (item.has_write()) {
            if (!is_list(item)) {
                auto& val = item.template write_value<T>();
                std::list<T> write_list;
                write_list.push_back(val);
                write_list.push_back(v);
                item.clear_write();
                item.add_write(write_list);
                item.add_flags(list_bit);
            }
            else {
                auto& write_list = item.template write_value<std::list<T>>();
                write_list.push_back(v);
            }
        }
        else item.add_write(v);
    }

    pop_type& pop() {
        pop_type* my_lazypop = &global_thread_lazypops[TThread::id()];
        auto item = Sto::item(this, my_lazypop);
        item.add_flags(pop_bit);
        item.add_write();
        Sto::item(this, popitem_key).add_write();
        return *my_lazypop;
    }

	bool fc_push(const int inValue) {

		SlotInfo* my_slot = _tls_slot_info;
		if(NULL == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = inValue;

		do {
			if (NULL == my_next)
				enq_slot(my_slot);

			bool is_cas = true;
			if(lock_fc(_fc_lock, is_cas)) {
				flat_combining();
				_fc_lock.set(0);
				return true;
			} else {
				Memory::write_barrier();
				if(!is_cas)
				while(_NULL_VALUE != my_re_ans && 0 != _fc_lock.getNotSafe()) {
                    sched_yield();
				} 
				Memory::read_barrier();
				if(_NULL_VALUE == my_re_ans) {
					return true;
				}
			}
		} while(true);
	}

	bool fc_pop() {
		SlotInfo* my_slot = _tls_slot_info;
		if(NULL == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = _DEQ_VALUE;

		do {
			if(NULL == my_next)
				enq_slot(my_slot);

			bool is_cas = true;
			if(lock_fc(_fc_lock, is_cas)) {
				flat_combining();
				_fc_lock.set(0);
				return ((-my_re_ans) != _NULL_VALUE); 
			} else {
				Memory::write_barrier();
				if(!is_cas)
				while(_DEQ_VALUE == my_re_ans && 0 != _fc_lock.getNotSafe()) {
                    sched_yield();
				}
				Memory::read_barrier();
				if(_DEQ_VALUE != my_re_ans) {
				    return ((-my_re_ans) != _NULL_VALUE); 
				}
			}
		} while(true);
	}


private:
    bool is_pop(const TransItem& item) {
        return item.flags() & pop_bit;
    }
 
    bool is_list(const TransItem& item) {
        return item.flags() & list_bit;
    }
 
    bool lock(TransItem&, Transaction&) override {
        return true;
    }

    bool check(TransItem&, Transaction&) override {
        return true;
    }

    void install(TransItem& item, Transaction& txn) override {
        (void)txn;
        // install pops
        if (is_pop(item)) {
            auto lazypop = item.key<pop_type*>();
            lazypop->set_fulfilled();
            lazypop->set_popped(fc_pop());
        }
        // install pushes
        else if (item.key<int>() == pushitem_key) {
            // write all the elements
            if (is_list(item)) {
                auto& write_list = item.template write_value<std::list<T>>();
                while (!write_list.empty()) {
                    fc_push(write_list.front());
                    write_list.pop_front();
                }
            } else {
                auto& val = item.template write_value<T>();
                fc_push(val);
            }
        }
    }
    
    void unlock(TransItem& item) override {
        (void)item;
        return;
    }


    void cleanup(TransItem& item, bool committed) override {
        (void)committed;
        (void)item;
        (global_thread_lazypops + TThread::id())->~pop_type();
    }

    pop_type global_thread_lazypops[24];
};

template <typename T>
thread_local typename FCQueueLP2<T>::SlotInfo* FCQueueLP2<T>::_tls_slot_info = NULL;

#endif // #ifndef FCQUEUELP2_H
