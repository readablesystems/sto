#pragma once

#include <vector>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "versioned_value.hh"


template <typename T, bool Opacity = false>
class PriorityQueue: public TObject {
    typedef TransactionTid::type Version;
    typedef versioned_value_struct<T> versioned_value;
    
    static constexpr TransItem::flags_type insert_tag = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_tag = TransItem::user0_bit<<1;
    static constexpr TransItem::flags_type dirty_tag = TransItem::user0_bit<<2;

    static constexpr Version insert_bit = TransactionTid::user_bit; // XXX get rid of this
    static constexpr Version delete_bit = TransactionTid::user_bit<<1; // XXX get rid of this
    static constexpr Version dirty_bit = TransactionTid::user_bit<<2; // XXX get rid of this

    static constexpr int pop_key = -2;
    static constexpr int empty_key = -3;
    static constexpr int top_key = -4;
public:
    PriorityQueue() : heap_() {
        size_ = 0;
        poplock_ = 0;
        popversion_ = 0;
        dirtytid_ = -1;
        dirtyval_ = -1;
        dirtycount_ = 0;
    }

    // Adds v to the priority queue
    void add(versioned_value* v) {
        int child = size_;
        if (child >= heap_.size()) {
            heap_.push_back(v);
        } else {
            heap_[child] = v;
        }
        size_++;

        while (child > 0) {
            int parent = (child - 1) / 2;
            versioned_value* before = heap_[parent];
            int old = child;
            versioned_value* parent_val = heap_[parent];
            if (heap_[child]->read_value() > parent_val->read_value()) {
                swap(child, parent);
                child = parent;
            } else {
                return;
            }
        }
    }
    
    // Removes the maximum element from the heap
    versioned_value* removeMax(versioned_value* expVal = NULL) {
        int bottom  = --size_;
        if (bottom < 0) {
            return NULL;
        }
        if (bottom == 0) {
            versioned_value* res = heap_[0];
            return res;
        }
        
        versioned_value* res = heap_[0];

        if (expVal != NULL && res != expVal) {
            unlock(&poplock_);
            Sto::abort();
            return NULL;
        }
        swap(bottom, 0);
        
        int child = 0;
        int parent = 0;
        while (2*parent < size_ - 1) {
            int left = parent * 2 + 1;
            int right = (parent * 2) + 2;
            if (right >= size_) {
                if (left >= size_) {
                    break;
                }
                if (heap_[left]->read_value() > heap_[parent]->read_value()) {
                    swap(parent, left);
                    parent = left;
                } else {
                    break;
                }

            } else {
                if (heap_[left]->read_value() > heap_[right]->read_value()) {
                    child = left;
                } else {
                    child = right;
                }
                if (heap_[child]->read_value() > heap_[parent]->read_value()) {
                    swap(parent, child);
                    parent = child;
                } else {
                    break;
                }
            }
        }
        return res;
    }
    
    versioned_value* getMax() {
        assert(TransactionTid::is_locked_here(poplock_));
        if (size_ == 0) {
            return NULL;
        }
        while(1) {
            versioned_value* val = heap_[0];
            auto item = Sto::item(this, val);
            if (is_inserted(val->version())) {
                if (has_insert(item)) {
                    // push then pop
                    return val;
                } else {
                    // Some other transaction is inserting a node with high priority
                    unlock(&poplock_);
                    Sto::abort();
                    return NULL;
                }
            } else if (is_deleted(val->version())) {
                removeMax(val);
                if (size_ == 0) return NULL;
            } else {
                return val;
            }
        }
    }
    
    void push_nontrans(T v) {
        lock(&poplock_);
        versioned_value* val = versioned_value::make(v, TransactionTid::increment_value + insert_bit);
        add(val);
        unlock(&poplock_);
    }
    
    void push(T v) {
        lock(&poplock_); // TODO: locking this is not required, but performance seems to be better with this
                            // Can also try readers-writers lock
        if (dirtytid_ != -1 && dirtytid_ != TThread::id() && v > dirtyval_) {
            unlock(&poplock_);
            Sto::abort();
            return;
        }
        versioned_value* val = versioned_value::make(v, TransactionTid::increment_value + insert_bit);
        add(val);
        Sto::item(this, val).add_write(v).add_flags(insert_tag);
        unlock(&poplock_);
        
    }
    
    T pop() {
        // Check if we previously read the top element.
        auto top_item = Sto::check_item(this, top_key);
        versioned_value* read_val = NULL;
        if (top_item != NULL && top_item->has_read()) {
            read_val = (*top_item).template read_value<versioned_value*>();
        }
        // Check if we previously saw the queue as empty.
        auto empty_item = Sto::check_item(this, empty_key);
        bool read_empty = empty_item != NULL && empty_item->has_read();
        
        if (size_ == 0) {
            if (read_val != NULL) {
                Sto::abort();
            }
            else Sto::item(this, empty_key).add_read(0);
            // XXX opacity
            Sto::item(this, pop_key).add_read(TransactionTid::unlocked(popversion_));
            return -1;
        }
        
        lock(&poplock_);
        if (dirtytid_ != -1 && dirtytid_ != TThread::id()) {
            // queue is in dirty state
            unlock(&poplock_);
            Sto::abort();
            return -1;
        }
        
        versioned_value* val = getMax();
        // If we already read the top value, then either val = read_val or val is pushed by the current transaction
        bool shouldBeInserted = false;
        if (read_empty && val != NULL) shouldBeInserted = true;
        if (read_val != NULL && read_val->read_value() == val->read_value()) { // TODO: Should we compare values or versioned_values?
            top_item->remove_read();
        } else if (read_val != NULL) {
            shouldBeInserted = true;
        }
        auto item = Sto::item(this, val);
        if (shouldBeInserted && !has_insert(item)) {
                unlock(&poplock_);
                Sto::abort();
                return -1;
        }
        
        if (val == NULL) {
            Sto::item(this, empty_key).add_read(0);
            Sto::item(this, pop_key).add_read(TransactionTid::unlocked(popversion_));
            unlock(&poplock_);
            return -1;
        }
        if (dirtytid_ == -1 || val->read_value() < dirtyval_) {
            dirtyval_ = val->read_value();
            fence();
        }
        dirtytid_ = TThread::id();
        
        removeMax(val);
        unlock(&poplock_);
        
        if (has_insert(item)) {
            item.add_flags(delete_tag);
        } else {
            item.add_write(0).add_flags(delete_tag);
            dirtycount_++;
        }
        
        
        Sto::item(this, pop_key).add_write(0);
        return val->read_value();
    }
    
    T top() {
        if (size_ == 0) {
            Sto::item(this, empty_key).add_read(0);
            return -1;
        }
        
        Sto::item(this, pop_key).add_read(TransactionTid::unlocked(popversion_));
        acquire_fence();
        if (size_ == 0) {
            Sto::item(this, empty_key).add_read(0);
            return -1;
        }
        
        lock(&poplock_);
        if (dirtytid_ != -1 && dirtytid_ != TThread::id()) {
            // queue is in dirty state
            unlock(&poplock_);
            Sto::abort();
        }
        versioned_value* val = getMax();
        unlock(&poplock_);
        if (val == NULL) {
            Sto::item(this, empty_key).add_read(0);
            return -1;
        }
        T retval = val->read_value();
        Sto::item(this, val).add_read(val->version());
        Sto::item(this, top_key).add_read(val);
        return retval;
    }
    
    int unsafe_size() {
        return size_; // TODO: this is not transactional yet
    }
    
    void lock(versioned_value *e) {
        lock(&e->version());
    }
    void unlock(versioned_value *e) {
        unlock(&e->version());
    }
    
    bool lock(TransItem& item, Transaction& txn) override {
        return item.key<int>() != pop_key
            || txn.try_lock(item, popversion_);
    }
    
    bool check(TransItem& item, Transaction&) override {
        if (item.key<int>() == top_key) { return true; }
        else if (item.key<int>() == empty_key) {
            // check that no other transaction  pushed items onto the queue
            for (int i = 0; i < size_; i++) {
                versioned_value* val = heap_[i];
                if (!is_inserted(val->version())
                    || TransactionTid::is_locked_elsewhere(val->version()))
                    return false;
            }
            
            if (dirtytid_ != -1 && dirtytid_ != TThread::id()) return false;
            return true;
        }
        else if (item.key<int>() == pop_key) {
            return TransactionTid::check_version(popversion_, item.template read_value<Version>());
        } else {
            // This is top case
            auto e = item.key<versioned_value*>();
            if (dirtytid_ != -1 && dirtytid_ != TThread::id() && dirtyval_ >= e->read_value()) return false;
            else if (has_delete(item)) return true;
            // check that e is not pushed down by other transactions
            int level = 1; // level that contains the root
            bool found = false;
            for (int i = 0; i < size_; i++) {
                versioned_value* val = heap_[i];
                if (val == e || val->read_value() == e->read_value()) found = true; 
                else if (val->read_value() > e->read_value()) {
                    auto it = Sto::check_item(this, val);
                    if (it != NULL && has_insert(*it)) {
                        level = findLevel(i) + 1;
                        continue;
                    } else {
                        return false;
                    }
                }
                if (i == endOfLevel(level)) break;
            }
            if (dirtytid_ != -1 && dirtytid_ != TThread::id() && dirtyval_ >= e->read_value()) return false;
            if (!found) return false;
            else return true;
        }
    }
    
    
    void install(TransItem& item, Transaction& t) override {
        if (item.key<int>() == pop_key){
            if (Opacity) {
                TransactionTid::set_version(popversion_, t.commit_tid());
            } else {
                TransactionTid::inc_nonopaque_version(popversion_);
            }
        } else {
            auto e = item.key<versioned_value*>();
            if (has_insert(item)) {
                erase_inserted(&e->version());
            }
        }
    }
    
    void unlock(TransItem& item) override {
        if (item.key<int>() == pop_key)
            unlock(&popversion_);
    }

    void cleanup(TransItem& item, bool committed) override {
        if (committed && dirtytid_ == TThread::id()) {
            dirtytid_ = -1;
        }
        if (!committed) {
            if(has_insert(item) && has_delete(item)) {
                // Do nothing
                return;
            }
            if (has_insert(item)) {
                auto e = item.key<versioned_value*>();
                mark_deleted(&e->version());
                fence();
                erase_inserted(&e->version());
            } else if (has_delete(item)) {
                auto e = item.key<versioned_value*>();
                auto v = e->read_value();
                versioned_value* val = versioned_value::make(v, TransactionTid::increment_value);
                lock(&poplock_);
                add(val);
                unlock(&poplock_);
                fence();
                dirtycount_--;
                if (dirtycount_ == 0) {
                    assert(dirtytid_ == TThread::id());
                    dirtytid_ = -1;
                }
            }
        }
    }
    
    // Used for debugging
    void print() {
        for (int i =0; i < size_; i++) {
            std::cout << heap_[i]->read_value() << "[" << (!is_inserted(heap_[i]->version()) && !is_deleted(heap_[i]->version())) << "] ";
        }
        std::cout << std::endl;
    }
    
    
private:
    static void lock(Version *v) {
        TransactionTid::lock(*v);
    }
    
    static void unlock(Version *v) {
        TransactionTid::unlock(*v);
    }
    
    static bool has_insert(const TransItem& item) {
        return item.flags() & insert_tag;
    }
    static bool has_delete(const TransItem& item) {
        return item.flags() & delete_tag;
    }
    
    static bool has_dirty(const TransItem& item) {
        return item.flags() & dirty_tag;
    }
    
    static bool is_inserted(Version v) {
        return v & insert_bit;
    }
    
    static void erase_inserted(Version* v) {
        *v = *v & (~insert_bit);
    }
    
    static void mark_inserted(Version* v) {
        *v = *v | insert_bit;
    }
    
    static bool is_dirty(Version v) {
        return v & dirty_bit;
    }
    
    static void erase_dirty(Version* v) {
        assert(is_dirty(*v));
        *v = *v & (~dirty_bit);
    }
    
    static void mark_dirty(Version* v) {
        assert(!is_dirty(*v));
        *v = *v | dirty_bit;
    }
            
    static bool is_deleted(Version v) {
        return v & delete_bit;
    }
            
    static void erase_deleted(Version* v) {
        *v = *v & (~delete_bit);
    }
            
    static void mark_deleted(Version* v) {
        *v = *v | delete_bit;
    }
    
    static int findLevel(int i) {
        return ceil(log((double) (i+2)) / log(2.0));
    }
    
    static int endOfLevel(int l) {
        assert(l >= 1);
        return (1 << l) - 2;
    }


    void swap(int i, int j) {
        versioned_value* tmp = heap_[i];
        heap_[i] = heap_[j];
        heap_[j] = tmp;
    }
    
    std::vector<versioned_value *> heap_;
    Version poplock_;
    Version popversion_;
    int size_;
    int dirtyval_; // min value popped by a transaction that dirtied the queue
    int dirtytid_; // thread id of the transaction that dirtied the queue
    int dirtycount_; // number of pops by the transaction that dirtied the queue
    
    
};
