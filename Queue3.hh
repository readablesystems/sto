#pragma once

#include <list>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "TWrapped.hh"

/*
 * This queue uses pessimistic locking (similar to the scheme proposed in the TDSL paper
 * http://dl.acm.org/citation.cfm?doid=2908080.2908112).
 * 
 * Any modification or read of the head of the list (pop/front) locks the queue for the remainder
 * of the txn. This means that aborts only occur upon encountering empty queues.
 */


template <typename T, unsigned BUF_SIZE = 100000,
          template <typename> class W = TOpaqueWrapped>
class Queue3: public Shared {
public:
    typedef typename W<T>::version_type version_type;

    Queue3() : head_(0), tail_(0), queueversion_(0) {}

    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type read_writes = TransItem::user0_bit<<1;
    static constexpr TransItem::flags_type list_bit = TransItem::user0_bit<<2;
    static constexpr TransItem::flags_type empty_bit = TransItem::user0_bit<<3;

    void push(const T& v) {
        auto item = Sto::item(this, -1);
        if (item.has_write()) {
            if (!is_list(item)) {
                auto& val = item.template write_value<T>();
                std::list<T> write_list;
                if (!is_empty(item)) {
                    write_list.push_back(val);
                    item.clear_flags(empty_bit);
                }
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
        auto pushitem = Sto::item(this,-1);
        // lock the queue until done with txn
        if (!queueversion_.is_locked_here()) {
            if (!queueversion_.try_lock()) { 
                Sto::abort(); 
            }
        }
        // abort if the queue version number has changed since the beginning of this txn
        if (pushitem.has_read()) {
            if (!pushitem.item().check_version(queueversion_)) {
                queueversion_.unlock();
                Sto::abort();
            }
        }

        fence();
        auto index = head_;
        auto item = Sto::item(this, index);

        while (1) {
           if (index == tail_) {
               auto v = queueversion_;
               fence();
                // empty queue, so we have to add a read of the queueversion
                if (index == tail_) {
                    if (!pushitem.has_read())
                        pushitem.observe(v);
                    if (pushitem.has_write()) {
                        if (is_list(pushitem)) {
                            auto& write_list = pushitem.template write_value<std::list<T>>();
                            // if there is an element to be pushed on the queue, return addr of queue element
                            if (!write_list.empty()) {
                                write_list.pop_front();
                                item.add_flags(read_writes);
                                return true;
                            }
                            else return false;
                        }
                        // not a list, has exactly one element
                        else if (!is_empty(pushitem)) {
                            pushitem.add_flags(empty_bit);
                            return true;
                        }
                        else return false;
                    }
                    // fail if trying to read from an empty queue
                    else return false;  
                } 
            }
            if (has_delete(item)) {
                index = (index + 1) % BUF_SIZE;
                item = Sto::item(this, index);
            }
            else break;
        }
        item.add_flags(delete_bit);
        item.add_write(0);
        return true;
    }

    bool front(T& val) {
        auto pushitem = Sto::item(this,-1);
        // lock the queue until done with txn
        if (!queueversion_.is_locked_here()) {
            if (!queueversion_.try_lock()) { 
                Sto::abort(); 
            }
        }
        // abort if the queue version number has changed since the beginning of this txn
        if (pushitem.has_read()) {
            if (!pushitem.item().check_version(queueversion_)) {
                queueversion_.unlock();
                Sto::abort();
            }
        }

        fence();
        unsigned index = head_;
        auto item = Sto::item(this, index);
        while (1) {
            // empty queue, so we have to add a read of the queueversion
            if (index == tail_) {
                auto v = queueversion_;
                fence();
                if (index == tail_) {
                    if (!pushitem.has_read())
                        pushitem.observe(v);
                    if (pushitem.has_write()) {
                        if (is_list(pushitem)) {
                            auto& write_list= pushitem.template write_value<std::list<T>>();
                            // if there is an element to be pushed on the queue, return addr of queue element
                            if (!write_list.empty()) {
                                val = write_list.front();
                                return true;
                            }
                            else return false;
                        }
                        // not a list, has exactly one element
                        else if (!is_empty(pushitem)) {
                            auto& v = pushitem.template write_value<T>();
                            val = v;
                            return true;
                        }
                        else return false;
                    }
                    return false;
                }
            }
            if (has_delete(item)) {
                index = (index + 1) % BUF_SIZE;
                item = Sto::item(this, index);
            }
            else break;
        }
        val = queueSlots[index];
        return true;
    }

    bool singleton_push(T& val) {
        // lock the queue until done with operation
        if (!queueversion_.is_locked_here()) {
            if (!queueversion_.try_lock()) { 
                Sto::abort(); 
            }
        }
        assert(tail_ != (head_-1) % BUF_SIZE);
        queueSlots[tail_] = val;
        tail_ = (tail_+1) % BUF_SIZE;

        queueversion_.set_version(Sto::commit_tid());
        queueversion_.unlock();
        return true;
    }

    bool singleton_pop() {
        // lock the queue until done with operation
        // note that we don't need to check queueversion_ 
        // because this is the only operation in the txn
        if (!queueversion_.is_locked_here()) {
            if (!queueversion_.try_lock()) { 
                Sto::abort(); 
            }
        }
        if (head_ == tail_) return false;
        else {
            head_ = (head_+1) % BUF_SIZE;
            queueversion_.unlock();
            return true;
        }
    }

    bool singleton_front(T& val) {
        // lock the queue until done with operation
        // note that we don't need to check queueversion_ 
        // because this is the only operation in the txn
        if (!queueversion_.is_locked_here()) {
            if (!queueversion_.try_lock()) { 
                Sto::abort(); 
            }
        }
        while (1) {
            if (head_ == tail_) return false;
            else {
                val = queueSlots[index];
                queueversion_.unlock();
                return true;
            }
        }
    }

private:
    bool has_delete(const TransItem& item) {
        return item.flags() & delete_bit;
    }
    bool is_rw(const TransItem& item) {
        return item.flags() & read_writes;
    }
 
    bool is_list(const TransItem& item) {
        return item.flags() & list_bit;
    }
 
    bool is_empty(const TransItem& item) {
        return item.flags() & empty_bit;
    }

    bool lock(TransItem& item, Transaction& txn) override {
        if ((item.key<int>() == -1) && !queueversion_.is_locked_here())  {
            return txn.try_lock(item, queueversion_);
        }
        return true;
    }

    bool check(TransItem& item, Transaction& t) override {
        (void) t;
        // check if we read off the write_list. We should only abort if both: 
        //      1) we saw the queue was empty during a pop/front and read off our own write list 
        //      2) someone else has pushed onto the queue before we got here
        if (item.key<int>() == -1)
            return item.check_version(queueversion_);
        // shouldn't reach this
        assert(0);
        return false;
    }

    void install(TransItem& item, Transaction& txn) override {
        // install pops
        if (has_delete(item)) {
            // queueversion_ should be locked here because we must have popped
            assert(queueversion_.is_locked_here());
            // only increment head if item popped from actual q
            if (!is_rw(item)) {
                head_ = (head_+1) % BUF_SIZE;
            }
        }
        // install pushes
        else if (item.key<int>() == -1) {
            assert(queueversion_.is_locked_here());
            auto head_index = head_;
            // write all the elements
            if (is_list(item)) {
                auto& write_list = item.template write_value<std::list<T>>();
                while (!write_list.empty()) {
                    // assert queue is not out of space            
                    assert(tail_ != (head_index-1) % BUF_SIZE);
                    queueSlots[tail_] = write_list.front();
                    write_list.pop_front();
                    tail_ = (tail_+1) % BUF_SIZE;
                }
            }
            else if (!is_empty(item)) {
                assert(tail_ != (head_index-1) % BUF_SIZE);
                auto& val = item.template write_value<T>();
                queueSlots[tail_] = val;
                tail_ = (tail_+1) % BUF_SIZE;
            }
            // set queueversion appropriately
            queueversion_.set_version(txn.commit_tid());
        }
    }
    
    void unlock(TransItem& item) override {
        (void)item;
        if (queueversion_.is_locked_here()) {
            queueversion_.unlock();
        }
    }

    void cleanup(TransItem& item, bool committed) override {
        (void)item;
        (void)committed;
        if (queueversion_.is_locked_here()) {
            queueversion_.unlock();
        }
    }

    T queueSlots[BUF_SIZE];

    unsigned head_;
    unsigned tail_;
    version_type queueversion_;
};
