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
    typedef VersionFunctions<Version> QueueVersioning;
    
    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type front_bit = TransItem::user0_bit<<1;

    void transPush(Transaction& t, const T& v) {
        auto& item = t.item(this, -1);
        if (item.has_write()) {
            auto& write_list = unpack<std::list<T>>(item.key());
            write_list.push_back(v);
        }
        else {
            std::list<T> write_list;
            write_list.push_back(v);
            t.add_write(item, write_list);
        }
    }

    bool transPop(Transaction& t) {
        unsigned index = head_;
        auto& item = t.item(this, index);
        while (has_delete(item)) {
            if (index == tail_) {
                Version tv = tailversion_;
                // if someone has pushed onto tail, can successfully do a front read, so skip reading from our pushes -- check?????
                if (index == tail_) {
                    auto& pushitem = t.item(this,-1);
                    if (pushitem.has_write()) {
                        auto& write_list = unpack<std::list<T>>(pushitem.key());
                        // if there is an element to be pushed on the queue, return addr of queue element
                        if (!write_list.empty()) {
                            write_list.pop_front();
                            // must ensure that tail_ is not modified at check
                            if (!item.has_read())
                                t.add_read(pushitem, tailversion_);
                        }
                    }
                } 
                return false; 
            }
            item = t.item(this, index);
            index = (index + 1) % BUF_SIZE;
        }
        item.assign_flags(delete_bit);
        if (!item.has_read()) {
           t.add_read(item, headversion_);
           t.add_write(item, 0);
        }
        return true;
    }

    T* transFront(Transaction& t) {
        unsigned index = head_;
        auto& item = t.item(this, index);
        while (has_delete(item)) {
            // empty queue
            if (index == tail_) {
                Version tv = tailversion_;
                // if someone has pushed onto tail, can successfully do a front read, so skip reading from our pushes -- check?????
                if (index == tail_) {
                    auto& pushitem = t.item(this,-1);
                    if (pushitem.has_write()) {
                        auto& write_list = unpack<std::list<T>>(pushitem.key());
                        // if there is an element to be pushed on the queue, return addr of queue element
                        if (!write_list.empty()) {
                            return &queueSlots[tail_];
                            // must ensure that tail_ is not modified at check
                            if (!item.has_read())
                                t.add_read(pushitem, tv);
                        }
                    }
                }
                return NULL;
            }
            item = t.item(this, index);
            index = (index + 1) % BUF_SIZE;
        }
        //check that head was not modified at time of commit
        if (!item.has_read())
           t.add_read(item, headversion_);
        item.assign_flags(front_bit);
        return &queueSlots[index];
    }
    
private:
  bool is_front(TransItem& item) {
      return item.flags() & front_bit;
    }

    bool has_delete(TransItem& item) {
        return item.flags() & delete_bit;
    }
    
    void lock(Version& v) {
        QueueVersioning::lock(v);
    }

    void unlock(Version& v) {
        QueueVersioning::unlock(v);
    }
     
    void lock(TransItem& item) {
        // only lock headversion for pops 
        if (has_delete(item))
            lock(headversion_);
        // only lock tailversion for pushes and front on empty queue
        else if (item.has_write() || item.has_read())
            lock(tailversion_);
    }

    void unlock(TransItem& item) {
        if (has_delete(item)) 
            unlock(headversion_);
        else if (item.has_write() || item.has_read())
            unlock(tailversion_);
    }
  
    bool check(const TransItem& item, const Transaction& t) {
        (void) t;
        auto hv = headversion_;
        auto tv = tailversion_;
        // check if was a pop or front (without dealing with empty queue)
        bool head_check = (QueueVersioning::versionCheck(hv, item.template read_value<Version>()) && (!QueueVersioning::is_locked(hv) || !has_delete(item)));
    
        // check if we read off the write_list (and locked tailversion)
        bool tail_check = (QueueVersioning::versionCheck(tv, item.template read_value<Version>()) && (!QueueVersioning::is_locked(tv) || has_delete(item) || is_front(item)));

        return head_check && tail_check;
    }

    void install(TransItem& item, const Transaction&) {
	    if (has_delete(item)) {
            head_ = head_+1 % BUF_SIZE;
            QueueVersioning::inc_version(headversion_);
        }
        else {
            auto write_list = unpack<std::list<T>>(item.key());
            auto head_index = head_;
            while (!write_list.empty()) {
                if (tail_ != head_index) {
                    tail_ = (tail_ +1) % BUF_SIZE;
                    queueSlots[tail_] = write_list.front();
                    write_list.pop_front();
                }
                //else return "fail"; ?????
            }
        }
    }
    
    T queueSlots[BUF_SIZE];

    unsigned head_;
    unsigned tail_;
    unsigned queuesize_;
    Version tailversion_;
    Version headversion_;
};
