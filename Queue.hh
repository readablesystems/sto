// atomic check_empty -- check empty, check version, check empty again

#pragma once

#include <list>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "VersionFunctions.hh"

template <typename T, unsigned BUF_SIZE = 256> 
class Queue: public Shared {
public:
    Queue() : head_(0), tail_(0), queuesize_(0), tailversion_(0), headversion_(0) {}

    typedef uint32_t Version;
    typedef VersionFunctions<Version, 0> QueueVersioning;
    
    static constexpr Version delete_bit = 1<<0;
    static constexpr Version front_bit = 1<<1;
    static constexpr Version pop_bit = 1<<2;

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
                            // must ensure that tail_ is not modified at check
                        }
                    }
                    // fail because trying to read from an empty queue
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
       
        item->or_flags(delete_bit);
        if (!item->has_read()) {
           t.add_read(*item, headversion_);
        }
        t.add_write(*item, 0);
        return true;
    }

    T* transFront(Transaction& t) {
        unsigned index = head_;
        auto item = &t.item(this, index);
//dowhile
        while (1) {
            // empty queue
            if (index == tail_) {
                Version tv = tailversion_;
                // if someone has pushed onto tail, can successfully do a front read, so skip reading from our pushes 
                if (index == tail_) {
                    auto& pushitem = t.item(this,-1);
                    if (pushitem.has_write()) {
                        auto& write_list = item->template write_value<std::list<T>>();
                        // if there is an element to be pushed on the queue, return addr of queue element
                        if (!write_list.empty()) {
                            return &queueSlots[tail_];
                        }
                    }
                    if (!pushitem.has_read())
                        t.add_read(pushitem, tailversion_);
                }
                return NULL;
            }
            if (has_delete(*item)) {
                index = (index + 1) % BUF_SIZE;
                item = &t.item(this, index);
            }
            else break;
        }
        //check that head was not modified at time of commit
        if (!item->has_read())
           t.add_read(*item, headversion_);
        item->or_flags(front_bit);
        return &queueSlots[index];
    }
    
private:
  bool is_front(TransItem& item) {
        return item.has_flags(front_bit);
    }

    bool has_delete(TransItem& item) {
        return item.has_flags(delete_bit);
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

    void install(TransItem& item) {
	    if (has_delete(item)) {
            head_ = (head_+1) % BUF_SIZE;
            QueueVersioning::inc_version(headversion_);
        }
        else {
            auto& write_list = item.template write_value<std::list<T>>();
            auto head_index = head_;
            bool list_empty = write_list.empty();
            assert(!list_empty);
            
            while (!write_list.empty()) {
                // queue out of space            
                assert(tail_ != (head_index-1) % BUF_SIZE);
                queueSlots[tail_] = write_list.front();
                write_list.pop_front();
                tail_ = (tail_+1) % BUF_SIZE;
            }
            if (!list_empty)
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
