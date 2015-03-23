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

    bool empty() {
        return (head_ == tail_);
    }

    // TRANSACTIONAL CALLS
    void transPush(Transaction& t, const T& v) {
        auto item = t.item(this, -1);
        if (item.has_write()) {
            auto& write_list = item.template write_value<std::list<T>>();
            write_list.push_back(v);
        }
        else {
            std::list<T> write_list;
            write_list.push_back(v);
            item.add_write(write_list);
        }
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
                        auto& write_list = pushitem.template write_value<std::list<T>>();
                        // if there is an element to be pushed on the queue, return addr of queue element
                        if (!write_list.empty()) {
                            write_list.pop_front();
                            item.add_flags(read_writes);
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
            lockitem.add_read(headversion_);
            lockitem.add_write(0);
        }
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
            lockitem.add_read(headversion_);
            lockitem.add_write(0);
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
            return QueueVersioning::versionCheck(hv, item.template read_value<Version>()) || (!QueueVersioning::is_locked(hv) && !has_delete(item));
        }

        // check if we read off the write_list (and locked tailversion)
        else if (item.key<int>() == -1) {
            auto tv = tailversion_;
            return QueueVersioning::versionCheck(tv, item.template read_value<Version>());
        }
        return false;
    }

    void install(TransItem& item, const Transaction& t) {
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
    Version tailversion_;
    Version headversion_;
};
