// atomic check_empty -- check empty, check version, check empty again
// tailversion -- lock instead of tail, check version# if empty

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

    void transPop(Transaction& t) {
        /*
        if empty_queue:
            if has_write(t.item(this,-1)) then remove first item off list (need to check that queue is still empty at commit time?)
            else fail
        */
        unsigned index = head_;
        auto& item = t.item(this, index);
        while (has_delete(item)) {
        //    if (index == tail_);
                //assert fail; 
            item = t.item(this, index);
            index = (index + 1) % BUF_SIZE;
        }
        item.set_flags(delete_bit);
        if (!item.has_read()) {
           t.add_read(item, headversion_);
            t.add_write(item, 0);
        }
    }

    T* transFront(Transaction& t) {
        /*
        if empty_queue:
            if has_write(t.item(this,-1)) then reference first item off list (need to check that queue is still empty at commit time?)
            else fail
        */
        unsigned index = head_;
        auto& item = t.item(this, index);
        while (has_delete(item)) {
         //   if (index == tail_)
            item = t.item(this, index);
            index = (index + 1) % BUF_SIZE;
        }
        if (!item.has_read())
           t.add_read(item, headversion_);
        return &queueSlots[index];
    }
    
private:
    bool has_delete(TransItem& item) {
        return item.has_flags(delete_bit);
    }
    
    void lock(Version& v) {
        QueueVersioning::lock(v);
    }

    void unlock(Version& v) {
        QueueVersioning::unlock(v);
    }
     
    void lock(TransItem& item) {
        if (has_delete(item))
            lock(headversion_);
        else if (item.has_write())
            lock(tailversion_);
    }

    void unlock(TransItem& item) {
        if (has_delete(item)) 
            unlock(headversion_);
        else if (item.has_write())
            unlock(tailversion_);
    }
  
    bool check(TransItem& item, Transaction& t) {
	    //if transaction assumed queue was empty, check if still empty
        (void) t;
        auto hv = headversion_;
        return (QueueVersioning::versionCheck(hv, item.template read_value<Version>()) && (!QueueVersioning::is_locked(hv) || !has_delete(item)));
    }

    void install(TransItem& item) {
	    if (has_delete(item)) {
            head_ = head_+1 % BUF_SIZE;
            QueueVersioning::inc_version(headversion_);
        }
        // another transaction inserting onto tail = don't need to increment queueversion
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
