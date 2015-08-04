#pragma once

#include "config.h"
#include "compiler.hh"
#include <iostream>
#include <vector>
#include "Transaction.hh"
#include "Box.hh"
#include "VersionFunctions.hh"

#define log2(x) ceil(log((double) size) / log(2.0))

template<typename T, bool Opacity = false, typename Elem = Box<T>> class VecIterator;

template <typename T, bool Opacity = false, typename Elem = Box<T>>
class Vector : public Shared {
    friend class VecIterator<T, Opacity, Elem>;
    typedef VecIterator<T, Opacity, Elem> iterator;
    
    typedef TransactionTid::type Version;
    typedef VersionFunctions<Version> Versioning;
    
    static constexpr void* size_key = (void*)0;
    static constexpr TransItem::flags_type list_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type empty_bit = TransItem::user0_bit<<1;
public:
    typedef unsigned key_type;
    typedef T value_type;
    
    Vector() {
        capacity_ = 0;
        size_ = 0;
        vecversion_ = 0;
        data_ = NULL;
    }
    
    Vector(uint32_t size) {
        size_ = size;
        capacity_ = 1 << ((int) log2(size));
        vecversion_ = 0;
        data_ = new T[capacity_];
    }
    
    void reserve(uint32_t new_capacity, bool lock) {
        if (new_capacity <= capacity_)
            return;
        Elem * new_data = new Elem[new_capacity];
        
        if(lock) lock_version(vecversion_);
        for (uint32_t i = 0; i < capacity_; i++) {
            new_data[i] = data_[i];
        }
        for (uint32_t i = size_; i < new_capacity; i++) {
            new_data[i].initialize(this, i);
        }
        capacity_ = new_capacity;
        Transaction::rcu_free(data_);
        data_ = new_data;
        if(lock) unlock_version(vecversion_);
    }
    
    void push_back(const value_type& v) {
        auto item = Sto::item(this, -1);
        if (item.has_write()) {
            if (!is_list(item)) {
                auto& val = item.template write_value<T>();
                std::vector<T> write_list;
                if (!is_empty(item)) {
                    write_list.push_back(val);
                    item.clear_flags(empty_bit);
                }
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
        }
        add_lock_vector_item();
        add_trans_size_offs(1);
    }
    
    size_t transSize() {
        t_item(this).add_read(vecversion_);
        acquire_fence();
        return size_ + trans_size_offs();
    }
    
    size_t size() const {
        return size_;
    }
    
    T read(key_type i) {
        assert(i < size_);
        return data_[i].read();
    }
    
    void write(key_type i, value_type v) {
        assert(i < size_);
        data_[i].write(std::move(v));
    }
    
    value_type transRead(const key_type& i){
        uint32_t size = size_;
        fence();
        if (i < size)
            return data_[i].transRead();
        else {
            assert(i < size + trans_size_offs());
            int diff = i - size;
            Version ver = vecversion_;
            fence();
            auto extra_items = Sto::item(this,-1);
            if (!extra_items.has_read()) {
                // We need to register the vecversion_ to invalidate other concurrent push_backs.
                extra_items.add_read(ver);
            }
            if (extra_items.has_write()) {
                if (is_list(extra_items)) {
                    auto& write_list= extra_items.template write_value<std::vector<T>>();
                    if (!write_list.empty()) {
                        return write_list[diff];
                    }
                    else assert(false);
                }
                // not a list, has exactly one element
                else if (!is_empty(extra_items)) {
                    assert(diff == 0);
                    return extra_items.template write_value<T>();
                }
                else assert(false);
            }
            assert(false);
        }
    }
    
    void transWrite(const key_type& i, value_type v) {
        uint32_t size = size_;
        fence();
        if (i < size)
            data_[i].transWrite(std::move(v));
        else {
            assert(i < size + trans_size_offs());
            int diff = i - size;
            Version ver = vecversion_;
            fence();
            auto extra_items = Sto::item(this,-1);
            if (!extra_items.has_read()) {
                // We need to register the vecversion_ to invalidate other concurrent push_backs.
                extra_items.add_read(ver);
            }
            if (extra_items.has_write()) {
                if (is_list(extra_items)) {
                    auto& write_list= extra_items.template write_value<std::vector<T>>();
                    if (!write_list.empty()) {
                        write_list[diff] = v;
                    }
                    else assert(false);
                }
                // not a list, has exactly one element
                else if (!is_empty(extra_items)) {
                    assert(diff == 0);
                    extra_items.clear_write();
                    extra_items.add_write(v);
                }
                else assert(false);
            } else assert(false);
        }
    }
    
    bool is_list(const TransItem& item) {
        return item.flags() & list_bit;
    }
    
    bool is_empty(const TransItem& item) {
        return item.flags() & empty_bit;
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
        data_[i].lock();
    }
    
    void unlock(key_type i) {
        data_[i].unlock();
    }
    
    template <typename PTR>
    TransProxy t_item(PTR *node) {
        // can switch this to fresh_item to not read our writes
        return Sto::item(this, node);
    }
    
    void add_trans_size_offs(int size_offs) {
        // TODO: it would be more efficient to store this directly in Transaction,
        // since the "key" is fixed (rather than having to search the transset each time)
        auto item = t_item(size_key);
        int cur_offs = 0;
        // XXX: this is sorta ugly
        if (item.has_read()) {
            cur_offs = item.template read_value<int>();
            item.update_read(cur_offs, cur_offs + size_offs);
        } else
            item.add_read(cur_offs + size_offs);
    }
    
    int trans_size_offs() {
        auto item = t_item(size_key);
        if (item.has_read())
            return item.template read_value<int>();
        return 0;
    }

    void add_lock_vector_item() {
        auto item = t_item((void*)this);
        item.add_write(0);
    }
    
    bool check(const TransItem& item, const Transaction& trans){
        if (item.key<void*>() == size_key) {
            return true;
        }
        if (item.key<Vector*>() == this || item.key<int>() == -1) {
            auto lv = vecversion_;
            return TransactionTid::same_version(lv, item.template read_value<Version>())
                && (!is_locked(lv) || item.has_lock(trans));
        }
        key_type i = item.key<key_type>();
        return data_[i].check(item, trans);
    }
    
    void lock(TransItem& item){
        if (item.key<Vector*>() == this)
            lock_version(vecversion_);
        else if (item.key<int>() == -1)
            return; // Do nothing as we will anyways lock vecversion for size.
        else
            lock(item.key<key_type>());
    }
    void unlock(TransItem& item){
        if (item.key<Vector*>() == this)
            unlock_version(vecversion_);
        else if (item.key<int>() == -1)
            return;
        else
            unlock(item.key<key_type>());
    }
    
    void install(TransItem& item, const Transaction& t) {
        //install value
        if (item.key<Vector*>() == this)
            return;
        if (item.key<int>() == -1) {
            // write all the elements
            if (is_list(item)) {
                auto& write_list = item.template write_value<std::vector<T>>();
                for (int i = 0; i < write_list.size(); i++) {
                    if (size_ >= capacity_) {
                        // Need to resize
                        int new_capacity = (capacity_  == 0) ? 1 : capacity_ << 1;
                        reserve(new_capacity, false); // Don't lock vecversion_ since it is already locked
                    }
                    data_[size_++].write(write_list[i]);
                }
            }
            else if (!is_empty(item)) {
                auto& val = item.template write_value<T>();
                if (size_ >= capacity_) {
                    // Need to resize
                    int new_capacity = (capacity_  == 0) ? 1 : capacity_ << 1;
                    reserve(new_capacity, false); // Don't lock vecversion_ since it is already locked
                }
                data_[size_++].write(val);
            }
            
            if (Opacity) {
                TransactionTid::set_version(vecversion_, t.commit_tid());
            } else {
                TransactionTid::inc_invalid_version(vecversion_);
            }
        } else {
            data_[item.key<key_type>()].install(item, t);
        }
    }
    
    iterator begin() { return iterator(this, 0); }
    iterator end() {
        add_lock_vector_item(); // to invalidate size changes after this call.
        return iterator(this, size_ + trans_size_offs());
    }
    
private:
    uint32_t size_;
    uint32_t capacity_;
    Version vecversion_; // for vector size
    Elem* data_;
};

// TODO: iterator should also deal with objects from push_backs within the same transaction
template<typename T, bool Opacity, typename Elem>
class VecIterator : public std::iterator<std::forward_iterator_tag, T> {
public:
    typedef T value_type;
    VecIterator(Vector<T, Opacity, Elem> * arr, int ptr) : myArr(arr), myPtr(ptr) { }
    VecIterator(const VecIterator& itr) : myArr(itr.myArr), myPtr(itr.myPtr) {}
    
    VecIterator& operator= (const VecIterator& v) {
        myArr = v.myArr;
        myPtr = v.myPtr;
        return *this;
    }
    
    bool operator==(VecIterator<T, Opacity, Elem> other) const {
        return (myArr == other.myArr) && (myPtr == other.myPtr);
    }
    
    bool operator!=(VecIterator<T, Opacity, Elem> other) const {
        return !(operator==(other));
    }
    
    Elem& operator*() {
        return myArr->data_[myPtr];
    }
    
    /* This is the prefix case */
    VecIterator<T, Opacity, Elem>& operator++() { ++myPtr; return *this; }
    
    /* This is the postfix case */
    VecIterator<T, Opacity, Elem> operator++(int) {
        VecIterator<T, Opacity, Elem> clone(*this);
        ++myPtr;
        return clone;
    }
    
private:
    Vector<T, Opacity, Elem> * myArr;
    int myPtr;
};

