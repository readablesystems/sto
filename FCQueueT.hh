#ifndef FCQUEUET_H 
#define FCQUEUET_H
//#define DEBUGQ

#include <deque>
#include <list>
#include <math.h>
#include "FlatCombining.hh"
#include "Transaction.hh"
#include "cpp_framework.hh"

using namespace CCP;

template <typename T>
class FCQueueT : public Shared {

private:

	static const int _MAX_THREADS	= 1024;
	static const int _NULL_VALUE	= 0;
	static const int _ENQ_VALUE     = (INT_MIN+0); // have we enqueued anything? 
	static const int _ABORT_VALUE   = (INT_MIN+1); // should abort!
	static const int _DEQ_VALUE	    = (INT_MIN+2); // remove the phantom items from the queue 
	static const int _POP_VALUE     = (INT_MIN+3); // pop() was called. Mark an item phantom (or abort)
	static const int _CLEANUP_VALUE = (INT_MIN+4); // cleanup() was called. erase phantom flags
	static const int _EMPTY_VALUE   = (INT_MIN+5); // check if queue is empty. 
	const int		_NUM_THREADS    = 20;

	//list inner types ------------------------------
	struct SlotInfo {
		int volatile		_req_ans;		//here 1 can post the request and wait for answer
		void* volatile		_req_list;		//here 1 can post the request and wait for answer
		int volatile		_tid;	        //which thread is making the request
		int volatile		_time_stamp;	//when 0 not connected
		SlotInfo* volatile	_next;			//when NULL not connected

		SlotInfo() {
			_req_ans	 = _NULL_VALUE;
            _req_list    = NULL;
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

    struct internal_elem {
        bool volatile is_phantom_;
        int volatile tid_;
        int volatile value_;

        internal_elem() : is_phantom_(0), tid_(-1), value_(0) {}

        void set_internal_elem(int val) volatile {
            //assert(!is_phantom_);
            value_ = val;
        }
        
        bool phantom() volatile { return is_phantom_; } 
        
        void mark_phantom(int tid) volatile { 
            //assert(tid_ = -1);
            tid_ = tid;
            is_phantom_ = true; 
        }
        void unmark_phantom(int) volatile { 
            //assert(tid = tid_); 
            is_phantom_ = false; 
            tid_ = -1;
        }
    };

	struct Node {
		Node* volatile	_next;
		internal_elem volatile	_values[256];

		static Node* get_new(const int in_num_values) {
			const size_t new_size = (sizeof(Node) + (in_num_values + 2 - 256) * sizeof(internal_elem));

			Node* const new_node = (Node*) calloc(1,new_size);
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
		internal_elem volatile* enq_value_ary;
		if(NULL == _new_node) 
			_new_node = Node::get_new(_NODE_SIZE);
		enq_value_ary = _new_node->_values;
		enq_value_ary->set_internal_elem(1);
		++enq_value_ary;

		// prepare for deq
		internal_elem volatile * deq_value_ary = _tail->_values;
		deq_value_ary += deq_value_ary->value_;

		int num_added = 0;
		for (int iTry=0; iTry<_NUM_REP; ++iTry) {
			Memory::read_barrier();

			int num_changes=0;
			SlotInfo* curr_slot = _tail_slot.get();
			while(NULL != curr_slot->_next) {
                int tid = curr_slot->_tid;
				const int curr_value = curr_slot->_req_ans;

                // PUSHES
                // we want to push a list
                // done when sets curr_list to NULL
                if (curr_slot->_req_list) {
                    /*
                    auto write_list = (std::list<int>*) curr_slot->_req_list;
                    while(!write_list->empty()) {
                        const int value = write_list->front();
                        ++num_changes;
                        enq_value_ary->set_internal_elem(value);
                        ++enq_value_ary;

                        ++num_added;
                        if(num_added >= _NODE_SIZE) {
                            Node* const new_node2 = Node::get_new(_NODE_SIZE+4);
                            memcpy((void*)(new_node2->_values), (void*)(_new_node->_values), (_NODE_SIZE+2)*sizeof(internal_elem) );
                            _new_node = new_node2; 
                            enq_value_ary = _new_node->_values;
                            enq_value_ary->set_internal_elem(1);
                            ++enq_value_ary;
                            enq_value_ary += _NODE_SIZE;
                            _NODE_SIZE += 4;
                        }
                        write_list->pop_front();
                    }
                    curr_slot->_req_list = NULL;
                    curr_slot->_time_stamp = _NULL_VALUE;*/
                }
                // we want to push a value
                // done when sets curr_value to NULL
				else if(curr_value > _NULL_VALUE) {
					++num_changes;
					enq_value_ary->set_internal_elem(curr_value);
					++enq_value_ary;
					curr_slot->_req_ans = _NULL_VALUE;
					curr_slot->_time_stamp = _NULL_VALUE;

					++num_added;
					if(num_added >= _NODE_SIZE) {
						Node* const new_node2 = Node::get_new(_NODE_SIZE+4);
						memcpy((void*)(new_node2->_values), (void*)(_new_node->_values), (_NODE_SIZE+2)*sizeof(internal_elem) );
						_new_node = new_node2; 
						enq_value_ary = _new_node->_values;
						enq_value_ary->set_internal_elem(1);
						++enq_value_ary;
						enq_value_ary += _NODE_SIZE;
						_NODE_SIZE += 4;
					}

                // ACTUAL DEQS
                // done when sets curr_value NULL 
                // we actually want to dequeue all the values that we marked as dirty!
				} else if(_DEQ_VALUE == curr_value) {
					++num_changes;
                    while(1) {
					    const int curr_deq = deq_value_ary->value_;
                        if(0 != curr_deq) {
                            if (deq_value_ary->phantom()) {
#ifdef DEBUGQ
                                std::cout << (void*)deq_value_ary << " deq " << tid << std::endl;
#endif
                                // we're actually going to pop this.
                                // must be ours!
                                //assert(deq_value_ary->tid_ == tid);
                                ++deq_value_ary;
                            } else {
                                // no one has popped this far yet. just return.
                                curr_slot->_req_ans = _NULL_VALUE;
					            curr_slot->_time_stamp = _NULL_VALUE;
                                break;
                            }
                        } else if(NULL != _tail->_next) {
                            _tail = _tail->_next;
                            deq_value_ary = _tail->_values;
                            deq_value_ary += deq_value_ary->value_;
                            continue;
                        } else {
                            curr_slot->_req_ans = _NULL_VALUE;
                            curr_slot->_time_stamp = _NULL_VALUE;
                            break;
                        }
					} 
                
                // POP WAS CALLED 
                // done when sets curr_value to some negative value (found) or NULL (empty)
                // we want to mark an item as dirty. this should make no modifications
                // to the queue itself
				} else if(_POP_VALUE == curr_value) {
					++num_changes;
					auto curr_deq_pos = deq_value_ary;
                    auto tmp_tail = _tail;
                    while(1) {
#ifdef DEBUGQ
                        std::cout << curr_deq_pos->value_ << " at " << (void*)curr_deq_pos << std::endl;
#endif
                        if(0 != curr_deq_pos->value_) {
                            // the queue is nonempty! 
                            if (curr_deq_pos->tid_ == tid && curr_deq_pos->phantom()) {
                                // keep going... we've already popped within this txn
                                curr_deq_pos++;
                            } else {
                                if (curr_deq_pos->phantom()) {
                                    // someone else is popping! abort
                                    curr_slot->_req_ans = _ABORT_VALUE;
                                } else {
                                    // we can actually mark this as ours to pop
#ifdef DEBUGQ
                                    std::cout << (void*)curr_deq_pos << " marked " << tid << std::endl;
#endif
                                    curr_deq_pos->mark_phantom(tid);
                                    curr_slot->_req_ans = -(curr_deq_pos->value_);
                                    curr_slot->_time_stamp = _NULL_VALUE;
                                }
                                break;
                            }
                        } else if(NULL != tmp_tail->_next) {
                            tmp_tail = tmp_tail->_next;
                            curr_deq_pos = tmp_tail->_values;
                            curr_deq_pos += curr_deq_pos->value_;
#ifdef DEBUGQ
                            std::cout << (void*) curr_deq_pos << " next " << tid << std::endl;
#endif
                            continue;
                        } else {
                            // empty queue! (or we've popped off everything)
                            // for now, let's not deal with RMW
                            curr_slot->_req_ans = _NULL_VALUE;
                            curr_slot->_time_stamp = _NULL_VALUE;
                            break;
                        }
                    }
               
                // CLEANUP WAS CALLED
                // done when sets curr_value to NULL
                // this is a cleanup call---unmark any values we marked as dirty
                // if we didn't mark any items at the front dirty, just return
				} else if(_CLEANUP_VALUE == curr_value) {
					++num_changes;
					auto curr_deq_pos = deq_value_ary;
                    auto tmp_tail = _tail;
                    while(1) {
                        if(0 != curr_deq_pos->value_) {
                            // we found an item to pop!
                            if (curr_deq_pos->phantom()) {
                                //assert(curr_deq_pos->tid_ == tid);
                                // we were going to pop this!
                                // keep going... we've already popped within this txn
                                curr_deq_pos->unmark_phantom(tid);
#ifdef DEBUGQ
                                std::cout << (void*)curr_deq_pos << " clean " << tid << std::endl;
#endif
                                curr_deq_pos++;
                            } else {
                                // we didn't pop at all
                                curr_slot->_req_ans = _NULL_VALUE;
                                curr_slot->_time_stamp = _NULL_VALUE;
                                break;
                            }
                        } else if(NULL != tmp_tail->_next) {
                            tmp_tail = tmp_tail->_next;
                            curr_deq_pos = tmp_tail->_values;
                            curr_deq_pos += curr_deq_pos->value_;
                            continue;
                        } else {
                            // we're at the end of the queue!
                            curr_slot->_req_ans = _NULL_VALUE;
                            curr_slot->_time_stamp = _NULL_VALUE;
                            break;
                        }
                    }
                
                // CHECKING IF EMPTY
                // Note we only call this if we did in fact see an empty queue!
				} else if(_EMPTY_VALUE == curr_value) {
					++num_changes;
					auto curr_deq_pos = deq_value_ary;
                    auto tmp_tail = _tail;
                    
                    if (enq_value_ary != (_new_node->_values + 1)) {
                        curr_slot->_req_ans = _ENQ_VALUE;
                        curr_slot->_time_stamp = _NULL_VALUE;
                        continue;
                    }
                    while(1) {
                        if(0 != curr_deq_pos->value_) {
                            if (curr_deq_pos->tid_ == tid && curr_deq_pos->phantom()) {
                                // keep going... we're going to pop this
                                curr_deq_pos++;
                            } else {
                                // it wasn't empty!!
                                curr_slot->_req_ans = -curr_deq_pos->value_;
                                curr_slot->_time_stamp = _NULL_VALUE;
                                break;
                            }
                        } else if(NULL != tmp_tail->_next) {
                            tmp_tail = tmp_tail->_next;
                            curr_deq_pos = tmp_tail->_values;
                            curr_deq_pos += curr_deq_pos->value_;
                            continue;
                        } else {
                            // empty! but check if we're going to enq anything... 
                            curr_slot->_req_ans = _NULL_VALUE;
                            curr_slot->_time_stamp = _NULL_VALUE;

                            // we can't allow anyone else to enq or the previous value
                            // is no longer valid
                            // install all enques/deques and return
                            if(0 == deq_value_ary->value_ && NULL != _tail->_next) {
                                _tail = _tail->_next;
                            } else {
                                // set where to start next in dequeing
                                _tail->_values->set_internal_elem(deq_value_ary -  _tail->_values);
                            }

                            if(enq_value_ary != (_new_node->_values + 1)) {
                                enq_value_ary->set_internal_elem(0);
                                _head->_next = _new_node;
                                _head = _new_node;
                                _new_node  = NULL;
                            } 
                            return;
                        }
                    }
				}
				curr_slot = curr_slot->_next;
			}//while on slots

			if(num_changes < _REP_THRESHOLD)
				break;
		}//for repetition

		if(0 == deq_value_ary->value_ && NULL != _tail->_next) {
			_tail = _tail->_next;
		} else {
            // set where to start next in dequeing
			_tail->_values->set_internal_elem(deq_value_ary -  _tail->_values);
		}

		if(enq_value_ary != (_new_node->_values + 1)) {
			enq_value_ary->set_internal_elem(0);
			_head->_next = _new_node;
			_head = _new_node;
			_new_node  = NULL;
		} 
	}


public:
    typedef T           value_type;     ///< Value type

private:
    // STO
    static constexpr TransItem::flags_type empty_q_bit = TransItem::user0_bit<<1;
    static constexpr TransItem::flags_type list_bit = TransItem::user0_bit<<2;
    static constexpr int pushitem_key = -1;
    static constexpr int popitem_key = -2;

public:
	FCQueueT() :	_NUM_REP(_NUM_THREADS), _REP_THRESHOLD((int)(ceil(_NUM_THREADS/(1.7))))
	{
		_head = Node::get_new(_NUM_THREADS);
		_tail = _head;
		_head->_values[0].set_internal_elem(1);
		_head->_values[1].set_internal_elem(0);

		_tail_slot.set(new SlotInfo());
		_timestamp = 0;
		_NODE_SIZE = 4;
		_new_node = NULL;
	}

	virtual ~FCQueueT() { }

    // TRANSACTIONAL CALLS
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

	bool pop() {
        bool popped = fc_pop(); 
        auto item = Sto::item(this, popitem_key);
        // we saw an empty queue in a previous pop, but it's no longer empty!
        if (saw_empty(item) && popped) {
            Sto::abort();
        }
        // things are still consistent... record that we saw an empty queue or that we popped
        if (!popped) {
            item.add_flags(empty_q_bit);
            item.add_read(0);
        // we actually need to deque something at commit time
        } else if (!item.has_write()) {
            item.add_write();
        }
        return popped;
	}

    bool fc_pop() {
		SlotInfo* my_slot = _tls_slot_info;
		if(NULL == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		int volatile& my_re_tid = my_slot->_tid;
		my_re_tid = TThread::id();
		my_re_ans = _POP_VALUE;

		do {
			if(NULL == my_next)
				enq_slot(my_slot);

			bool is_cas = true;
			if(lock_fc(_fc_lock, is_cas)) {
				flat_combining();
				_fc_lock.set(0);
                if (my_re_ans == _ABORT_VALUE)
                    Sto::abort();
                break;
			} else {
				Memory::write_barrier();
				if(!is_cas)
				while(_POP_VALUE == my_re_ans && 0 != _fc_lock.getNotSafe()) {
                    sched_yield();
				}
				Memory::read_barrier();
				if(_POP_VALUE != my_re_ans) {
                    if (my_re_ans == _ABORT_VALUE)
                        Sto::abort();
                    break;
				}
			}
		} while(true);
        return (-my_re_ans) != _NULL_VALUE;
    }

	bool fc_push_val(const int inValue) {
		SlotInfo* my_slot = _tls_slot_info;
		if(NULL == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		int volatile& my_re_tid = my_slot->_tid;
		my_re_tid = TThread::id();
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

	bool fc_push(std::list<int>& inValue) {
		SlotInfo* my_slot = _tls_slot_info;
		if(NULL == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
        void* volatile& my_re_list = my_slot->_req_list;
		int volatile& my_re_tid = my_slot->_tid;
		my_re_tid = TThread::id();
		my_re_list = (void*)&inValue;

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
				while(NULL != my_re_list && 0 != _fc_lock.getNotSafe()) {
                    sched_yield();
				} 
				Memory::read_barrier();
				if(NULL == my_re_list) {
					return true;
				}
			}
		} while(true);
	}

	void fc_cleanup() {
		SlotInfo* my_slot = _tls_slot_info;
		if(NULL == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		int volatile& my_re_tid = my_slot->_tid;
		my_re_tid = TThread::id();
		my_re_ans = _CLEANUP_VALUE;

		do {
			if (NULL == my_next)
				enq_slot(my_slot);

			bool is_cas = true;
			if(lock_fc(_fc_lock, is_cas)) {
				flat_combining();
				_fc_lock.set(0);
                break;
			} else {
				Memory::write_barrier();
				if(!is_cas)
				while(_NULL_VALUE != my_re_ans && 0 != _fc_lock.getNotSafe()) {
                    sched_yield();
				} 
				Memory::read_barrier();
				if(_NULL_VALUE == my_re_ans) {
					return;
				}
			}
		} while(true);
	}

    bool fc_empty() {
        SlotInfo* my_slot = _tls_slot_info;
		if(NULL == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		int volatile& my_re_tid = my_slot->_tid;
		my_re_tid = TThread::id();
		my_re_ans = _EMPTY_VALUE;

		do {
			if (NULL == my_next)
				enq_slot(my_slot);

			bool is_cas = true;
			if(lock_fc(_fc_lock, is_cas)) {
				flat_combining();
				_fc_lock.set(0);
                return (my_re_ans == _NULL_VALUE);
			} else {
				Memory::write_barrier();
				if(!is_cas)
				while(_NULL_VALUE != my_re_ans && 0 != _fc_lock.getNotSafe()) {
                    sched_yield();
				} 
				Memory::read_barrier();
                if (my_re_ans != _EMPTY_VALUE) {
                    return (my_re_ans == _NULL_VALUE);
                }
			}
		} while(true);
    }

    void fc_perform_deques() {
        SlotInfo* my_slot = _tls_slot_info;
		if(NULL == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		int volatile& my_re_tid = my_slot->_tid;
		my_re_tid = TThread::id();
		my_re_ans = _DEQ_VALUE;

		do {
			if (NULL == my_next)
				enq_slot(my_slot);

			bool is_cas = true;
			if(lock_fc(_fc_lock, is_cas)) {
				flat_combining();
				_fc_lock.set(0);
				return;
			} else {
				Memory::write_barrier();
				if(!is_cas)
				while(_NULL_VALUE != my_re_ans && 0 != _fc_lock.getNotSafe()) {
                    sched_yield();
				} 
				Memory::read_barrier();
				if(_NULL_VALUE == my_re_ans) {
					return;
				}
			}
		} while(true);
    }

private:
    bool saw_empty(const TransItem& item) {
        return item.flags() & empty_q_bit;
    }
 
    bool is_list(const TransItem& item) {
        return item.flags() & list_bit;
    }
 
    bool lock(TransItem&, Transaction&) override {
        return true;
    }

    bool check(TransItem& item, Transaction&) override {
        if (saw_empty(item)) {
            return fc_empty();
        }
        return true;
    }

    void install(TransItem& item, Transaction&) override {
        if (item.key<int>() == popitem_key) {
            // we popped something!
            fc_perform_deques();
            return;
        }
        // install pushes
        else if (item.key<int>() == pushitem_key) {
            // write all the elements
            if (is_list(item)) {
                auto& write_list = item.template write_value<std::list<T>>();
                while(!write_list.empty()) {
                    // XXX
                    fc_push_val(write_list.front());
                    write_list.pop_front();
                }
                //fc_push(write_list);
            } else {
                auto& val = item.template write_value<T>();
                fc_push_val(val);
            }
        }
    }
    
    void unlock(TransItem&) override {
        return;
    }

    void cleanup(TransItem& item, bool committed) override {
        (void)item;
        if (!committed) {
            fc_cleanup();
        }
    }
};

template <typename T>
thread_local typename FCQueueT<T>::SlotInfo* FCQueueT<T>::_tls_slot_info = NULL;

#endif // #ifndef FCQUEUET_H
