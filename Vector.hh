#pragma once

#include "config.h"
#include "compiler.hh"
#include <iostream>
#include <vector>
#include <map>
#include "Transaction.hh"
#include "TArrayProxy.hh"
#include "Box.hh"
#include "VersionFunctions.hh"
#include "rwlock.hh"

#define IT_SIZE 10000
#define log2(x) ceil(log((double) size) / log(2.0))

class OutOfBoundsException {};

template<typename T, bool Opacity = false, typename Elem = Box<T>> class Vector;
template<typename T, bool Opacity = false, typename Elem = Box<T>> class VecIterator;


template <typename T, bool Opacity, typename Elem>
class Vector : public Shared {
public:
    typedef unsigned index_type;

private:
    friend class VecIterator<T, Opacity, Elem>;
    typedef const Vector<T, Opacity, Elem> const_vector_type;
    typedef Vector<T, Opacity, Elem> vector_type;

    typedef TransactionTid::type Version;
    typedef VersionFunctions<Version> Versioning;

    static constexpr int vector_key = -1;
    static constexpr int push_back_key = -2;
    static constexpr int size_key = -3;
    static constexpr int size_pred_key = -4;
    static constexpr TransItem::flags_type list_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit<<1;
    static constexpr int32_t value_shift = 1;
    static constexpr int32_t geq_mask = 1;
public:
    typedef int key_type;
    typedef T value_type;
    typedef value_type get_type;
    
    typedef VecIterator<T, Opacity, Elem> iterator;
    typedef const VecIterator<T, Opacity, Elem> const_iterator;
    typedef TArrayProxy<Vector<T, Opacity, Elem>> proxy_type;
    typedef TConstArrayProxy<Vector<T, Opacity, Elem>> const_proxy_type;
    typedef int32_t size_type;

    
    Vector(): resize_lock_() {
        capacity_ = 0;
        size_ = 0;
        vecversion_ = 0;
        data_ = NULL;
    }
    
    Vector(size_type size): resize_lock_() {
        size_ = 0;
        capacity_ = 1 << ((int) log2(size));
        vecversion_ = 0;

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
        auto vecitem = add_vector_version(vecversion_);
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
    
    size_type size() {
        add_vector_version(vecversion_);
        acquire_fence();
        return size_ + trans_size_offs();
    }
    
    bool checkSize(size_type sz) {
        size_type size = size_;
        int32_t offset = trans_size_offs();
        
        int32_t pred = (sz - offset) << value_shift;
        if (size + offset == sz) {
        } else if (size + offset > sz) {
            pred |= geq_mask;
        } else {
            Sto::abort();
        }
        auto item = Sto::item(this, size_pred_key);
        if (item.has_predicate()) {
            int32_t old_pred = item.template predicate_value<int32_t>();
            // Add the covering predicate of the old predicate and the new one.
            if (pred == old_pred || (old_pred & pred & geq_mask))
                pred = pred >= old_pred ? pred : old_pred;
            else if ((old_pred & geq_mask) && old_pred <= (pred | geq_mask))
                /* OK */;
            else if ((pred & geq_mask) && pred <= (old_pred | geq_mask))
                pred = old_pred;
            else
                Sto::abort();
        }
        item.set_predicate(pred);
        return size + offset == sz;
    }

    size_type unsafe_size() const {
        return size_;
    }
    T unsafe_get(key_type i) const {
        assert(i < size_);
        return data_[i].unsafe_read();
    }

    proxy_type front() {
        if (size_ + trans_size_offs() == 0)
            Sto::abort();
        return proxy_type(this, 0);
    }

    proxy_type back() {
        size_t sz = size();
        if (sz == 0)
            Sto::abort();
        return proxy_type(this, sz - 1);
    }

    const_proxy_type operator[](key_type i) const {
        size_type sz = size_ + trans_size_offs();
        if (i >= sz)
            Sto::abort();
        return const_proxy_type(this, i);
    }
    proxy_type operator[](key_type i) {
        size_type sz = size_ + trans_size_offs();
        if (i >= sz)
            Sto::abort();
        return proxy_type(this, i);
    }

    value_type transGet(const key_type& i) const {
        Version ver = vecversion_;
        acquire_fence();
        size_type size = size_;
        acquire_fence();
        if (i >= size + trans_size_offs()) {
            auto vecitem = vector_item();
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
            add_vector_version(vecversion_);
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
            auto vecitem = vector_item();
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
            add_vector_version(ver);
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
    
    static bool is_list(const TransItem& item) {
        return item.flags() & list_bit;
    }
    
    static bool has_delete(const TransItem& item) {
        return item.flags() & delete_bit;
    }
    
    void lock_version(Version& v) {
        TransactionTid::lock(v);
    }
    
    void unlock_version(Version& v) {
        TransactionTid::unlock(v);
    }
    
    bool is_locked(Version& v) const {
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

    void add_trans_size_offs(int size_offs) {
        // TODO: it would be more efficient to store this directly in Transaction,
        // since the "key" is fixed (rather than having to search the transset each time)
        auto item = Sto::item(this, size_key);
        item.set_stash(item.template stash_value<int>(0) + size_offs);
    }

    int trans_size_offs() const {
        return Sto::item(const_cast<Vector<T, Opacity, Elem>*>(this), size_key).template stash_value<int>(0);
    }

    TransProxy vector_item() const {
        // can switch this to fresh_item to not read our writes
        return Sto::item(this, vector_key);
    }

    void add_lock_vector_item() {
        vector_item().add_write(0);
    }

    TransProxy add_vector_version(Version ver) const {
        return vector_item().add_read(ver);
    }

    bool check_predicate(TransItem& item, Transaction&) {
        assert(item.key<int>() == size_pred_key);
        auto lv = vecversion_;
        if (is_locked(lv))
            return false;
        vector_item().add_read(lv);
        acquire_fence();
        auto size = size_;
        int32_t pred = item.template predicate_value<int32_t>();
        int32_t pred_value = pred >> value_shift;
        if (pred & geq_mask)
            return size >= pred_value;
        else
            return size == pred_value;
    }

    bool check(const TransItem& item, const Transaction& trans){
        if (item.key<int>() == vector_key || item.key<int>() == push_back_key) {
            auto lv = vecversion_;
            return TransactionTid::same_version(lv, item.template read_value<Version>())
                && (!is_locked(lv) || item.has_lock(trans));
        }
        key_type i = item.key<key_type>();
        assert(i >= 0);
        if (item.flags() & Elem::valid_only_bit) {
            if (i >= size_ + trans_size_offs()) {
                return false;
            }
        }
        return data_[i].check(item, trans);
    }
    
    bool lock(TransItem& item){
        if (item.key<int>() == vector_key) {
            lock_version(vecversion_); // TODO: no need to lock vecversion_ if trans_size_offs() is 0
        } else if (item.key<int>() != push_back_key) {
            lock(item.key<key_type>());
        }
        return true;
    }

    void install(TransItem& item, const Transaction& t) {
        //install value
        if (item.key<int>() == vector_key)
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

    void unlock(TransItem& item) {
        if (item.key<int>() == vector_key)
            unlock_version(vecversion_);
        else if (item.key<int>() != push_back_key)
            unlock(item.key<key_type>());
    }

    void print(FILE* f, const TransItem& item) const {
        if (item.key<int>() == size_pred_key) {
            fprintf(f, "<%p.size%s%d>", this, item.predicate_value<int32_t>() & geq_mask ? ">=" : "==", item.predicate_value<int32_t>() >> value_shift);
        } else {
            if (item.key<int>() == size_key)
                fprintf(f, "<%p.size", this);
            else if (item.key<int>() == vector_key)
                fprintf(f, "<%p", this);
            else
                fprintf(f, "<%p.%d", this, item.key<int>());
            if (item.has_read())
                fprintf(f, " r%" PRIu64, item.read_value<Version>());
            if (item.has_write())
                fprintf(f, " w%p", item.write_value<void*>());
            fprintf(f, ">");
        }
    }

    iterator begin() {
        return iterator(this, 0, false);
    }
    iterator end() {
        //vector_item().add_read(vecversion_);// to invalidate size changes after this call.
        //acquire_fence();
        return iterator(this, 0, true);
    }

    // This is not-transactional and only used for debugging purposes
    void print() {
        for (int i = 0; i < size_ + trans_size_offs(); i++) {
            std::cout << unsafe_get(i) << " ";
        }
        std::cout << std::endl;
    }
    
private:
    size_type size_;
    size_type capacity_;
    Version vecversion_; // for vector size
    rwlock resize_lock_; // to do concurrent resize
    Elem* data_;
};


template<typename T, bool Opacity, typename Elem>
class VecIterator : public std::iterator<std::random_access_iterator_tag, T> {
public:
    typedef T value_type;
    typedef Vector<T, Opacity, Elem> vector_type;
    typedef typename vector_type::proxy_type proxy_type;
    typedef VecIterator<T, Opacity, Elem> iterator;
    VecIterator() = default;
    VecIterator(Vector<T, Opacity, Elem> * arr, int ptr, bool endy) : myArr(arr), myPtr(ptr), endy(endy) { }
    VecIterator(const VecIterator& itr) : myArr(itr.myArr), myPtr(itr.myPtr), endy(itr.endy) {}
    
    VecIterator& operator= (const VecIterator& v) {
        myArr = v.myArr;
        myPtr = v.myPtr;
        endy = v.endy;
        return *this;
    }
    
    bool operator==(iterator other) const {
        if (myArr != other.myArr)
            return false;
        else if (endy == other.endy)
            return myPtr == other.myPtr;
        else {
            size_t sz = endy ? other.myPtr - myPtr : myPtr - other.myPtr;
            return myArr->checkSize(sz);
        }
    }
    
    bool operator!=(iterator other) const {
        return !(operator==(other));
    }
    
    proxy_type operator*() {
        return proxy_type(myArr, endy ? myArr->size() + myPtr : myPtr);
    }

    proxy_type operator[](int delta) {
        return proxy_type(myArr, (endy ? myArr->size() + myPtr : myPtr) + delta);
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
            size_t size = myArr->size();
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
