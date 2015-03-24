#pragma once

#include <list>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "VersionFunctions.hh"

template <typename T, unsigned BUF_SIZE = 1000000> 
class Queue: public Shared {
public:
    Queue() : head_(0), tail_(0), tailversion_(0), headversion_(0) {}

    typedef uint32_t Version;
    typedef VersionFunctions<Version> QueueVersioning;
    
    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type read_writes = TransItem::user0_bit<<1;
    static constexpr TransItem::flags_type list_bit = TransItem::user0_bit<<2;
    static constexpr TransItem::flags_type empty_bit = TransItem::user0_bit<<3;

    // NONTRANSACTIONAL PUSH/POP/EMPTY
    void push(T v) {
        queueSlots[tail_] = v;
        tail_ = (tail_+1)%BUF_SIZE;
        assert(head_ != tail_);
    }
    
    T pop() {
        assert(head_ != tail_);
        T v = queueSlots[head_];
        head_ = (head_+1)%BUF_SIZE;
        return v;
    }

    bool empty() {
        return (head_ == tail_);
    }

  template <typename RandomGen>
  void shuffle(RandomGen gen) {
    auto head = &queueSlots[head_];
    auto tail = &queueSlots[tail_];
    // don't support wrap-around shuffle
    assert(head < tail);
    std::shuffle(head, tail, gen);
  }

    // TRANSACTIONAL CALLS
    void transPush(Transaction& t, const T& v) {
        auto item = t.item(this, -1);
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

    bool transPop(Transaction& t) {
        auto hv = headversion_;
        fence();
        auto index = head_;
        auto item = t.item(this, index);

        while (1) {
           if (index == tail_) {
               Version tv = tailversion_;
               fence();
                // if someone has pushed onto tail, can successfully do a front read, so don't read our own writes
                if (index == tail_) {
                    auto pushitem = t.item(this,-1);
                    if (!pushitem.has_read())
                        pushitem.add_read(tv);
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
                item = t.item(this, index);
            }
            else break;
        }
        // ensure that head is not modified by time of commit 
        auto lockitem = t.item(this, -2);
        if (!lockitem.has_read()) {
            lockitem.add_read(hv);
        }
        lockitem.add_write(0);
        item.add_flags(delete_bit);
        item.add_write(0);
        return true;
    }

    bool transFront(Transaction& t, T& val) {
        auto hv = headversion_;
        fence();
        unsigned index = head_;
        auto item = t.item(this, index);
        while (1) {
            // empty queue
            if (index == tail_) {
                Version tv = tailversion_;
                fence();
                // if someone has pushed onto tail, can successfully do a front read, so skip reading from our pushes 
                if (index == tail_) {
                    auto pushitem = t.item(this,-1);
                    if (!pushitem.has_read())
                        pushitem.add_read(tv);
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
                item = t.item(this, index);
            }
            else break;
        }
        // ensure that head was not modified at time of commit
        auto lockitem = t.item(this, -2);
        if (!lockitem.has_read()) {
            lockitem.add_read(hv);
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

    void lock(Version& v) {
        QueueVersioning::lock(v);
    }

    void unlock(Version& v) {
        QueueVersioning::unlock(v);
    }
     
    void lock(TransItem& item) {
        if (item.key<int>() == -1)
            lock(tailversion_);
        else if (item.key<int>() == -2)
            lock(headversion_);
    }

    void unlock(TransItem& item) {
        if (item.key<int>() == -2) {
            unlock(headversion_);
        }
        else if (item.has_write() && item.key<int>() == -1)
            unlock(tailversion_);
    }
  
    bool check(const TransItem& item, const Transaction& t) {
        (void) t;
        // check if was a pop or front 
        if (item.key<int>() == -2) {
            auto hv = headversion_;
            return QueueVersioning::versionCheck(hv, item.template read_value<Version>()) && (!QueueVersioning::is_locked(hv) || item.has_write());
        }

        // check if we read off the write_list (and locked tailversion)
        else if (item.key<int>() == -1) {
            auto tv = tailversion_;
            return QueueVersioning::versionCheck(tv, item.template read_value<Version>()) && (!QueueVersioning::is_locked(tv) || item.has_write());
        }
        // shouldn't reach this
        assert(0);
        return false;
    }

    void install(TransItem& item, const Transaction&) {
	    // ignore lock_headversion marker item
        if (item.key<int>() == -2)
            return;
        // install pops
        if (has_delete(item)) {
            // only increment head if item popped from actual q
            if (!is_rw(item))
                head_ = (head_+1) % BUF_SIZE;
            QueueVersioning::inc_version(headversion_);
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

            QueueVersioning::inc_version(tailversion_);
        }
    }
    
    T queueSlots[BUF_SIZE];

    unsigned head_;
    unsigned tail_;
    Version tailversion_;
    Version headversion_;
};
