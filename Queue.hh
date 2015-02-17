#pragma once

#include <list>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "VersionFunctions.hh"

template <typename T, unsigned BUF_SIZE = 256> 
class Queue: public Shared {
public:
    // is this like a constructor???????
    Queue() : head_(NULL), tail_(NULL), queuesize_(0), queueversion_(0) {}

    typedef uint32_t Version;
    typedef VersionFunctions<Version, 0> QueueVersioning;
    
    static constexpr Version lock_bit = 1<<1;

    static constexpr Version delete_bit = 1<<0;

    T queueSlots[BUF_SIZE];

    void transPush(Transaction& t, const T& v) {
        auto& item = t.item(this, -1);
        std::list<T> write_list;
        if (item.has_write())
            // unsure if correct way to create list??????
            write_list = unpack<std::list<T>>(item.key());
        else write_list.push_back(v);
        t.add_write(item, write_list);
    }

    void transPop(Transaction& t) {
        /*
        if empty_queue:
            if has_write(t.item(this,-1)) then remove first item off list (need to check that queue is still empty at commit time?)
            else fail
        */
        std::ptrdiff_t index = get_index(head_);
        std::ptrdiff_t tail_index = get_index(tail_);
        auto& item = t.item(this, index);
        while (has_delete(item)) {
            if (index = tail_index);
                //assert fail; 
            item = t.item(this, index);
            index = (index + 1) % BUF_SIZE;
        }
        item.set_flags(delete_bit);
        if (!item.has_read())
           t.add_read(item, queueversion_);
    }

    T* transFront(Transaction& t) {
        /*
        if empty_queue:
            if has_write(t.item(this,-1)) then reference first item off list (need to check that queue is still empty at commit time?)
            else fail
        */
        std::ptrdiff_t index = get_index(head_);
        std::ptrdiff_t tail_index = get_index(tail_);
        auto& item = t.item(this, index);
        while (has_delete(item)) {
            if (index = tail_index)
            item = t.item(this, index);
            index = (index + 1) % BUF_SIZE;
        }
        if (!item.has_read())
           t.add_read(item, queueversion_);
        return &queueSlots[index];
    }
    
private:
    std::ptrdiff_t get_index(T *ptr) {
        if (ptr) return ptr - queueSlots;
        else return 0;
    }
    
    bool has_delete(TransItem& item) {
        return item.has_flags(delete_bit);
    }

    bool is_locked(T *index) {
        return index.has_flags(lock_bit);
    }
    
    void lock(Version& v) {
        QueueVersioning::lock(v);
    }

    void unlock(Version& v) {
        QueueVersioning::unlock(v);
    }
     
    void lock(TransItem& item) {
        if (has_delete(item))
            head_.atomic_add_flags(lock_bit);
        else if (item.has_write())
            tail_.atomic_add_flags(lock_bit);
        lock(queueversion_);
    }

    void unlock(TransItem& item) {
        if (has_delete(item))
            head_.set_flags(head_.flags() & ~lock_bit);
        else if (item.has_write())
            tail_.set_flags(head_.flags() & ~lock_bit);
        unlock(queueversion_);
    }
  
    bool check(TransItem& item, Transaction& t) {
	    //if transaction assumed queue was empty, check if still empty
        auto qv = queueversion_;
        //check to ensure that no other thread has locked head/tail, or that we were the ones to lock
        // only need to check if another transaction is about to change the version number if this is a read????
        return (QueueVersioning::versionCheck(qv, item.template read_value<Version>()) && (!is_locked(tail) || item.has_write()));
    }

    void install(TransItem& item) {
	    if (has_delete(item)) {
            auto index = get_index(head_);
            assert(item.key() == (void*)index);
            head_ = &queueSlots[index+1 % BUF_SIZE];
            QueueVersioning::inc_version(queueversion_);
        }
        // another transaction inserting onto tail = don't need to increment queueversion????
        else if (item.has_write()) {
            auto write_list = unpack<std::list<T>>(item.key());
            auto head_index = get_index(head_);
            auto index = get_index(tail_);
            while (!write_list.empty()) {
                if (index != head_index) {
                    index = (index+1) % BUF_SIZE;
                    queueSlots[index] = write_list.front();
                    write_list.pop_front();
                }
                //else return "fail"; ?????
            }
            tail_ = &queueSlots[index % BUF_SIZE];
        }
    }

    // not sure if taggedlow is the correct way to implement????
    TaggedLow<T> head_;
    TaggedLow<T> tail_;
    long queuesize_;
    Version queueversion_;
};
