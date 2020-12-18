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

#include <any>
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
    static constexpr bool Commute = false;
    static constexpr bool Opacity = false;
    static constexpr bool ReadMyWrite = false;
    static constexpr bool MVCC = false;
};

template <typename V>
class Hashtable_results {
public:
    struct delete_result_type {
        bool abort;  // Whether the transaction should abort
        bool success;  // Whether the action succeeded

        static constexpr delete_result_type Abort = {
            .abort = true, .success = false
        };

        // Non-aborting failure
        static constexpr delete_result_type Fail = {
            .abort = false, .success = false
        };

        // Converts to an unstructure tuple of the result
        std::tuple<bool, bool> to_tuple() const {
            return { abort, success };
        }
    };

    struct insert_result_type {
        bool abort;  // Whether the transaction should abort
        bool success;  // Whether the operation succeeded
        bool existed;  // Whether the key already existed

        static constexpr insert_result_type Abort = {
            .abort = true, .success = false, .existed = false
        };

        // Non-aborting failure
        static constexpr insert_result_type Fail = {
            .abort = false, .success = false, .existed = false
        };

        // Converts to an unstructure tuple of the result
        std::tuple<bool, bool, bool> to_tuple() const {
            return { abort, success, existed };
        }
    };

    struct select_result_type {
        bool abort;  // Whether the transaction should abort
        bool success;  // Whether the action succeeded
        uintptr_t ref;  // An internally-held reference
        const V* value;  // The selected value

        static constexpr select_result_type Abort = {
            .abort = true, .success = false, .ref = 0, .value = nullptr
        };

        // Non-aborting failure
        static constexpr select_result_type Fail = {
            .abort = false, .success = false, .ref = 0, .value = nullptr
        };

        // Converts to an unstructure tuple of the result
        std::tuple<bool, bool, uintptr_t, const V*> to_tuple() const {
            return { abort, success, ref, value };
        }
    };
};

template <
    typename K, typename V,
    typename H=std::hash<K>, typename P=std::equal_to<K>>
class Hashtable_mvcc_params : public Hashtable_params<K, V, H, P> {
public:
    static constexpr bool MVCC = true;
};

template <
    typename K, typename V,
    typename H=std::hash<K>, typename P=std::equal_to<K>>
class Hashtable_opaque_params : public Hashtable_params<K, V, H, P> {
public:
    static constexpr bool Opacity = true;
};

enum HashtableAccessMethod {
    Blind = 0,
    ReadOnly,
    ReadWrite,
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
    typedef commutators::Commutator<value_type> comm_type;
    typedef typename Params::Hash Hash;
    typedef typename Params::Pred Pred;
    typedef typename std::conditional_t<Params::Opacity, TVersion, TNonopaqueVersion> version_type;
    typedef typename std::conditional_t<Params::Opacity, TWrapped<value_type>, TNonopaqueWrapped<value_type>> wrapped_type;
    typedef IndexValueContainer<value_type, version_type> value_container_type;

    typedef HashtableAccessMethod AccessMethod;
    typedef typename Hashtable_results<value_type>::delete_result_type delete_result_type;
    typedef typename Hashtable_results<value_type>::insert_result_type insert_result_type;
    typedef typename Hashtable_results<value_type>::select_result_type select_result_type;

    // MVCC typedefs
    typedef MvObject<value_type> object_type;
    typedef typename object_type::history_type history_type;

#ifdef STO_NO_STM
    static constexpr bool enable_stm = false;
#else
    static constexpr bool enable_stm = true;
#endif

    static constexpr AccessMethod BlindAccess = AccessMethod::Blind;
    static constexpr AccessMethod ReadOnlyAccess = AccessMethod::ReadOnly;
    static constexpr AccessMethod ReadWriteAccess = AccessMethod::ReadWrite;

private:
    // Our hashtable is an array of linked lists.
    // An internal_elem is the node type for these linked lists.
    struct occ_internal_elem {
        occ_internal_elem* next;
        key_type key;
        value_container_type row_container;
        bool deleted;

        occ_internal_elem(const key_type& k, const value_type& v, bool valid)
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

    // And the MVCC version of the internal element
    struct mvcc_internal_elem {
        mvcc_internal_elem* next;
        key_type key;
        object_type obj;

        mvcc_internal_elem(const key_type& k): next(nullptr), key(k), obj() {}
    };

public:
    typedef typename std::conditional_t<Params::MVCC, mvcc_internal_elem, occ_internal_elem> internal_elem;

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
        internal_elem* head;
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

    inline key_type coerce_key(const std::any& key) const {
        return std::any_cast<key_type>(key);
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
        if constexpr (Params::MVCC) {
            if (!e) {
                return nullptr;
            }
            return &(e->obj.nontrans_access());
        } else {
            if (!e || !e->valid() || e->deleted) {
                return nullptr;
            }
            return &(e->row_container.row);
        }
    }

    void nontrans_put(const key_type& key, const value_type& value) {
        bucket_entry& buck = map_[find_bucket_idx(key)];
        buck.version.lock_exclusive();
        internal_elem* e = find_in_bucket(buck, key);

        if (!e) {
            internal_elem* new_head;
            if constexpr (Params::MVCC) {
                new_head = new internal_elem(key);
                new_head->obj.nontrans_access() = value;
            } else {
                new_head = new internal_elem(key, value, true);
            }
            new_head->next = buck.head;
            buck.head = new_head;
        } else {
            if constexpr (Params::MVCC) {
                e->obj.nontrans_access() = value;
            } else {
                copy_row(e, &value);
                buck.version.inc_nonopaque();
            }
        }
        buck.version.unlock_exclusive();
    }

    typename std::enable_if_t<enable_stm, delete_result_type>
    // Transactional delete of the given key.
    transDelete(const key_type& key) {
        bucket_entry& buck = map_[find_bucket_idx(key)];
        bucket_version_type buck_vers = buck.version;
        fence();

        internal_elem* e = find_in_bucket(buck, key);
        if (e) {
            auto item = Sto::item(this, e);
            history_type* h = nullptr;
            bool valid = true;
            if constexpr (Params::MVCC) {
                h = e->obj.find(txn_read_tid());
                (void) valid;  // Avoid set-but-not-used error
            } else {
                valid = e->valid();
                (void) h;  // Avoid set-but-not-used error
            }

            if constexpr (Params::MVCC) {
                if (is_phantom(h, item)) {
                    return delete_result_type::Fail;
                }
            } else {
                if (is_phantom(e, item)) {
                    return delete_result_type::Abort;
                }
            }

            if constexpr (Params::MVCC) {
                if (Params::ReadMyWrite) {
                    if (has_delete(item)) {
                        return delete_result_type::Fail;
                    }
                    if (h->status_is(DELETED) && has_insert(item)) {
                        item.add_flags(delete_bit);
                        return { .abort = false, .success = true };
                    }
                }
            } else {
                if (Params::ReadMyWrite && has_insert(item)) {
                    if (!valid && has_insert(item)) {
                        // Deleting own insert
                        remove(e);
                        // Make item ignored and queue for gc later
                        item.remove_read().remove_write().clear_flags(
                                insert_bit | delete_bit);
                        Sto::item(this, make_bucket_key(buck))
                            .observe(buck_vers);
                        return { .abort = false, .success = true };
                    }

                    assert(valid);

                    if (has_delete(item)) {
                        return delete_result_type::Fail;
                    }
                }
            }

                
            // Add this to read set
            if constexpr (Params::MVCC) {
                MvAccess::template read<value_type>(item, h);
                if (h->status_is(DELETED)) {
                    // Deleting an already-deleted element
                    return delete_result_type::Fail;
                }
                item.add_write();
            } else {
                if (!item.observe(e->version())) {
                    return delete_result_type::Abort;
                }
                item.add_write();
                fence();

                // Double check deleted status after observation
                if (e->deleted) {  // Another thread beat us to it
                    return delete_result_type::Abort;
                }
            }

            item.add_flags(delete_bit);
            return { .abort = false, .success = true };
        }

        // Item not found, so observe the bucket for changes
        auto buck_item = Sto::item(this, make_bucket_key(buck));
        if (!buck_item.observe(buck_vers)) {
            return delete_result_type::Abort;
        }

        return delete_result_type::Fail;
    }

    typename std::enable_if_t<enable_stm, select_result_type>
    // Transactional get on the given key.
    transGet(const key_type& key, const AccessMethod access) {
        bucket_entry& buck = map_[find_bucket_idx(key)];
        version_type buck_vers = buck.version;
        fence();

        internal_elem* e = find_in_bucket(buck, key);

        if (e) {
            return transGet(reinterpret_cast<uintptr_t>(e), access);
        }

        if (!Sto::item(this, make_bucket_key(buck)).observe(buck_vers)) {
            return select_result_type::Abort;
        }

        return select_result_type::Fail;
    }

    typename std::enable_if_t<enable_stm, select_result_type>
    // Transactional get on the given reference.
    transGet(uintptr_t ref, const AccessMethod access) {
        auto e = reinterpret_cast<internal_elem*>(ref);

        TransProxy item = Sto::item(this, e);
        history_type* h = nullptr;
        if constexpr (Params::MVCC) {
            h = e->obj.find(txn_read_tid());
        } else {
            (void) h;  // Avoid set-but-not-used error
        }

        if constexpr (Params::MVCC) {
            if (is_phantom(h, item)) {
                return select_result_type::Fail;
            }
        } else {
            if (is_phantom(e, item)) {
                return select_result_type::Abort;
            }
        }

        if (Params::ReadMyWrite) {
            if (has_delete(item)) {
                return select_result_type::Fail;
            }

            if (has_update(item)) {
                value_type* vp = nullptr;
                if (has_insert(item)) {
                    if constexpr (Params::MVCC) {
                        vp = h->vp();
                    } else {
                        vp = &(e->row_container.row);
                    }
                } else {
                    vp = item.template raw_write_value<value_type*>();
                }
                assert(vp);
                return {
                    .abort = false, .success = true, .ref = ref, .value = vp };
            }
        }

        value_type* vp = nullptr;
        switch (access) {
            case Blind:
                // Do nothing
                break;
            case ReadOnly:
                if constexpr (Params::MVCC) {
                    MvAccess::template read<value_type>(item, h);
                    vp = h->vp();
                    assert(vp);
                } else {
                    if (!item.observe(e->version())) {
                        return select_result_type::Abort;
                    }
                    vp = &(e->row_container.row);
                }
                break;
            case ReadWrite:
                if constexpr (Params::MVCC) {
                    MvAccess::template read<value_type>(item, h);
                    vp = h->vp();
                    assert(vp);
                } else {
                    bool abort = !item.observe(e->version());
                    if (!abort) {
                        item.add_write();
                    }
                    item.add_flags(update_bit);

                    if (abort) {
                        return select_result_type::Abort;
                    }
                    vp = &(e->row_container.row);
                }
                break;
        }

        return {
            .abort = false, .success = true, .ref = ref, .value = vp };
    }

    typename std::enable_if_t<enable_stm, insert_result_type>
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
            history_type* h = nullptr;

            if constexpr (Params::MVCC) {
                h = e->obj.find(txn_read_tid());
                if (is_phantom(h, item)) {
                    return insert_result_type::Abort;
                }
            } else {
                (void) h;  // Avoid set-but-not-used error
                if (is_phantom(e, item)) {
                    return insert_result_type::Abort;
                }
            }

            if (Params::ReadMyWrite) {
                if (has_delete(item)) {
                    item.clear_flags(delete_bit).clear_write().
                        template add_write<value_type*>(value);
                    return insert_result_type::Fail;
                }
            }

            if constexpr (Params::MVCC) {
                if (overwrite) {
                    item.template add_write<value_type*>(value);
                } else {
                    MvAccess::template read<value_type>(item, h);
                }
            } else {
                if (overwrite) {
                    item.template add_write<value_type*>(value);
                    if (Params::ReadMyWrite) {
                        if (has_insert(item)) {
                            copy_row(e, value);
                        }
                    }
                } else {
                    if (!item.observe(e->version())) {
                        return insert_result_type::Abort;
                    }
                }
            }

            return { .abort = false, .success = true, .existed = true };
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

        return { .abort = false, .success = true, .existed = false };
    }

    typename std::enable_if_t<enable_stm, void>
    // Transactional update on the given reference. Since the reference is
    // given, it is assumed that the row already exists.
    transUpdate(uintptr_t ref, value_type* value) {
        auto e = reinterpret_cast<internal_elem*>(ref);
        auto item = Sto::item(this, e);
        if constexpr (Params::MVCC) {
            item.add_write(value);
        } else {
            item.acquire_write(e->version(), value);
        }
    }

    typename std::enable_if_t<enable_stm, void>
    // Transactional update on the given reference. Since the reference is
    // given, it is assumed that the row already exists.
    transUpdate(uintptr_t ref, const comm_type& comm) {
        assert(&comm);
        auto e = reinterpret_cast<internal_elem*>(ref);
        auto item = Sto::item(this, e);
        item.add_commute(&comm);
    }

    // TObject interface method
    bool lock(TransItem& item, Transaction& txn) override {
        assert(!is_bucket(item));
        auto e = item.key<internal_elem*>();

        if constexpr (Params::MVCC) {
            if (item.has_read()) {
                auto hprev = item.read_value<history_type*>();
                // Lock fails if prev history element has already been read
                // in the future
                if (Sto::commit_tid() < hprev->rtid()) {
                    TransProxy(txn, item).add_write(nullptr);
                    return false;
                }
            }

            history_type* h;
            if (item.has_commute()) {
                // Create a delta version
                auto wval = item.template write_value<comm_type*>();
                h = e->obj.new_history(
                    Sto::commit_tid(), std::move(*wval));
            } else {
                // Create a full version
                auto wval = item.template write_value<value_type*>();
                if (has_delete(item)) {
                    // Handling delete versions
                    h = e->obj.new_history(
                        Sto::commit_tid(), nullptr);
                    h->status_delete();
                    h->set_delete_cb(this, delete_cb, e);
                } else {
                    // Just a regular write
                    h = e->obj.new_history(
                        Sto::commit_tid(), wval);
                }
            }

            assert(h);  // Ensure that we actually managed to generate something

            bool result = e->obj.template cp_lock<Params::Commute>(Sto::commit_tid(), h);
            if (!result && !h->status_is(MvStatus::ABORTED)) {
                // Our version didn't make it onto the chain; simply deallocate
                e->obj.delete_history(h);
                TransProxy(txn, item).add_mvhistory(nullptr);
            } else {
                // Version is on the version chain
                TransProxy(txn, item).add_mvhistory(h);
            }
            return result;
        } else {
            return txn.try_lock(item, e->version());
        }
    }

    // TObject interface method
    bool check(TransItem& item, Transaction& txn) override {
        if (is_bucket(item)) {
            bucket_entry& buck = *bucket_address(item);
            return buck.version.cp_check_version(txn, item);
        }

        auto e = item.key<internal_elem*>();
        if constexpr (Params::MVCC) {
            auto h = item.template read_value<history_type*>();
            return e->obj.cp_check(txn_read_tid(), h);
        } else {
            return e->version().cp_check_version(txn, item);
        }
    }

    // TObject interface method
    void install(TransItem& item, Transaction& txn) override {
        assert(!is_bucket(item));
        auto e = item.key<internal_elem*>();

        if constexpr (Params::MVCC) {
            auto h = item.template write_value<history_type*>();
            e->obj.cp_install(h);
        } else {
            if (has_delete(item)) {
                assert(e->valid() && !e->deleted);
                e->deleted = true;
                fence();
                txn.set_version(e->version());
                return;
            }

            // Is an update
            if (!has_insert(item)) {
                if (item.has_commute()) {
                    comm_type &comm = *item.write_value<comm_type*>();
                    copy_row(e, comm);
                } else {
                    auto value = item.write_value<value_type*>();
                    copy_row(e, value);
                }
            }

            txn.set_version_unlock(e->version(), item);
        }
    }

    // TObject interface method
    void unlock(TransItem& item) override {
        assert(!is_bucket(item));
        if constexpr (Params::MVCC) {
            (void) item;  // Avoid set-but-not-used error
        } else {
            auto e = item.key<internal_elem*>();
            e->version().cp_unlock(item);
        }
    }

    // TObject interface method
    void cleanup(TransItem& item, bool committed) override {
        if constexpr (Params::MVCC) {
            if (!committed) {
                // Abort all unaborted history elements
                auto e = item.key<internal_elem*>();
                if (item.has_mvhistory()) {
                    auto h = item.template write_value<history_type*>();
                    if (h) {
                        h->status_txn_abort();
                    }
                }
            }
        } else {
            if (committed ? has_delete(item) : has_insert(item)) {
                assert(!is_bucket(item));
                auto e = item.key<internal_elem*>();
                assert(!e->valid() || e->deleted);
                remove(e);
                item.clear_needs_unlock();
            }
        }
    }

private:
    static bucket_entry* bucket_address(const TransItem& item) {
        uintptr_t bucket_key = item.key<uintptr_t>();
        return reinterpret_cast<bucket_entry*>(bucket_key & ~bucket_bit);
    }

    static void copy_row(internal_elem *e, comm_type &comm) {
        comm.operate(e->row_container.row);
    }

    static void copy_row(internal_elem* e, const value_type* value) {
        if (value) {
            e->row_container.row = *value;
        }
    }

    // Callback function for enqueueing the physical deletes
    template <typename P=Params>
    static typename std::enable_if_t<P::MVCC, void>
    delete_cb(void* ht_ptr, void* ele_ptr, void* history_ptr) {
        auto ht = reinterpret_cast<Hashtable<Params>*>(ht_ptr);
        auto el = reinterpret_cast<internal_elem*>(ele_ptr);
        auto hp = reinterpret_cast<history_type*>(history_ptr);

        bucket_entry& buck = ht->map_[ht->find_bucket_idx(el->key)];
        buck.version.lock_exclusive();

        internal_elem* prev = nullptr;
        internal_elem* curr = buck.head;
        while (curr && (curr != el)) {
            prev = curr;
            curr = curr->next;
        }

        assert(curr);

        if (prev) {
            prev->next = curr->next;
        } else {
            buck.head = curr->next;
        }
        if (el->obj.is_head(hp)) {
            // Delete is still the latest version, so just gc the entire object
            buck.version.unlock_exclusive();
            Transaction::rcu_delete(el);
        } else {
            buck.version.unlock_exclusive();
        }
    }

    static bool is_bucket(const TransItem& item) {
        return item.key<uintptr_t>() & bucket_bit;
    }

    template <typename P=Params>
    static typename std::enable_if_t<P::MVCC, bool>
    is_phantom(history_type* h, const TransItem& item) {
        return (h->status_is(DELETED) && !has_insert(item));
    }

    template <typename P=Params>
    static typename std::enable_if_t<!P::MVCC, bool>
    is_phantom(internal_elem* e, const TransItem& item) {
        return (!e->valid() && !has_insert(item));
    }

    static uintptr_t make_bucket_key(const bucket_entry& bucket) {
        return (reinterpret_cast<uintptr_t>(&bucket) | bucket_bit);
    }

    static TransactionTid::type txn_read_tid() {
        return Sto::read_tid();
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
