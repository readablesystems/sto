#pragma once

#include <list>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "TWrapped.hh"

template <typename T, unsigned BUF_SIZE = 1000000,
          template <typename> class W = TOpaqueWrapped>
class Queue: public TObject {
public:
    typedef typename W<T>::version_type version_type;

    Queue() : head_(0), tail_(0), tailversion_(0), headversion_(0) {}

    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type read_writes = TransItem::user0_bit<<1;
    static constexpr TransItem::flags_type list_bit = TransItem::user0_bit<<2;
    static constexpr TransItem::flags_type empty_bit = TransItem::user0_bit<<3;

    // NONTRANSACTIONAL PUSH/POP/EMPTY
    void nontrans_push(T v) {
        queueSlots[tail_] = v;
        tail_ = (tail_+1)%BUF_SIZE;
        assert(head_ != tail_);
    }
    
    T nontrans_pop() {
        assert(head_ != tail_);
        T v = queueSlots[head_];
        head_ = (head_+1)%BUF_SIZE;
        return v;
    }

    bool nontrans_empty() const {
        return head_ == tail_;
    }

    template <typename RandomGen>
    void nontrans_shuffle(RandomGen gen) {
        auto head = &queueSlots[head_];
        auto tail = &queueSlots[tail_];
        // don't support wrap-around shuffle
        assert(head < tail);
        std::shuffle(head, tail, gen);
    }

    void nontrans_clear() {
        while (!nontrans_empty())
            nontrans_pop();
    }

    // TRANSACTIONAL CALLS
    void transPush(const T& v) {
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

    bool transPop() {
        auto hv = headversion_;
        fence();
        auto index = head_;
        auto item = Sto::item(this, index);

        while (1) {
           if (index == tail_) {
               auto tv = tailversion_;
               fence();
                // if someone has pushed onto tail, can successfully do a front read, so don't read our own writes
                if (index == tail_) {
                    auto pushitem = Sto::item(this,-1);
                    if (!pushitem.has_read())
                        pushitem.observe(tv);
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
        // ensure that head is not modified by time of commit 
        auto lockitem = Sto::item(this, -2);
        if (!lockitem.has_read()) {
            lockitem.observe(hv);
        }
        lockitem.add_write(0);
        item.add_flags(delete_bit);
        item.add_write(0);
        return true;
    }

    bool transFront(T& val) {
        auto hv = headversion_;
        fence();
        unsigned index = head_;
        auto item = Sto::item(this, index);
        while (1) {
            // empty queue
            if (index == tail_) {
                auto tv = tailversion_;
                fence();
                // if someone has pushed onto tail, can successfully do a front read, so skip reading from our pushes 
                if (index == tail_) {
                    auto pushitem = Sto::item(this,-1);
                    if (!pushitem.has_read())
                        pushitem.observe(tv);
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
        // ensure that head was not modified at time of commit
        auto lockitem = Sto::item(this, -2);
        if (!lockitem.has_read()) {
            lockitem.observe(hv);
        }  
        val = queueSlots[index];
        return true;
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
        if (item.key<int>() == -1)
            return txn.try_lock(item, tailversion_);
        else if (item.key<int>() == -2)
            return txn.try_lock(item, headversion_);
        else
            return true;
    }

    bool check(TransItem& item, Transaction& t) override {
        (void) t;
        // check if was a pop or front 
        if (item.key<int>() == -2)
            return item.check_version(headversion_);
        // check if we read off the write_list (and locked tailversion)
        else if (item.key<int>() == -1)
            return item.check_version(tailversion_);
        // shouldn't reach this
        assert(0);
        return false;
    }

    void install(TransItem& item, Transaction& txn) override {
	    // ignore lock_headversion marker item
        if (item.key<int>() == -2)
            return;
        // install pops
        if (has_delete(item)) {
            // only increment head if item popped from actual q
            if (!is_rw(item))
                head_ = (head_+1) % BUF_SIZE;
            headversion_.set_version(txn.commit_tid());
        }
        // install pushes
        else if (item.key<int>() == -1) {
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
                auto& val = item.template write_value<T>();
                queueSlots[tail_] = val;
                tail_ = (tail_+1) % BUF_SIZE;
            }

            tailversion_.set_version(txn.commit_tid());
        }
    }
    
    void unlock(TransItem& item) override {
        if (item.key<int>() == -1)
            tailversion_.unlock();
        else if (item.key<int>() == -2)
            headversion_.unlock();
    }

    T queueSlots[BUF_SIZE];

    unsigned head_;
    unsigned tail_;
    version_type tailversion_;
    version_type headversion_;
};
