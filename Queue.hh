#pragma once

#include "TaggedLow.hh"
#include "Transaction.hh"
#include "VersionFunctions.hh"

template <typename T, unsigned BUF_SIZE = 256> 
class Queue: public Shared {
public:
    // is this like a constructor???????
    Queue() : head_(0), tail_(0), queuesize_(0), queueversion_(0) {}

    typedef uint32_t Version;
    typedef VersionFunctions<Version, 0> QueueVersioning;
    
    static constexpr Version lock_bit = 1<<1;

    T queueSlots[BUF_SIZE];

    void transPush(Transaction& t, const Value& v) {
        auto& item = t.item(this, -1);
        if (item.has_write()) {
            // unsure if correct way to create list??????
            auto write_list = unpack<list<T>>(-1);
            write_list.push_back(v);
        }
        else list<T> write_list {v};
        t.add_write(item, write_list);
    }

    void transPop(Transaction& t) {
        /*
        if empty_queue:
            if has_write(t.item(this,-1)) then remove first item off list (need to check that queue is still empty at commit time?)
            else fail
        */
        long index = head;
        auto& item = t.item(this, index);
        while (has_delete(item)) {
            if (index = tail)
                assert fail; 
            item = t.item(this, index);
            index = (index + 1) % buf_size;
        }
        item.set_flags(delete_bit);
        if (!item.has_read())
           t.add_read(item, queueversion);
    }

    Value& transFront(Transaction& t) {
        /*
        if empty_queue:
            if has_write(t.item(this,-1)) then reference first item off list (need to check that queue is still empty at commit time?)
            else fail
        */
        long index = head;
        auto& item = t.item(this, index);
        while (has_delete(item)) {
            if (index = tail)
                //assert fail; ??
            item = t.item(this, index);
            index = (index + 1) % buf_size;
        }
        if (!item.has_read())
           t.add_read(item, queueversion);
        return &queueSlots[index];
    }
    
private:
    bool has_delete(TransItem& item) {
        return item.has_flags(delete_bit);
    }

    bool is_locked(long *index) {
        return index.has_flags(lock_bit);
    }
    
    void lock(Version& v) {
        ListVersioning::lock(v);
    }

    void unlock(Version& v) {
        ListVersioning::unlock(v);
    }
     
    void lock(TransItem& item) {
        if (has_delete(item))
            head.atomic_add_flags(lock_bit);
        else if (item.has_write)
            tail.atomic_add_flags(lock_bit);
        lock(queueversion_);
    }

    void unlock(TransItem& item) {
        if (has_delete(item))
            head.set_flags(head.flags() & ~lock_bit);
        else if (item.has_write)
            tail.set_flags(head.flags() & ~lock_bit);
        unlock(queueversion_);
    }
  
    bool check(TransItem& item, Transaction& t) {
	    //if transaction assumed queue was empty, check if still empty
        auto qv = queueversion_;
        if (item.has_read())
           return QueueVersioning::versionCheck(qv, item.template read_value<Version>());//&& (!is_locked(lv));  ?????
    }

    void install(TransItem& item) {
	    if (has_delete(item)) {
            assert(item.key() == head index);
            head_index = (head_index+1) % BUF_SIZE;
            QueueVersioning::inc_version(queueversion_);
        }
        else if (item.has_write()) {
            write_list = unpack<list<T>>(item.key());
            while (!write_list.empty()) {
                tail = (tail+1) % BUF_SIZE; 
                if (tail != head)
                    queue[tail] = write_list.front();
                    write_list.pop_front();
                //else return "fail"; ?????
            }
        }
    }

    // not sure if taggedlow is the correct way to implement????
    TaggedLow<long> head_;
    TaggedLow<long> tail_;
    long queuesize_;
    Version queueversion_;
};
