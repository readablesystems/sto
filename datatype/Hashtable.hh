#pragma once
#include "config.h"
#include "compiler.hh"

#include "Sto.hh"

#include "masstree.hh"
#include "kvthread.hh"
#include "masstree_tcursor.hh"
#include "masstree_insert.hh"
#include "masstree_print.hh"
#include "masstree_remove.hh"
#include "masstree_scan.hh"
#include "string.hh"

#include <vector>
#include "DB_structs.hh"
#include "VersionSelector.hh"
#include "MVCC.hh"

#include "TBox.hh"
#include "TMvBox.hh"

// Inherit from this class as needed
template <
    typename K, typename V,
    typename H=std::hash<K>, typename P=std::equal_to<K>>
class Hashtable_params {
public:
    typedef K Key;
    typedef V Value;
    typedef H Hash;
    typedef P Pred;

    static constexpr uint32_t Capacity = 129;
    static constexpr bool Opacity = false;
    static constexpr bool ReadMyWrite = false;
};

template <typename Params>
#ifdef STO_NO_STM
class Hashtable {
#else
class Hashtable : public TObject {
#endif
public:
    typedef typename Params::Key key_type;
    typedef typename Params::Value value_type;
    typedef typename Params::Hash Hash;
    typedef typename Params::Pred Pred;
    typedef typename std::conditional<Params::Opacity, TVersion, TNonopaqueVersion>::type version_type;
    typedef typename std::conditional<Params::Opacity, TWrapped<value_type>, TNonopaqueWrapped<value_type>>::type wrapped_type;
    typedef IndexValueContainer<value_type, version_type> value_container_type;

#ifdef STO_NO_STM
    static constexpr bool enable_stm = false;
#else
    static constexpr bool enable_stm = true;
#endif

    struct select_result_type {
        bool abort;  // Whether the transaction should abort
        bool success;  // Whether the action succeeded
        uintptr_t ref;  // An internally-held reference
        const value_type* value;  // The selected value

        static constexpr select_result_type Abort = {
            .abort = true, .success = false, .ref = 0, .value = nullptr
        };

        // Converts to an unstructure tuple of the result
        std::tuple<bool, bool, uintptr_t, const value_type*> to_tuple() const {
            return { abort, success, ref, value };
        }
    };

    struct insert_result_type {
        bool abort;  // Whether the transaction should abort
        bool existed;  // Whether the key already existed

        static constexpr insert_result_type Abort = {
            .abort = true, .existed = false
        };

        // Converts to an unstructure tuple of the result
        std::tuple<bool, bool> to_tuple() const {
            return { abort, existed };
        }
    };

    struct delete_result_type {
        bool abort;  // Whether the transaction should abort
        bool success;  // Whether the action succeeded

        static constexpr delete_result_type Abort = {
            .abort = true, .success = false
        };

        // Converts to an unstructure tuple of the result
        std::tuple<bool, bool> to_tuple() const {
            return { abort, success };
        }
    };

    // Our hashtable is an array of linked lists.
    // An internal_elem is the node type for these linked lists.
    struct internal_elem {
        internal_elem *next;
        key_type key;
        value_container_type row_container;
        bool deleted;

        internal_elem(const key_type& k, const value_type& v, bool valid)
            : next(nullptr), key(k),
              row_container(
                      Sto::initialized_tid() |
                      ((!enable_stm || valid) ? 0 : invalid_bit),
                      !valid, v),
              deleted(false) {}

        version_type& version() {
            return row_container.row_version();
        }

        bool valid() {
            return !(version().value() & invalid_bit);
        }
    };

private:
    // For concurrency control of buckets
    typedef version_type bucket_version_type;

    // used to mark whether a key is a bucket (for bucket version checks)
    // or a pointer (which will always have the lower 3 bits as 0)
    static constexpr uintptr_t bucket_bit = 1U << 0;

    static constexpr typename version_type::type invalid_bit = TransactionTid::user_bit;

    static constexpr TransItem::flags_type insert_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit << 1;
    static constexpr TransItem::flags_type update_bit = TransItem::user0_bit << 2;

    struct bucket_entry {
        internal_elem *head;
        // this is the bucket version number, which is incremented on insert
        // we use it to make sure that an unsuccessful key lookup will still be
        // unsuccessful at commit time (because this will always be true if no
        // new inserts have occurred in this bucket)
        version_type version;
        bucket_entry() : head(nullptr), version(0) {}
    };

    typedef std::vector<bucket_entry> MapType;

public:
    Hashtable(
            uint32_t size = Params::Capacity, Hash h = Hash(),
            Pred p = Pred()) :
            map_(), hasher_(h), pred_(p) {
        map_.resize(size);
    }

    static bool has_delete(const TransItem& item) {
        return (item.flags() & delete_bit) != 0;
    }

    static bool has_insert(const TransItem& item) {
        return (item.flags() & insert_bit) != 0;
    }

    static bool has_update(const TransItem& item) {
        return (item.flags() & update_bit) != 0;
    }

    inline size_t find_bucket_idx(const key_type& k) const {
        return hash(k) % nbuckets();
    }
    inline size_t hash(const key_type& k) const {
        return hasher_(k);
    }
    inline size_t nbuckets() const {
        return map_.size();
    }

    bool nontrans_delete(const key_type& key) {
        bucket_entry& buck = map_[find_bucket_idx(key)];
        buck.version.lock_exclusive();

        internal_elem* prev = nullptr;
        internal_elem* curr = buck.head;
        while (curr && !pred_(curr->key, key)) {
            prev = curr;
            curr = curr->next;
        }

        if (!curr) {
            buck.version.unlock_exclusive();
            return false;
        }

        if (prev) {
            prev->next = curr->next;
        } else {
            buck.head = curr->next;
        }

        buck.version.unlock_exclusive();
        delete curr;
        return true;
    }

    value_type* nontrans_get(const key_type& key) {
        bucket_entry& buck = map_[find_bucket_idx(key)];
        internal_elem* e = find_in_bucket(buck, key);
        if (!e || !e->valid() || e->deleted) {
            return nullptr;
        }

        return &(e->row_container.row);
    }

    void nontrans_put(const key_type& key, const value_type& value) {
        bucket_entry& buck = map_[find_bucket_idx(key)];
        buck.version.lock_exclusive();
        internal_elem* e = find_in_bucket(buck, key);

        if (!e) {
            internal_elem* new_head = new internal_elem(key, value, true);
            new_head->next = buck.head;
            buck.head = new_head;
        } else {
            copy_row(e, &value);
            buck.version.inc_nonopaque();
        }
        buck.version.unlock_exclusive();
    }

    typename std::enable_if<enable_stm, delete_result_type>::type
    // Transactional delete of the given key.
    transDelete(const key_type& key) {
        bucket_entry& buck = map_[find_bucket_idx(key)];
        bucket_version_type buck_vers = buck.version;
        fence();

        internal_elem* e = find_in_bucket(buck, key);
        if (e) {
            fence();

            auto item = Sto::item(this, e);
            bool valid = e->valid();

            if (is_phantom(e, item)) {
                return delete_result_type::Abort;
            }

            if (Params::ReadMyWrite && has_insert(item)) {
                if (!valid && has_insert(item)) {
                    // Deleting own insert
                    remove(e);
                    // Make item ignored and queue for garbage collection later
                    item.remove_read().remove_write().clear_flags(
                            insert_bit | delete_bit);
                    Sto::item(this, make_bucket_key(buck)).observe(buck_vers);
                    return { .abort = false, .success = true };
                }

                assert(valid);

                if (has_delete(item)) {
                    return { .abort = false, .success = false };
                }
            }

            // Add this to read set
            if (!item.observe(e->version())) {
                return delete_result_type::Abort;
            }
            item.add_write();
            fence();

            // Double check deleted status after observation
            if (e->deleted) {  // Another thread beat us to it
                return delete_result_type::Abort;
            }

            item.add_flags(delete_bit);
            return { .abort = false, .success = true };
        }

        // Item not found, so observe the bucket for changes
        auto buck_item = Sto::item(this, make_bucket_key(buck));
        if (!buck_item.observe(buck_vers)) {
            return delete_result_type::Abort;
        }

        return { .abort = false, .success = false };
    }

    typename std::enable_if<enable_stm, select_result_type>::type
    // Transactional get on the given key.
    transGet(const key_type& key, const bool readonly) {
        bucket_entry& buck = map_[find_bucket_idx(key)];
        version_type buck_vers = buck.version;
        fence();

        internal_elem* e = find_in_bucket(buck, key);

        if (e) {
            return transGet(reinterpret_cast<uintptr_t>(e), readonly);
        }

        if (!Sto::item(this, make_bucket_key(buck)).observe(buck_vers)) {
            return select_result_type::Abort;
        }

        return { .abort = false, .success = false, .ref = 0, .value = nullptr };
    }

    typename std::enable_if<enable_stm, select_result_type>::type
    // Transactional get on the given reference.
    transGet(uintptr_t ref, const bool readonly) {
        auto e = reinterpret_cast<internal_elem*>(ref);
        bool abort = false;

        TransProxy item = Sto::item(this, e);
        if (is_phantom(e, item)) {
            return select_result_type::Abort;
        }

        if (Params::ReadMyWrite) {
            if (has_delete(item)) {
                return {
                    .abort = false, .success = false, .ref = 0,
                    .value = nullptr };
            }

            if (has_update(item)) {
                value_type* vp = nullptr;
                if (has_insert(item)) {
                    vp = &(e->row_container.row);
                } else {
                    vp = item.template raw_write_value<value_type*>();
                }
                assert(vp);
                return {
                    .abort = false, .success = true, .ref = ref, .value = vp };
            }
        }

        if (readonly) {
            abort = !item.observe(e->version());
        } else {
            if (item.observe(e->version())) {
                item.add_write();
            } else {
                abort = true;
            }
            item.add_flags(update_bit);
        }

        if (abort) {
            return select_result_type::Abort;
        }

        return {
            .abort = false, .success = true, .ref = ref,
            .value = &(e->row_container.row) };
    }

    typename std::enable_if<enable_stm, insert_result_type>::type
    // Transactional put on the given key. By default, will not overwrite the
    // existing value if the key already exists in the table.
    transPut(
            const key_type& key, value_type* value, const bool overwrite=false) {
        bucket_entry& buck = map_[find_bucket_idx(key)];
        buck.version.lock_exclusive();
        internal_elem* e = find_in_bucket(buck, key);

        // Key is already in table
        if (e) {
            buck.version.unlock_exclusive();

            auto item = Sto::item(this, e);
            if (is_phantom(e, item)) {
                return insert_result_type::Abort;
            }

            if (Params::ReadMyWrite) {
                if (has_delete(item)) {
                    item.clear_flags(delete_bit).clear_write().
                        template add_write<value_type*>(value);
                    return {
                        .abort = false, .existed = false };
                }
            }

            if (overwrite) {
                item.add_write(value);
                if (Params::ReadMyWrite) {
                    if (has_insert(item)) {
                        copy_row(e, value);
                    }
                } else {
                    if (!item.observe(e->version())) {
                        return insert_result_type::Abort;
                    }
                }
            }

            return { .abort = false, .existed = true };
        }

        // Key is not already in table

        // Insert the new row and check bucket version for changes
        auto buck_vers_0 = bucket_version_type(buck.version.unlocked_value());
        insert_in_bucket(buck, key, value, false);
        internal_elem* new_head = buck.head;
        auto buck_vers_1 = bucket_version_type(buck.version.unlocked_value());

        buck.version.unlock_exclusive();

        // Update bucket version in read set
        auto buck_item = Sto::item(this, make_bucket_key(buck));
        if (buck_item.has_read()) {
            buck_item.update_read(buck_vers_0, buck_vers_1);
        }

        // Finish the write
        auto item = Sto::item(this, new_head);
        item.template add_write<value_type*>(value);  // XXX: is this necessary?
        item.add_flags(insert_bit);

        return { .abort = false, .existed = false };
    }

    typename std::enable_if<enable_stm, void>::type
    // Transactional update on the given reference. Since the reference is
    // given, it is assumed that the row already exists.
    transUpdate(uintptr_t ref, value_type* value) {
        auto e = reinterpret_cast<internal_elem*>(ref);
        auto item = Sto::item(this, e);
        item.acquire_write(e->version(), value);
    }

    // TObject interface method
    bool lock(TransItem& item, Transaction& txn) override {
        assert(!is_bucket(item));
        auto e = item.key<internal_elem*>();
        return txn.try_lock(item, e->version());
    }

    // TObject interface method
    bool check(TransItem& item, Transaction& txn) override {
        if (is_bucket(item)) {
            bucket_entry& buck = *bucket_address(item);
            return buck.version.cp_check_version(txn, item);
        }

        auto e = item.key<internal_elem*>();
        return e->version().cp_check_version(txn, item);
    }

    // TObject interface method
    void install(TransItem& item, Transaction& txn) override {
        assert(!is_bucket(item));
        auto e = item.key<internal_elem*>();

        if (has_delete(item)) {
            assert(e->valid() && !e->deleted);
            e->deleted = true;
            fence();
            txn.set_version(e->version());
            return;
        }

        // Is an update
        if (!has_insert(item)) {
            auto value = item.write_value<value_type*>();
            copy_row(e, value);
        }

        txn.set_version_unlock(e->version(), item);
    }

    // TObject interface method
    void unlock(TransItem& item) override {
        assert(!is_bucket(item));
        auto e = item.key<internal_elem*>();
        e->version().cp_unlock(item);
    }

    // TObject interface method
    void cleanup(TransItem& item, bool committed) override {
        if (committed ? has_delete(item) : has_insert(item)) {
            assert(!is_bucket(item));
            auto e = item.key<internal_elem*>();
            assert(!e->valid() || e->deleted);
            remove(e);
            item.clear_needs_unlock();
        }
    }

private:
    static bucket_entry* bucket_address(const TransItem& item) {
        uintptr_t bucket_key = item.key<uintptr_t>();
        return reinterpret_cast<bucket_entry*>(bucket_key & ~bucket_bit);
    }

    static void copy_row(internal_elem* e, const value_type* value) {
        if (value) {
            e->row_container.row = *value;
        }
    }

    static bool is_bucket(const TransItem& item) {
        return item.key<uintptr_t>() & bucket_bit;
    }

    static bool is_phantom(internal_elem* e, const TransItem& item) {
        return (!e->valid() && !has_insert(item));
    }

    static uintptr_t make_bucket_key(const bucket_entry& bucket) {
        return (reinterpret_cast<uintptr_t>(&bucket) | bucket_bit);
    }

    // Find a key's corresponding internal_elem in a bucket
    internal_elem* find_in_bucket(const bucket_entry& buck, const key_type& k) {
        internal_elem* curr = buck.head;
        while (curr && !pred_(curr->key, k)) {
            curr = curr->next;
        }
        return curr;
    }

    // Insert an internal_elem into a bucket
    void insert_in_bucket(
            bucket_entry& buck, const key_type& k, const value_type* v,
            bool valid) {
        assert(buck.version.is_locked());

        internal_elem* new_head = new internal_elem(
                k, v ? *v : value_type(), valid);
        internal_elem* curr_head = buck.head;

        new_head->next = curr_head;
        buck.head = new_head;

        buck.version.inc_nonopaque();
    }

    // Remove an internal_elem during transactions, with locks
    void remove(internal_elem* e) {
        bucket_entry& buck = map_[find_bucket_idx(e->key)];
        buck.version.lock_exclusive();

        internal_elem* prev = nullptr;
        internal_elem* curr = buck.head;
        while (curr && (curr != e)) {
            prev = curr;
            curr = curr->next;
        }

        assert(curr);

        if (prev) {
            prev->next = curr->next;
        } else {
            buck.head = curr->next;
        }

        buck.version.unlock_exclusive();
        Transaction::rcu_delete(curr);
    }

    // this is the hashtable itself, an array of bucket_entry's
    MapType map_;
    Hash hasher_;
    Pred pred_;
};
