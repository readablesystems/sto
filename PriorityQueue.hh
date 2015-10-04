#pragma once

#include <vector>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "versioned_value.hh"
#include "VersionFunctions.hh"

enum Status{
    AVAILABLE,
    EMPTY,
    BUSY,
};

template <typename T>
struct HeapNode{
    typedef versioned_value_struct<T> versioned_value;
    typedef TransactionTid::type Version;
public:
    versioned_value* val;
    Version ver;
    Status status;
    int owner;
    HeapNode(versioned_value* val_) : val(val_), ver(0), status(BUSY) {
        owner = Transaction::threadid;
    }
    
    bool amOwner() {
        return status == BUSY && owner == Transaction::threadid;
    }
};

template <typename T, bool Opacity = false>
class PriorityQueue: public Shared {
    typedef TransactionTid::type Version;
    typedef VersionFunctions<Version> Versioning;
    typedef versioned_value_struct<T> versioned_value;
    
    static constexpr TransItem::flags_type insert_tag = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_tag = TransItem::user0_bit<<1;
    static constexpr TransItem::flags_type dirty_tag = TransItem::user0_bit<<2;
    
    static constexpr Version insert_bit = TransactionTid::user_bit1;
    static constexpr Version delete_bit = TransactionTid::user_bit1<<1;
    static constexpr Version dirty_bit = TransactionTid::user_bit1<<2;
    
    static constexpr int NO_ONE = -1;
    
    static constexpr int pop_key = -2;
    static constexpr int empty_key = -3;
    static constexpr int top_key = -4;
public:
    PriorityQueue() : heap_() {
        size_ = 0;
        heaplock_ = 0;
        poplock_ = 0;
        popversion_ = 0;
        dirtytid_ = -1;
        dirtyval_ = -1;
        dirtycount_ = 0;
    }
    
    // Concurrently adds v to the priority queue
    void add(versioned_value* v) {
        lock(&heaplock_);
        int child = size_;
        if (child >= heap_.size()) {
            HeapNode<T>* new_node = new HeapNode<T>(v);
            heap_.push_back(new_node);
        } else {
            lock(&heap_[child]->ver);
            heap_[child]->val = v;
            heap_[child]->status = BUSY;
            heap_[child]->owner = Transaction::threadid;
            unlock(&heap_[child]->ver);
        }
        size_++;
        
        unlock(&heaplock_);
        while (child > 0) {
            int parent = (child - 1) / 2;
            versioned_value* before = heap_[parent]->val;
            lock(&(heap_[parent]->ver));
            lock(&(heap_[child]->ver));
            int old = child;
            if (heap_[parent]->status == AVAILABLE && heap_[child]->amOwner()) {
                versioned_value* parent_val = heap_[parent]->val;
                if (heap_[child]->val->read_value() > parent_val->read_value()) {
                    swap(child, parent);
                    child = parent;
                } else {
                    assert(heap_[child]->status == BUSY);
                    heap_[child]->status = AVAILABLE;
                    heap_[child]->owner = NO_ONE;
                    unlock(&(heap_[old]->ver));
                    unlock(&(heap_[parent]->ver));
                    return;
                }
            } else if (!heap_[child]->amOwner()) {
                child = parent;
            }
            
            unlock(&(heap_[old]->ver));
            unlock(&(heap_[parent]->ver));
        }
        
        if (child == 0) {
            lock(&(heap_[0]->ver));
            if (heap_[0]->amOwner()) {
                assert(heap_[child]->status == BUSY);
                heap_[0]->status = AVAILABLE;
                heap_[0]->owner = NO_ONE;
            }
            unlock(&(heap_[0]->ver));
        }
    }
    
    // Concurrently removes the maximum element from the heap
    versioned_value* removeMax(versioned_value* expVal = NULL) {
        lock(&heaplock_);
        int bottom  = --size_;
        if (bottom < 0) {
            unlock(&heaplock_);
            return NULL;
        }
        if (bottom == 0) {
            lock(&heap_[0]->ver);
            versioned_value* res = heap_[0]->val;
            unlock(&heaplock_);
            heap_[0]->status = EMPTY;
            heap_[0]->owner = NO_ONE;
            unlock(&heap_[0]->ver);
            return res;
        }
        lock(&heap_[0]->ver);
        while(1) {
            lock(&heap_[bottom]->ver);
            if (heap_[bottom]->status == AVAILABLE) {
                break;
            } else {
                unlock(&heap_[bottom]->ver);
            }
        }
        versioned_value* res = heap_[0]->val;
        unlock(&heaplock_);
        if (expVal != NULL && res != expVal) {
            unlock(&heap_[0]->ver);
            unlock(&heap_[bottom]->ver);
            unlock(&poplock_);
            Sto::abort();
            return NULL;
        }
        heap_[0]->status = EMPTY;
        heap_[0]->owner = NO_ONE;
        swap(bottom, 0);
        assert(heap_[bottom]->status == EMPTY);
        unlock(&heap_[bottom]->ver);
        
        int child = 0;
        int parent = 0;
        while (2*parent < size_ - 1) {
            int left = parent * 2 + 1;
            int right = (parent * 2) + 2;
            if (right >= size_) {
                lock(&heap_[left]->ver);
                if (heap_[left]->status == EMPTY) {
                    unlock(&heap_[left]->ver);
                    break;
                }
                if (heap_[left]->val->read_value() > heap_[parent]->val->read_value()) {
                    swap(parent, left);
                    unlock(&heap_[parent]->ver);
                    parent = left;
                } else {
                    unlock(&heap_[left]->ver);
                    break;
                }

            } else {
            lock(&heap_[left]->ver);
            lock(&heap_[right]->ver);
            if (heap_[left]->status == EMPTY) {
                unlock(&heap_[right]->ver);
                unlock(&heap_[left]->ver);
                break;
            } else if (heap_[right]->status == EMPTY || heap_[left]->val->read_value() > heap_[right]->val->read_value()) {
                unlock(&heap_[right]->ver);
                child = left;
            } else {
                unlock(&heap_[left]->ver);
                child = right;
            }
            if (heap_[child]->val->read_value() > heap_[parent]->val->read_value()) {
                swap(parent, child);
                unlock(&heap_[parent]->ver);
                parent = child;
            } else {
                unlock(&heap_[child]->ver);
                break;
            }
            }
        }
        unlock(&heap_[parent]->ver);
        return res;
    }
    
    versioned_value* getMax() {
        assert(is_locked(poplock_));
        if (size_ == 0) {
            return NULL;
        }
        while(1) {
            lock(&heap_[0]->ver);
            if (heap_[0]->status == AVAILABLE) {
                versioned_value* val = heap_[0]->val;
                unlock(&heap_[0]->ver);
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
            } else {
                assert(heap_[0]->status != AVAILABLE);
                unlock(&heap_[0]->ver);
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
        if (dirtytid_ != -1 && dirtytid_ != Transaction::threadid && v > dirtyval_) {
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
            if (read_val != NULL) Sto::abort();
            else Sto::item(this, empty_key).add_read(0);
            Sto::item(this, pop_key).add_read(popversion_);
            return -1;
        }
        
        lock(&poplock_);
        if (dirtytid_ != -1 && dirtytid_ != Transaction::threadid) {
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
            Sto::item(this, pop_key).add_read(popversion_);
            unlock(&poplock_);
            return -1;
        }
        if (dirtytid_ == -1 || val->read_value() < dirtyval_) {
            dirtyval_ = val->read_value();
            fence();
        }
        dirtytid_ = Transaction::threadid;
        
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
        Sto::item(this, pop_key).add_read(popversion_);
        acquire_fence();
        if (size_ == 0) {
            Sto::item(this, empty_key).add_read(0);
            return -1;
        }
        
        lock(&poplock_);
        if (dirtytid_ != -1 && dirtytid_ != Transaction::threadid) {
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
    
    int size() {
        return size_; // TODO: this is not transactional yet
    }
    
    void lock(versioned_value *e) {
        lock(&e->version());
    }
    void unlock(versioned_value *e) {
        unlock(&e->version());
    }
    
    void lock(TransItem& item) {
        if (item.key<int>() == pop_key){
            lock(&popversion_);
        } else {
            //lock(item.key<versioned_value*>());
        }
    }
    
    void unlock(TransItem& item) {
        if (item.key<int>() == pop_key){
            unlock(&popversion_);
        } else {
            //unlock(item.key<versioned_value*>());
        }
    }
    
    bool check(const TransItem& item, const Transaction& trans){
        if (item.key<int>() == top_key) { return true; }
        else if (item.key<int>() == empty_key) {
            // check that no other transaction  pushed items onto the queue
            for (int i = 0; i < size_; i++) {
                versioned_value* val = heap_[i]->val;
                auto it = Sto::check_item(this, val);
                if (!is_inserted(val->version()) ||
                    (is_locked(val->version()) && (it == NULL ||  ! (*it).has_lock())))
                    return false;
            }
            
            if (dirtytid_ != -1 && dirtytid_ != Transaction::threadid) return false;
            return true;
        }
        else if (item.key<int>() == pop_key) {
            auto lv = popversion_;
            return TransactionTid::same_version(lv, item.template read_value<Version>())
            && (!is_locked(lv) || item.has_lock(trans));
        } else {
            // This is top case
            auto e = item.key<versioned_value*>();
            if (dirtytid_ != -1 && dirtytid_ != Transaction::threadid && dirtyval_ >= e->read_value()) return false;
            else if (has_delete(item)) return true;
            // check that e is not pushed down by other transactions
            int level = 1; // level that contains the root
            bool found = false;
            for (int i = 0; i < size_; i++) {
                versioned_value* val = heap_[i]->val;
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
            if (dirtytid_ != -1 && dirtytid_ != Transaction::threadid && dirtyval_ >= e->read_value()) return false;
            if (!found) return false;
            else return true;
        }
    }
    
    
    void install(TransItem& item, const Transaction& t) {
        if (item.key<int>() == pop_key){
            if (Opacity) {
                TransactionTid::set_version(popversion_, t.commit_tid());
            } else {
                TransactionTid::inc_invalid_version(popversion_);
            }
        } else {
            auto e = item.key<versioned_value*>();
            if (has_insert(item)) {
                erase_inserted(&e->version());
            }
        }
    }
    
    void cleanup(TransItem& item, bool committed) {
        if (committed && dirtytid_ == Transaction::threadid) {
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
                    assert(dirtytid_ == Transaction::threadid);
                    dirtytid_ = -1;
                }
            }
        }
    }
    
    // Used for debugging
    void print() {
        for (int i =0; i < size_; i++) {
            std::cout << heap_[i]->val->read_value() << "[" << (!is_inserted(heap_[i]->val->version()) && !is_deleted(heap_[i]->val->version())) << "] ";
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
    
    static bool is_locked(Version v) {
        return TransactionTid::is_locked(v);
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
        assert(heap_[i]->ver == 1);
        assert(heap_[j]->ver == 1);
        HeapNode<T> tmp = *(heap_[i]);
        heap_[i]->val = heap_[j]->val;
        heap_[i]->status = heap_[j]->status;
        heap_[i]->owner = heap_[j]->owner;
        heap_[j]->val = tmp.val;
        heap_[j]->status = tmp.status;
        heap_[j]->owner = tmp.owner;
    }
    std::vector<HeapNode<T> *> heap_;
    Version heaplock_;
    Version poplock_;
    Version popversion_;
    int size_;
    int dirtyval_; // min value popped by a transaction that dirtied the queue
    int dirtytid_; // thread id of the transaction that dirtied the queue
    int dirtycount_; // number of pops by the transaction that dirtied the queue
    
    
};
