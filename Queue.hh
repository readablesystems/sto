#pragma once

#include <list>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "VersionFunctions.hh"

template <typename T, unsigned BUF_SIZE = 4096> 
class Queue: public Shared {
public:
    Queue() : head_(0), tail_(0), queuesize_(0), tailversion_(0), headversion_(0) {}

    typedef uint32_t Version;
    typedef VersionFunctions<Version> QueueVersioning;
    
    static constexpr Version delete_bit = 1<<0;
    static constexpr Version front_bit = 1<<1;
    static constexpr Version pop_bit = 1<<2;
    static constexpr Version read_writes = 1<<3;

    // NONTRANSACTIONAL PUSH/POP/EMPTY
    void push(T v) {
        queueSlots[tail_] = v;
        tail_ = (tail_+1)%BUF_SIZE;
    }
    
    T pop() {
        assert(head_ != tail_);
        T v = queueSlots[head_];
        head_ = (head_+1)%BUF_SIZE;
        return v;
    }
   
    // empty 
    bool empty() {
        return (head_ == tail_);
    }

    // TRANSACTIONAL CALLS
    void transPush(Transaction& t, const T& v) {
        auto& item = t.item(this, -1);
        if (item.has_write()) {
            auto& write_list = item.template write_value<std::list<T>>();
            write_list.push_back(v);
        }
        else {
            std::list<T> write_list;
            write_list.push_back(v);
            t.add_write(item, write_list);
        }
    }

    bool transPop(Transaction& t) {
        auto index = head_;
        auto item = &t.item(this, index);
        // mark only the first item popped so we only lock headversion once
        if (!has_delete(*item))
            item->or_flags(pop_bit);

        while (1) {
           if (index == tail_) { 
               Version tv = tailversion_;
                // if someone has pushed onto tail, can successfully do a front read, so don't read our own writes
                if (index == tail_) {
                    auto& pushitem = t.item(this,-1);
                    if (pushitem.has_write()) {
                        auto& write_list = pushitem.template write_value<std::list<T>>();
                        // if there is an element to be pushed on the queue, return addr of queue element
                        if (!write_list.empty()) {
                            write_list.pop_front();
                            item->or_flags(read_writes);
                        }
                        else return false;
                    }
                    // fail if trying to read from an empty queue
                    else return false; 
                    
                    if (!pushitem.has_read())
                        t.add_read(pushitem, tailversion_);
                } 
            }
            if (has_delete(*item)) {
                index = (index + 1) % BUF_SIZE;  
                item = &t.item(this, index);
            }
            else break;
        }
        
        // ensure that head is not modified by time of commit 
        item->or_flags(delete_bit);
        if (!item->has_read()) {
           t.add_read(*item, headversion_);
        }
        t.add_write(*item, 0);
        return true;
    }

    bool transFront(Transaction& t, T& val) {
        unsigned index = head_;
        auto item = &t.item(this, index);
        while (1) {
            // empty queue
            if (index == tail_) {
                Version tv = tailversion_;
                // if someone has pushed onto tail, can successfully do a front read, so skip reading from our pushes 
                if (index == tail_) {
                    auto& pushitem = t.item(this,-1);
                    if (pushitem.has_write()) {
                        auto& write_list = pushitem.template write_value<std::list<T>>();
                        // if there is an element to be pushed on the queue, return addr of queue element
                        if (!write_list.empty()) {
                            // install tail elements immediately
                            queueSlots[tail_] = write_list.front();
                            write_list.pop_front();
                            tail_ = (tail_ + 1) % BUF_SIZE;
                            val = queueSlots[index];
                            return true;
                        }
                        else return false;
                    }
                    if (!pushitem.has_read())
                        t.add_read(pushitem, tailversion_);
                }
                return false;
            }
            if (has_delete(*item)) {
                index = (index + 1) % BUF_SIZE;
                item = &t.item(this, index);
            }
            else break;
        }
        
        // ensure that head was not modified at time of commit
        if (!item->has_read())
           t.add_read(*item, headversion_);
        item->or_flags(front_bit);
        val = queueSlots[index];
        return true;
    }
    
private:
  bool is_front(TransItem& item) {
        return item.has_flags(front_bit);
    }

    bool has_delete(TransItem& item) {
        return item.has_flags(delete_bit);
    }
    
    bool is_rw(TransItem& item) {
        return item.has_flags(read_writes);
    }

    bool first_pop(TransItem& item) {
        return item.has_flags(pop_bit);
    }
   
    void lock(Version& v) {
        QueueVersioning::lock(v);
    }

    void unlock(Version& v) {
        QueueVersioning::unlock(v);
    }
     
    void lock(TransItem& item) {
        // only lock headversion for pops 
        if (has_delete(item)) {
            if (first_pop(item)) {
                lock(headversion_);
            }
        }
        // only lock tailversion for pushes and front on empty queue
        else if (item.has_write() || item.has_read())
            lock(tailversion_);
    }

    void unlock(TransItem& item) {
        if (has_delete(item)) {
            if (has_delete(item) && first_pop(item))
                unlock(headversion_);
        }
        else if (item.has_write() || item.has_read())
            unlock(tailversion_);
    }
  
    bool check(TransItem& item, Transaction& t) {
        (void) t;
        auto hv = headversion_;
        auto tv = tailversion_;
        // check if was a pop or front 
        if (is_front(item) || has_delete(item))
            return QueueVersioning::versionCheck(hv, item.template read_value<Version>()) || (!QueueVersioning::is_locked(hv) && !has_delete(item));

        // check if we read off the write_list (and locked tailversion)
        else 
            return QueueVersioning::versionCheck(tv, item.template read_value<Version>());
    }

    void install(TransItem& item, uint32_t tid) {
	    if (has_delete(item)) {
            // only increment head if item popped from actual q
            if (!is_rw(item))
                head_ = (head_+1) % BUF_SIZE;
            QueueVersioning::inc_version(headversion_);
        }
        else {
            auto& write_list = item.template write_value<std::list<T>>();
            auto head_index = head_;
            // write all the elements that have not yet been installed 
            while (!write_list.empty()) {
                // assert queue is not out of space            
                assert(tail_ != (head_index-1) % BUF_SIZE);
                queueSlots[tail_] = write_list.front();
                write_list.pop_front();
                tail_ = (tail_+1) % BUF_SIZE;
            }
            QueueVersioning::inc_version(tailversion_);
        }
    }
    
    T queueSlots[BUF_SIZE];

    unsigned head_;
    unsigned tail_;
    unsigned queuesize_;
    Version tailversion_;
    Version headversion_;
};
