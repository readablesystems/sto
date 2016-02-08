#pragma once

#include "config.h"
#include "compiler.hh"
#include <iostream>
#include <vector>
#include <map>
#include "Transaction.hh"
#include "Box.hh"
#include "VersionFunctions.hh"
#include "rwlock.hh"

#define IT_SIZE 10000
#define log2(x) ceil(log((double) size) / log(2.0))

class OutOfBoundsException {};

template<typename T, bool Opacity = false, typename Elem = Box<T>> class VecIterator;
template<typename T, bool Opacity, typename Elem> struct T_wrapper;

template <typename T, bool Opacity = false, typename Elem = Box<T>>
class Vector : public Shared {
    friend class VecIterator<T, Opacity, Elem>;
    
    typedef TransactionTid::type Version;
    typedef VersionFunctions<Version> Versioning;
    typedef T_wrapper<T, Opacity, Elem> wrapper;
    
    static constexpr int push_back_key = -1;
    static constexpr int size_key = -2;
    static constexpr int size_pred_key = -3;
    static constexpr TransItem::flags_type list_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit<<1;
    static constexpr int32_t eq_bit = 0;
    static constexpr int32_t geq_bit = 1;
    static constexpr int32_t pred_shift = 1;
    static constexpr int32_t pred_mask = 1;
public:
    typedef int key_type;
    typedef T value_type;
    
    typedef VecIterator<T, Opacity, Elem> iterator;
    typedef const VecIterator<T, Opacity, Elem> const_iterator;
    typedef wrapper& reference;
    typedef int32_t size_type;

    
    Vector(): resize_lock_() {
        capacity_ = 0;
        size_ = 0;
        vecversion_ = 0;
        data_ = NULL;
        it_objs = new wrapper[IT_SIZE];
        for (int i = 0; i < IT_SIZE; i++) {
          it_objs[i].initialize(this, i);
        }
    }
    
    Vector(size_type size): resize_lock_() {
        size_ = 0;
        capacity_ = 1 << ((int) log2(size));
        vecversion_ = 0;
        it_objs = new wrapper[IT_SIZE];
        for (int i = 0; i < IT_SIZE; i++) {
          it_objs[i].initialize(this, i);
        }

        data_ = new Elem[capacity_];
        for (int i = 0; i < capacity_; i++) {
            data_[i].initialize(this, i);
        }
    }
    
    void reserve(size_type new_capacity) {
        if (new_capacity <= capacity_)
            return;
        Elem * new_data = new Elem[new_capacity];
        
        resize_lock_.write_lock();
        for (size_type i = 0; i < capacity_; i++) {
            new_data[i] = data_[i];
        }
        for (size_type i = size_; i < new_capacity; i++) {
            new_data[i].initialize(this, i);
        }
        capacity_ = new_capacity;
        if (data_ != NULL) {
            Elem* old_data = data_;
            Transaction::rcu_cleanup([old_data] () {delete[] old_data; });
        }
        data_ = new_data;
        resize_lock_.write_unlock();
    }
    
    void push_back(const value_type& v) {
        if (trans_size_offs() < 0) {
            // There are some deleted items in the array, we need to overwrite them
            auto item = Sto::item(this, size_ + trans_size_offs());
            if (!item.has_write() || ! has_delete(item)) {
                // some other transaction has pushed items in the mean time, so abort
                Sto::abort();
            } else {
                item.clear_write();
                item.clear_flags(delete_bit);
                item.add_write(v);
                add_trans_size_offs(1);
            }
            return;
        }
        auto item = Sto::item(this, push_back_key);
        if (item.has_write()) {
            if (!is_list(item)) {
                auto& val = item.template write_value<T>();
                std::vector<T> write_list;
                write_list.push_back(val);
                write_list.push_back(v);
                item.clear_write();
                item.add_write(write_list);
                item.add_flags(list_bit);
            }
            else {
                auto& write_list = item.template write_value<std::vector<T>>();
                write_list.push_back(v);
            }
        }
        else {
            item.add_write(v);
            item.clear_flags(list_bit);
        }
        add_lock_vector_item();
        add_trans_size_offs(1);
    }
    
    void pop_back() {
        auto item = Sto::item(this, push_back_key);
        if (item.has_write()) {
            if (!is_list(item)) {
                item.clear_write();
                add_trans_size_offs(-1);
                return;
            }
            else {
                /* list */
                auto& write_list= item.template write_value<std::vector<T>>();
                write_list.pop_back();
                if (write_list.size() == 0) item.clear_write();
                add_trans_size_offs(-1);
                return;
            }
        }
        // add vecversion_ to the read set
        auto vecitem = t_item(this).add_read(vecversion_);
        acquire_fence();
        size_type size = size_;
        acquire_fence();
        if (size + trans_size_offs() == 0) {
            // Empty vector, behavior is undefined - so do nothing
        } else {
            // add vecversion_ to the write set as well
            vecitem.add_write(0);
            auto item = Sto::item(this, size_ + trans_size_offs() - 1).add_write(0).add_flags(delete_bit);
            if (item.flags() & Elem::valid_only_bit) {
                item.clear_read();
                item.clear_flags(Elem::valid_only_bit);
            }
            add_trans_size_offs(-1);
        }
    }
    
    iterator erase(iterator pos) {
        iterator end_it = end();
        for (iterator it = pos; it != (end_it - 1); ++it) {
            *(it) = (T) *(it + 1);
        }
        pop_back();
        return pos;
        
    }
    
    iterator insert(iterator pos, const T& value) {
        iterator end_it = end();
        push_back(*(end_it - 1));
        for (iterator it = (end_it - 1); it != pos; --it) {
            *(it) = (T) *(it - 1);
        }
        
        *pos = value;
        return pos;
    }
    
    size_t transSize() {
        t_item(this).add_read(vecversion_);
        acquire_fence();
        return size_ + trans_size_offs();
    }
    
    bool checkSize(size_type sz) {
        size_type size = size_;
        int32_t offset = trans_size_offs();
        
        int32_t pred = (sz - offset) << pred_shift;
        if (size + offset == sz) {
            pred |= eq_bit;
        } else if (size + offset > sz) {
            pred |= geq_bit;
        } else {
            Sto::abort();
        }
        auto item = Sto::item(this, size_pred_key);
        if (item.has_predicate()) {
            assert(item.has_read());
            int32_t old_pred = item.template read_value<int32_t>();
            // Add the covering predicate of the old predicate and the new one.
            if (old_pred != pred) {
                int32_t oldval = old_pred >> pred_shift;
                int32_t newval = pred >> pred_shift;
                if ((old_pred & pred_mask) == eq_bit) {
                    if ((pred & pred_mask) == eq_bit) {
                        // Contradicting predicates
                        Sto::abort();
                    } else {
                        assert((pred & pred_mask) == geq_bit);
                        if (oldval < newval) {
                            // Again contradicting
                            Sto::abort();
                        }
                        // old predicate is the covering  predicate - so retain it
                    }
                } else {
                    assert((old_pred & pred_mask) == geq_bit);
                    if ((pred & pred_mask) == eq_bit) {
                        if (newval < oldval) {
                            // Contradicting
                            Sto::abort();
                        } else {
                            // new predicate is the covering predicate
                            item.clear_predicate().add_predicate(pred);
                        }
                    } else {
                        assert((pred & pred_mask) == geq_bit);
                        if (newval > oldval) {
                            item.clear_predicate().add_predicate(pred);
                        }
                    }
                }
            }
        } else {
            assert(!item.has_read());
            item.add_predicate(pred);
        }
        return size + offset == sz;
    }
    
    size_t size() const {
        return size_;
    }
    
    T read(key_type i) {
        if (i < size_) {
            return data_[i].read();
        } else {
            return 0;
        }
    }
    
    void write(key_type i, value_type v) {
        assert(i < size_);
        data_[i].write(std::move(v));
    }
    
    wrapper& front() {
        wrapper * item = new wrapper(this, 0); //TODO: need to gc this
        return *item;
    }
    
    value_type transGet(const key_type& i){
        Version ver = vecversion_;
        acquire_fence();
        size_type size = size_;
        acquire_fence();
        if (i >= size + trans_size_offs()) {
            auto vecitem = t_item(this);
            bool aborted = false;
            if (vecitem.has_read()) {
                auto lv = vecversion_;
                if (!TransactionTid::same_version(lv, vecitem.template read_value<Version>())
                    || is_locked(lv)) {
                    aborted = true;
                    Sto::abort();
                }
            }
            if (!aborted) {
                vecitem.add_read(ver);
                throw OutOfBoundsException();
            }
        }
        if (i < size)
            return data_[i].transRead();
        else {
            int diff = i - size;
            
            auto extra_items = Sto::item(this, push_back_key);
            // We need to register the vecversion_ to invalidate other concurrent push_backs.
            t_item(this).add_read(ver);
            if (extra_items.has_write()) {
                if (is_list(extra_items)) {
                    auto& write_list= extra_items.template write_value<std::vector<T>>();
                    if (!write_list.empty()) {
                        return write_list[diff];
                    }
                    else assert(false);
                }
                // not a list, has exactly one element
                else {
                    assert(diff == 0);
                    return extra_items.template write_value<T>();
                }
            }
            assert(false);
        }
    }
    
    void transUpdate(const key_type& i, value_type v) {
        //std::cout << "Writing to " << i << " with value " << v << std::endl;
        Version ver = vecversion_;
        acquire_fence();
        size_type size = size_;
        acquire_fence();
        if (i >= size + trans_size_offs()) {
            bool aborted = false;
            auto vecitem = t_item(this);
            if (vecitem.has_read()) {
                auto lv = vecversion_;
                if (!TransactionTid::same_version(lv, vecitem.template read_value<Version>())
                    || is_locked(lv)) {
                    aborted = true;
                    Sto::abort();
                }
            }
            if (!aborted) {
                vecitem.add_read(ver);
                throw OutOfBoundsException();
            }

        }
        if (i < size)
            data_[i].transWrite(std::move(v));
        else {
            int diff = i - size;
            
            auto extra_items = Sto::item(this, push_back_key);
            // We need to register the vecversion_ to invalidate other concurrent push_backs.
            t_item(this).add_read(ver);
            if (extra_items.has_write()) {
                if (is_list(extra_items)) {
                    auto& write_list= extra_items.template write_value<std::vector<T>>();
                    if (!write_list.empty()) {
                        write_list[diff] = v;
                    }
                    else assert(false);
                }
                // not a list, has exactly one element
                else {
                    assert(diff == 0);
                    extra_items.clear_write();
                    extra_items.add_write(v);
                }
            } else {
                assert(false);
            }
        }
    }
    
    bool is_list(const TransItem& item) {
        return item.flags() & list_bit;
    }
    
    bool has_delete(const TransItem& item) {
        return item.flags() & delete_bit;
    }
    
    void lock_version(Version& v) {
        TransactionTid::lock(v);
    }
    
    void unlock_version(Version& v) {
        TransactionTid::unlock(v);
    }
    
    bool is_locked(Version& v) {
        return TransactionTid::is_locked(v);
    }
    
    void lock(key_type i) {
        while(1) {
            resize_lock_.read_lock();
            bool locked = data_[i].try_lock();
            resize_lock_.read_unlock();
            if (locked) break;
        }
    }
    
    void unlock(key_type i) {
        resize_lock_.read_lock();
        data_[i].unlock();
        resize_lock_.read_unlock();
    }
    
    template <typename PTR>
    TransProxy t_item(PTR *node) {
        // can switch this to fresh_item to not read our writes
        return Sto::item(this, node);
    }
    
    void add_trans_size_offs(int size_offs) {
        // TODO: it would be more efficient to store this directly in Transaction,
        // since the "key" is fixed (rather than having to search the transset each time)
        auto item = Sto::item(this, size_key);
        int cur_offs = 0;
        // XXX: this is sorta ugly
        if (item.has_read()) {
            cur_offs = item.template read_value<int>();
            item.update_read(cur_offs, cur_offs + size_offs);
        } else
            item.add_read(cur_offs + size_offs);
    }
    
    int trans_size_offs() {
        auto item = Sto::item(this, size_key);
        if (item.has_read())
            return item.template read_value<int>();
        return 0;
    }

    void add_lock_vector_item() {
        auto item = t_item((void*)this);
        item.add_write(0);
    }
    
    void readVersion(TransItem& item, Transaction& t) {
        if (item.key<int>() == size_pred_key) {
            auto vec_item = Sto::check_item(this, this);
            auto lv = vecversion_;
            if (is_locked(lv) && !(*vec_item).has_lock()) { Sto::abort(); }
            item.add_read_version(lv, t);
            return;
        }
        assert(false);
    }
    
    bool check(const TransItem& item, const Transaction& trans){
        if (item.key<int>() == size_key) {
            return true;
        }
        if (item.key<int>() == size_pred_key) {
            size_type size = size_;
            acquire_fence();
            
            int32_t pred = item.template read_value<int32_t>();
            int32_t rval = pred >> pred_shift;
            if ((pred & pred_mask) == eq_bit) {
                if (size != rval) return false;
            } else {
                assert((pred & pred_mask) == geq_bit);
                if (size < rval) return false;
            }
            auto& read_version = item.template write_value<Version>();
            if (read_version != vecversion_) { return false; }
            return true;
            
        }
        if (item.key<Vector*>() == this || item.key<int>() == push_back_key) {
            auto lv = vecversion_;
            return TransactionTid::same_version(lv, item.template read_value<Version>())
                && (!is_locked(lv) || item.has_lock(trans));
        }
        key_type i = item.key<key_type>();
        if (item.flags() & Elem::valid_only_bit) {
            if (i >= size_ + trans_size_offs()) {
                return false;
            }
        }
        return data_[i].check(item, trans);
    }
    
    void lock(TransItem& item){
        if (item.key<Vector*>() == this) {
            lock_version(vecversion_); // TODO: no need to lock vecversion_ if trans_size_offs() is 0
        } else if (item.key<int>() == push_back_key) {
            return; // Do nothing as we will anyways lock vecversion for size.
        }
        else {
            lock(item.key<key_type>());
        }
    }
    void unlock(TransItem& item){
        if (item.key<Vector*>() == this) {
            unlock_version(vecversion_);
        }
        else if (item.key<int>() == push_back_key) {
            return;
        }
        else {
            unlock(item.key<key_type>());
        }
    }
    
    void install(TransItem& item, const Transaction& t) {
        //install value
        if (item.key<Vector*>() == this)
            return;
        if (item.key<int>() == push_back_key) {
            // write all the elements
            if (is_list(item)) {
                auto& write_list = item.template write_value<std::vector<T>>();
                for (size_t i = 0; i < write_list.size(); i++) {
                    if (size_ >= capacity_) {
                        // Need to resize
                        int new_capacity = (capacity_  == 0) ? 1 : capacity_ << 1;
                        reserve(new_capacity);
                    }
                    resize_lock_.read_lock();
                    data_[size_].write(write_list[i]);
                    acquire_fence();
                    size_++;
                    resize_lock_.read_unlock();
                }
            }
            else {
                auto& val = item.template write_value<T>();
                if (size_ >= capacity_) {
                    // Need to resize
                    int new_capacity = (capacity_  == 0) ? 1 : capacity_ << 1;
                    reserve(new_capacity);
                }
                resize_lock_.read_lock();
                data_[size_].write(val);
                acquire_fence();
                size_++;
                resize_lock_.read_unlock();
            }
            
            if (Opacity) {
                TransactionTid::set_version(vecversion_, t.commit_tid());
            } else {
                TransactionTid::inc_invalid_version(vecversion_);
            }
        } else {
            auto index = item.key<key_type>();
            if (has_delete(item)) {
                if (index < size_) {
                    size_ = index;
                
                    if (Opacity) {
                        TransactionTid::set_version(vecversion_, t.commit_tid());
                    } else {
                        TransactionTid::inc_invalid_version(vecversion_);
                    }
                }
            } else if (index >= size_) {
                assert(false);
            }
            
            resize_lock_.read_lock();
            data_[index].install(item, t);
            resize_lock_.read_unlock();
            
        }
    }
    
    iterator begin() { return iterator(this, 0, false); }
    iterator end() {
        //t_item(this).add_read(vecversion_);// to invalidate size changes after this call.
        //acquire_fence();
        return iterator(this, 0, true);
    }
    
    
    // Extra methods used by concurrent.cc
    value_type transRead(const key_type& i){
        if (i >= size_ + trans_size_offs()) { // TODO: this isn't totally right
            return 0;
        } else {
            return transGet(i);
        }
    }
    
    void transWrite(const key_type& i, value_type v) {
        if (i >= size_ + trans_size_offs()) {
            return push_back(v);
        } else {
            return transUpdate(i, v);
        }
    }
    
    // This is not-transactional and only used for debugging purposes
    void print() {
        for (int i = 0; i < size_ + trans_size_offs(); i++) {
            std::cout << transRead(i) << " ";
        }
        std::cout << std::endl;
    }

    wrapper& get_it_obj(int idx) {
        assert(idx < IT_SIZE);
        return it_objs[idx];
    }
    
private:
    size_type size_;
    size_type capacity_;
    Version vecversion_; // for vector size
    rwlock resize_lock_; // to do concurrent resize
    Elem* data_;
    wrapper* it_objs;
};

    
template<typename T, bool Opacity, typename Elem>
struct T_wrapper {
    void initialize(Vector<T, Opacity, Elem> * arr, int idx) {
       arr_ = arr;
       idx_ = idx;
    }
    
    operator T() {
        return arr_->transGet(idx_);
    }
    
    T_wrapper& operator= (const T& v) {
        arr_->transUpdate(idx_, v);
        return *this;
    }
    
    T_wrapper& operator= (T_wrapper& v) {
        arr_->transUpdate(idx_, (T) v);
        return *this;
    }

private:
    Vector<T, Opacity, Elem> * arr_;
    int idx_;
};


template<typename T, bool Opacity, typename Elem>
class VecIterator : public std::iterator<std::random_access_iterator_tag, T> {
public:
    typedef T value_type;
    typedef T_wrapper<T, Opacity, Elem> wrapper;
    typedef VecIterator<T, Opacity, Elem> iterator;
    VecIterator(Vector<T, Opacity, Elem> * arr, int ptr, bool endy) : myArr(arr), myPtr(ptr), endy(endy) { }
    VecIterator(const VecIterator& itr) : myArr(itr.myArr), myPtr(itr.myPtr), endy(itr.endy) {}
    
    VecIterator& operator= (const VecIterator& v) {
        myArr = v.myArr;
        myPtr = v.myPtr;
        endy = v.endy;
        return *this;
    }
    
    bool operator==(iterator other) const {
        if (myArr != other.myArr) return false;
        if (endy == other.endy) {
            return myPtr == other.myPtr;
        } else {
            size_t sz = endy ? other.myPtr - myPtr : myPtr - other.myPtr;
            return myArr->checkSize(sz);
        }
    }
    
    bool operator!=(iterator other) const {
        return !(operator==(other));
    }
    
    wrapper& operator*() {
        int idx = endy ? myArr->transSize() + myPtr : myPtr;
        wrapper& item = myArr->get_it_obj(idx);
        return item;
    }
    
    wrapper& operator[](const int& n) {
        int idx = endy ? myArr->transSize() + myPtr : myPtr;
        wrapper& item = myArr->get_it_obj( idx + n);
        return item;
    }
    
    /* This is the prefix case */
    iterator& operator++() { ++myPtr; return *this; }
    
    /* This is the postfix case */
    iterator operator++(int) {
        VecIterator<T, Opacity, Elem> clone(*this);
        ++myPtr;
        return clone;
    }
    
    iterator& operator--() { --myPtr; return *this; }
    
    iterator operator--(int) {
        VecIterator<T, Opacity, Elem> clone(*this);
        --myPtr;
        return clone;
    }
    
    iterator operator+(int i) {
        VecIterator<T, Opacity, Elem> clone(*this);
        clone.myPtr += i;
        return clone;
    }
    
    iterator operator-(int i) {
        VecIterator<T, Opacity, Elem> clone(*this);
        clone.myPtr -= i;
        return clone;
    }
    
    int operator-(const iterator& rhs) {
        assert(rhs.myArr == myArr);
        if (endy == rhs.endy)
            return (myPtr - rhs.myPtr);
        else {
            size_t size = myArr->transSize();
            if (endy)
                return size + myPtr - rhs.myPtr;
            else
                return myPtr - rhs.myPtr - size;
        }
    }
        
private:
    Vector<T, Opacity, Elem> * myArr;
    size_t myPtr;
    bool endy;
};

