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

        internal_elem(const key_type& k, const value_type& val, bool mark_valid)
            : key(k), next(nullptr),
              version(Sto::initialized_tid() | (mark_valid ? 0 : invalid_bit)),
              value(val) {}

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
                if (!select_for_update(item, e->version, vptr))
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

    // TObject interface methods
    bool lock(TransItem& item, Transaction& txn) override;
    bool check(TransItem& item, Transaction&) override;
    void install(TransItem& item, Transaction& txn) override;
    void unlock(TransItem& item) override;
    void cleanup(TransItem& item, bool committed) override;

    // non-transactional methods
    value_type* nontrans_get(const key_type& k);
    void nontrans_put(const key_type& k, const value_type& v);

private:
    // remove a k-v node during transactions (with locks)
    void _remove(internal_elem *el);
    // non-transactional remove by key
    bool remove(const key_type& k);
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
    internal_elem *find_in_bucket(const bucket_entry& buck, const key_type& k);

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
        (void)vers;
        item.add_write();
        return true;
    }
    static bool select_for_update(TransProxy& item, TNonopaqueVersion& vers) {
        (void)vers;
        item.add_write();
        return true;
    }
    static bool select_for_update(TransProxy& item, TLockVersion& vers, value_type *vptr) {
        return item.acquire_write(vers, vptr);
    }
    static bool select_for_update(TransProxy& item, TVersion& vers, value_type* vptr) {
        (void)vers;
        item.add_write(vptr);
        return true;
    }
    static bool select_for_update(TransProxy& item, TNonopaqueVersion& vers, value_type* vptr) {
        (void)vers;
        item.add_write(vptr);
        return true;
    }

    static void copy_row(internal_elem *table_row, const value_type *value) {
        memcpy(&table_row->value, value, sizeof(value_type));
    }
};

}; // namespace tpcc
