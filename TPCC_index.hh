#pragma once
#include "config.h"
#include "compiler.hh"

#include "Transaction.hh"
#include "TWrapped.hh"
#include "print_value.hh"

#include <vector>

namespace tpcc {

// unordered index implemented as hashtable
template <typename K, typename V,
          bool Opacity = true, bool Adaptive = false, bool ReadMyWrite = false,
          unsigned Init_size = 129,
          typename W = V, typename Hash = std::hash<K>, typename Pred = std::equal_to<K>>
class unordered_index : public TObject {
public:
    typedef K key_type;
    typedef W value_type;

    typedef typename std::conditional<Opacity, TVersion, TNonopaqueVersion>::type bucket_version_type;
    typedef typename std::conditional<Adaptive, TLockVersion, bucket_version_type>::type version_type;
    
    static constexpr typename version_type::type invalid_bit = TransactionTid::user_bit;

    static constexpr bool index_read_my_write = ReadMyWrite;

private:
    // our hashtable is an array of linked lists. 
    // an internal_elem is the node type for these linked lists
    struct internal_elem {
        internal_elem *next;
        key_type key;
        version_type version;
        value_type value;
        bool deleted;

        internal_elem(const key_type& k, const value_type& val, bool mark_valid)
            : key(k), next(nullptr),
              version(Sto::initialized_tid() | (mark_valid ? 0 : invalid_bit)),
              value(val), deleted(false) {}

        bool valid() const {
            return !(version.value() & invalid_bit);
        }
    };

    struct bucket_entry {
        internal_elem *head;
        // this is the bucket version number, which is incremented on insert
        // we use it to make sure that an unsuccessful key lookup will still be
        // unsuccessful at commit time (because this will always be true if no
        // new inserts have occurred in this bucket)
        bucket_version_type version;
        bucket_entry() : head(nullptr), version(0) {}
    };

    typedef std::vector<bucket_entry> MapType;
    // this is the hashtable itself, an array of bucket_entry's
    MapType map_;
    Hash hasher_;
    Pred pred_;

    // used to mark whether a key is a bucket (for bucket version checks)
    // or a pointer (which will always have the lower 3 bits as 0)
    static constexpr uintptr_t bucket_bit = 1U<<0;

    static constexpr TransItem::flags_type insert_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit<<1;

public:
    typedef std::tuple<bool, bool, const value_type*> sel_return_type;
    typedef std::tuple<bool, bool>                    ins_return_type;
    typedef std::tuple<bool, bool>                    del_return_type;

    unordered_index(size_t size = Init_size, Hash h = Hash(), Pred p = Pred()) :
            map_(), hasher_(h), pred_(p) {
        map_.resize(size);
    }

    inline size_t hash(const key_type& k) {
        return hasher_(k);
    }
    inline size_t nbuckets() const {
        return map_.size();
    }
    inline size_t find_bucket_idx(const key_type& k) const {
        return hash(k) % nbuckets();
    }

    // returns (success : bool, found : bool, row_ptr : const internal_elem *)
    // will need to row back transaction if success == false
    sel_return_type
    select_row(const key_type& k, bool for_update = false) {
        bucket_entry& buck = map_[find_bucket_idx(k)];
        bucket_version_type buck_vers = buck.version;
        fence();
        internal_elem *e = find_in_bucket(buck, k);

        if (e) {
            // if found, return pointer to the row
            auto item = Sto::item(this, e);
            if (is_phantom(e, item))
                goto abort;

            if (index_read_my_write) {
                if (has_delete(item))
                    return sel_return_type(true, false, nullptr);
                if (item.has_write())
                    return sel_return_type(true, true, item.template write_value<internal_elem *>());
            }

            if (for_update) {
                if (!select_for_update(item, e->version))
                    goto abort;
            } else {
                if (!item.observe(e->version))
                    goto abort;
            }

            return sel_return_type(true, true, e);
        } else {
            // if not found, observe bucket version
            bool ok = Sto::item(this, make_bucket_key(&buck)).observe(buck_vers);
            if (!ok)
                goto abort;
            return sel_return_type(true, false, nullptr);
        }

    abort:
        return sel_return_type(false, false, nullptr);
    }

    // this method is only to be used after calling select_row() with for_update set to true
    // otherwise behavior is undefined
    // update_row() takes ownership of the row pointer (new_row) passed in, and the row to be updated (table_row)
    // should never be modified by the transaction user
    // the new_row pointer stays valid for the rest of the duration of the transaction and the associated
    // temporary row WILL NOT be deallocated until commit/abort time
    void update_row(const internal_elem *table_row, value_type *new_row) {
        auto item = Sto::item(this, const_cast<internal_elem *>(table_row));
        assert(item.has_write() && !has_insert(item));
        item.add_write(new_row);
    }

    // returns (success : bool, found : bool)
    // insert_row() takes ownership of the row pointer (vptr) passed in
    // the pointer stays valid for the rest of the duration of the transaction and the associated temporary row
    // WILL NOT be deallocated until commit/abort time
    ins_return_type
    insert_row(const key_type& k, value_type *vptr, bool overwrite) {
        bucket_entry& buck = map_[find_bucket_idx(k)];

        simple_lock(buck.version);
        internal_elem *e = find_in_bucket(buck, k);

        if (e) {
            simple_unlock(buck.version);
            auto item = Sto::item(this, e);
            if (is_phantom(e, item))
                goto abort;

            if (index_read_my_write) {
                if (has_delete(item)) {
                    item.clear_flags(delete_bit).clear_write().template add_write<value_type *>(vptr);
                    return ins_return_type(true, false);
                }
            }

            // cope with concurrent deletion
            //if (!item.observe(e->version))
            //    goto abort;

            if (overwrite) {
                if (!select_for_overwrite(item, e->version, vptr))
                    goto abort;
                if (index_read_my_write) {
                    if (has_insert(item)) {
                        copy_row(e, vptr);
                    }
                }
            } else {
                if (!item.observe(e->version))
                    goto abort;
            }

            return ins_return_type(true, true);
        } else {
            // insert the new row to the table and take note of bucket version changes
            bucket_version_type buck_vers_0 = buck.version.unlocked();
            insert_in_bucket(buck, k, *vptr, false);
            internal_elem *new_head = buck.head;
            bucket_version_type buck_vers_1 = buck.version.unlocked();

            simple_unlock(buck.version);

            // update bucket version in the read set (if any) since it's changed by ourselves
            auto bucket_item = Sto::item(this, make_bucket_key(&buck));
            if (bucket_item.has_read())
                bucket_item.update_read(buck_vers_0, buck_vers_1);

            auto item = Sto::item(this, new_head);
            // XXX adding write is probably unnecessary, am I right?
            item.template add_write<value_type *>(vptr);
            item.add_flags(insert_bit);

            return ins_return_type(true, false);
        }

    abort:
        return ins_return_type(false, false);
    }

    // returns (success : bool, found : bool)
    // for rows that are not inserted by this transaction, the actual delete doesn't take place
    // until commit time
    del_return_type
    delete_row(const key_type& k) {
        bucket_entry& buck = map_[find_bucket_idx(k)];
        bucket_version_type buck_vers = buck.version;
        fence();

        internal_elem *e = find_in_bucket(buck, k);
        if (e) {
            auto item = Sto::item(this, e);
            bool valid = e->valid();
            if (is_phantom(e, item))
                goto abort;
            if (index_read_my_write) {
                if (!valid && has_insert(item)) {
                    // deleting something we inserted
                    _remove(e);
                    item.remove_read().remove_write().clear_flags(insert_bit | delete_bit);
                    Sto::item(this, make_bucket_key(&buck)).observe(buck_vers);
                    return del_return_type(true, true);
                }
                assert(valid);
                if (has_delete(item))
                    return del_return_type(true, false);
            }
            // select_for_update() will automatically add an observation for OCC version types
            // at commit/check time we check for the special "deleted" field to see if the row
            // is still available for deletion
            if (!select_for_update(item, e->version))
                goto abort;
            //fence();
            //if (e->deleted)
            //    goto abort;
            item.add_flags(delete_bit);

            return del_return_type(true, true);
        } else {
            // not found -- add observation of bucket version
            bool ok = Sto::item(this, make_bucket_key(&buck)).observe(buck_vers);
            if (!ok)
                goto abort;
            return del_return_type(true, false);
        }

    abort:
        return del_return_type(false, false);
    }

    // TObject interface methods
    bool lock(TransItem& item, Transaction& txn) override {
        assert(!is_bucket(item));
        internal_elem *el = item.key<internal_elem *>();
        return txn.try_lock(item, el->version);
    }

    bool check(TransItem& item, Transaction&) override {
        if (is_bucket(item)) {
            bucket_entry &buck = *bucket_address(item);
            return buck.version.check_version(item.read_value<bucket_version_type>());
        } else {
            internal_elem *el = item.key<internal_elem *>();
            version_type rv = item.read_value<version_type>();
            return el->version.check_version(rv);
        }
    }

    void install(TransItem& item, Transaction& txn) override {
        assert(!is_bucket(item));
        internal_elem *el = item.key<internal_elem*>();
        if (has_delete(item)) {
            el->version.set_version_locked(el->version.value() + version_type::increment_value);
            el->deleted = true;
            fence();
            return;
        }
        if (!has_insert(item)) {
            // update
            copy_row(el, item.write_value<const value_type *>());
        }
        el->version.set_version(txn.commit_tid());

        if (Opacity && has_insert(item)) {
            bucket_entry& buck = map_[find_bucket_idx(el->key)];
            lock(buck.version);
            if (buck.version.value() & TransactionTid::nonopaque_bit)
                buck.version.set_version(txn.commit_tid());
            unlock(buck.version);
        }
    }

    void unlock(TransItem& item) override {
        assert(!is_bucket(item));
        internal_elem *el = item.key<internal_elem *>();
        unlock(el->version);
    }

    void cleanup(TransItem& item, bool committed) override {
        if (committed ? has_delete(item) : has_insert(item)) {
            assert(!is_bucket(item));
            internal_elem *el = item.key<internal_elem *>();
            assert(!el->valid() || el->deleted);
            _remove(el);
        }
    }

    // non-transactional methods
    value_type* nontrans_get(const key_type& k);
    void nontrans_put(const key_type& k, const value_type& v);

private:
    // remove a k-v node during transactions (with locks)
    void _remove(internal_elem *el) {
        bucket_entry& buck = map_[find_bucket_idx(el->key)];
        lock(buck.version);
        internal_elem *prev = nullptr;
        internal_elem *curr = buck.head;
        while (curr != nullptr && curr != el) {
            prev = curr;
            curr = curr->next;
        }
        assert(curr);
        if (prev != nullptr)
            prev->next = curr->next;
        else
            buck.head = curr->next;
        unlock(buck.version);
        Transaction::rcu_delete(curr);
    }
    // non-transactional remove by key
    bool remove(const key_type& k) {
        bucket_entry& buck = map_[find_bucket_idx(k)];
        lock(buck.version);
        internal_elem *prev = nullptr;
        internal_elem *curr = buck.head;
        while (curr != nullptr && !pred(curr->key, k)) {
            prev = curr;
            curr = curr->next;
        }
        if (curr == nullptr) {
            unlock(buck.version);
            return false;
        }
        if (prev != nullptr)
            prev->next = curr->next;
        else
            buck.head = curr->next;
        unlock(buck.version);
        return true;
    }
    // insert a k-v node to a bucket
    void insert_in_bucket(bucket_entry& buck, const key_type& k, const value_type& v, bool valid) {
        assert(is_locked(buck.version));

        internal_elem *new_head = new internal_elem(k, v, valid);
        internal_elem *curr_head = buck.head;

        new_head->next = curr_head;
        buck.head = new_head;

        buck.version.inc_nonopaque_version();
    }
    // find a key's k-v node (internal_elem) within a bucket
    internal_elem *find_in_bucket(const bucket_entry& buck, const key_type& k) {
        internal_elem *curr = buck.head;
        while (curr && !pred_(curr->key, k))
            curr = curr->next;
        return curr;
    }

    static bool has_delete(const TransItem& item) {
        return item.flags() & delete_bit;
    }
    static bool has_insert(const TransItem& item) {
        return item.flags() & insert_bit;
    }
    static bool is_phantom(internal_elem *e, const TransItem& item) {
        return (!e->valid() && !has_insert(item));
    }

    // TransItem keys
    static bool is_bucket(const TransItem& item) {
        return item.key<uintptr_t>() & bucket_bit;
    }
    static uintptr_t make_bucket_key(const bucket_entry& bucket) {
        return (reinterpret_cast<uintptr_t>(&bucket) | bucket_bit);
    }
    static bucket_entry *bucket_address(uintptr_t bucket_key) {
        return reinterpret_cast<bucket_entry*>(bucket_key & ~bucket_bit);
    }

    // new select_for_update methods (optionally) acquiring locks
    static bool select_for_update(TransProxy& item, TLockVersion& vers) {
        return item.acquire_write(vers);
    }
    static bool select_for_update(TransProxy& item, TVersion& vers) {
        TVersion v = vers;
        fence();
        if (!item.observe(v))
            return false;
        item.add_write();
        return true;
    }
    static bool select_for_update(TransProxy& item, TNonopaqueVersion& vers) {
        TNonopaqueVersion v = vers;
        fence();
        if (!item.observe(v))
            return false;
        item.add_write();
        return true;
    }
    static bool select_for_overwrite(TransProxy& item, TLockVersion& vers, value_type *vptr) {
        return item.acquire_write(vers, vptr);
    }
    static bool select_for_overwrite(TransProxy& item, TVersion& vers, value_type* vptr) {
        (void)vers;
        item.add_write(vptr);
        return true;
    }
    static bool select_for_overwrite(TransProxy& item, TNonopaqueVersion& vers, value_type* vptr) {
        (void)vers;
        item.add_write(vptr);
        return true;
    }

    static void copy_row(internal_elem *table_row, const value_type *value) {
        memcpy(&table_row->value, value, sizeof(value_type));
    }
};

}; // namespace tpcc
