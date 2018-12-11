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
#include "VersionSelector.hh"

namespace bench {

class version_adapter {
public:
    template <bool Adaptive>
    static bool select_for_update(TransProxy& item, TLockVersion<Adaptive>& vers) {
        return item.acquire_write(vers);
    }
    static bool select_for_update(TransProxy& item, TVersion& vers) {
        if (!item.observe(vers))
            return false;
        item.add_write();
        return true;
    }
    static bool select_for_update(TransProxy& item, TNonopaqueVersion& vers) {
        if (!item.observe(vers))
            return false;
        item.add_write();
        return true;
    }
    template <bool Opaque>
    static bool select_for_update(TransProxy& item, TSwissVersion<Opaque>& vers) {
        return item.acquire_write(vers);
    }
    // XXX is it correct?
    template <bool Opaque, bool Extend>
    static bool select_for_update(TransProxy& item, TicTocVersion<Opaque, Extend>& vers) {
        if (!item.observe(vers))
            return false;
        item.acquire_write(vers);
        return true;
    }

    template <bool Adaptive, typename T>
    static bool select_for_overwrite(TransProxy& item, TLockVersion<Adaptive>& vers, const T& val) {
        return item.acquire_write(vers, val);
    }
    template <typename T>
    static bool select_for_overwrite(TransProxy& item, TVersion& vers, const T& val) {
        (void)vers;
        item.add_write(val);
        return true;
    }
    template <typename T>
    static bool select_for_overwrite(TransProxy& item, TNonopaqueVersion& vers, const T& val) {
        (void)vers;
        item.add_write(val);
        return true;
    }
    template <bool Opaque, typename T>
    static bool select_for_overwrite(TransProxy& item, TSwissVersion<Opaque>& vers, const T& val) {
        return item.acquire_write(vers, val);
    }
    template <bool Opaque, bool Extend, typename T>
    static bool select_for_overwrite(TransProxy& item, TicTocVersion<Opaque, Extend>& vers, const T& val) {
        return item.acquire_write(vers, val);
    }
};

template <typename DBParams>
struct get_occ_version {
    typedef typename std::conditional<DBParams::Opaque, TVersion, TNonopaqueVersion>::type type;
};

template <typename DBParams>
struct get_version {
    typedef typename std::conditional<DBParams::TicToc, TicTocVersion<>,
            typename std::conditional<DBParams::Adaptive, TLockVersion<true /* adaptive */>,
            typename std::conditional<DBParams::TwoPhaseLock, TLockVersion<false>,
            typename std::conditional<DBParams::Swiss, TSwissVersion<DBParams::Opaque>,
            typename get_occ_version<DBParams>::type>::type>::type>::type>::type type;
};

template <typename DBParams>
class integer_box : public TObject {
public:
    typedef int64_t int_type;
    typedef TNonopaqueVersion version_type;

    integer_box()
        : vers(Sto::initialized_tid() | TransactionTid::nonopaque_bit),
          value() {}

    integer_box& operator=(int_type x) {
        value = x;
        return *this;
    }

    std::pair<bool, int_type> trans_read() {
        auto item = Sto::item(this, 0);
        bool ok = item.observe(vers);
        if (ok)
            return {true, value};
        else
            return {false, 0};
    }

    void trans_increment(int_type i) {
        auto item = Sto::item(this, 0);
        item.add_write(i);
    }

    bool lock(TransItem& item, Transaction& txn) override {
        return txn.try_lock(item, vers);
    }
    bool check(TransItem& item, Transaction& txn) override {
        return vers.cp_check_version(txn, item);
    }
    void install(TransItem& item, Transaction& txn) override {
        value += item.write_value<int_type>();
        txn.set_version_unlock(vers, item);
    }
    void unlock(TransItem& item) override {
        vers.cp_unlock(item);
    }

private:

    version_type vers;
    int_type value;
};

// unordered index implemented as hashtable
template <typename K, typename V, typename DBParams>
class unordered_index : public TObject {
public:
    typedef K key_type;
    typedef V value_type;

    typedef typename get_occ_version<DBParams>::type bucket_version_type;
    typedef typename get_version<DBParams>::type version_type;

    typedef std::hash<K> Hash;
    typedef std::equal_to<K> Pred;
    
    static constexpr typename version_type::type invalid_bit = TransactionTid::user_bit;

    static constexpr bool index_read_my_write = DBParams::RdMyWr;

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
            : next(nullptr), key(k),
              version(Sto::initialized_tid() | (mark_valid ? 0 : invalid_bit), !mark_valid),
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

    uint64_t key_gen_;

    // used to mark whether a key is a bucket (for bucket version checks)
    // or a pointer (which will always have the lower 3 bits as 0)
    static constexpr uintptr_t bucket_bit = 1U<<0;

    static constexpr TransItem::flags_type insert_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit<<1;

public:
    typedef std::tuple<bool, bool, uintptr_t, const value_type*> sel_return_type;
    typedef std::tuple<bool, bool>                               ins_return_type;
    typedef std::tuple<bool, bool>                               del_return_type;

    unordered_index(size_t size, Hash h = Hash(), Pred p = Pred()) :
            map_(), hasher_(h), pred_(p), key_gen_(0) {
        map_.resize(size);
    }

    inline size_t hash(const key_type& k) const {
        return hasher_(k);
    }
    inline size_t nbuckets() const {
        return map_.size();
    }
    inline size_t find_bucket_idx(const key_type& k) const {
        return hash(k) % nbuckets();
    }

    uint64_t gen_key() {
        return fetch_and_add(&key_gen_, 1);
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
                    return sel_return_type(true, false, 0, nullptr);
                if (item.has_write())
                    return sel_return_type(true, true, reinterpret_cast<uintptr_t>(e), &((item.template write_value<internal_elem *>())->value));
            }

            if (for_update) {
                if (!version_adapter::select_for_update(item, e->version))
                    goto abort;
            } else {
                if (!item.observe(e->version))
                    goto abort;
            }

            return sel_return_type(true, true, reinterpret_cast<uintptr_t>(e), &e->value);
        } else {
            // if not found, observe bucket version
            bool ok = Sto::item(this, make_bucket_key(buck)).observe(buck_vers);
            if (!ok)
                goto abort;
            return sel_return_type(true, false, 0, nullptr);
        }

    abort:
        return sel_return_type(false, false, 0, nullptr);
    }

    // this method is only to be used after calling select_row() with for_update set to true
    // otherwise behavior is undefined
    // update_row() takes ownership of the row pointer (new_row) passed in, and the row to be updated (table_row)
    // should never be modified by the transaction user
    // the new_row pointer stays valid for the rest of the duration of the transaction and the associated
    // temporary row WILL NOT be deallocated until commit/abort time
    void update_row(uintptr_t rid, value_type *new_row) {
        auto item = Sto::item(this, reinterpret_cast<internal_elem *>(rid));
        assert(item.has_write() && !has_insert(item));
        item.add_write(new_row);
    }

    // returns (success : bool, found : bool)
    // insert_row() takes ownership of the row pointer (vptr) passed in
    // the pointer stays valid for the rest of the duration of the transaction and the associated temporary row
    // WILL NOT be deallocated until commit/abort time
    ins_return_type
    insert_row(const key_type& k, value_type *vptr, bool overwrite = false) {
        bucket_entry& buck = map_[find_bucket_idx(k)];

        buck.version.lock_exclusive();
        internal_elem *e = find_in_bucket(buck, k);

        if (e) {
            buck.version.unlock_exclusive();
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
                if (!version_adapter::select_for_overwrite(item, e->version, vptr))
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
            auto buck_vers_0 = bucket_version_type(buck.version.unlocked_value());
            insert_in_bucket(buck, k, vptr, false);
            internal_elem *new_head = buck.head;
            auto buck_vers_1 = bucket_version_type(buck.version.unlocked_value());

            buck.version.unlock_exclusive();

            // update bucket version in the read set (if any) since it's changed by ourselves
            auto bucket_item = Sto::item(this, make_bucket_key(buck));
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
                    Sto::item(this, make_bucket_key(buck)).observe(buck_vers);
                    return del_return_type(true, true);
                }
                assert(valid);
                if (has_delete(item))
                    return del_return_type(true, false);
            }
            // select_for_update() will automatically add an observation for OCC version types
            // so that we can catch change in "deleted" status of a table row at commit time
            if (!version_adapter::select_for_update(item, e->version))
                goto abort;
            fence();
            // it vital that we check the "deleted" status after registering an observation
            if (e->deleted)
                goto abort;
            item.add_flags(delete_bit);

            return del_return_type(true, true);
        } else {
            // not found -- add observation of bucket version
            bool ok = Sto::item(this, make_bucket_key(buck)).observe(buck_vers);
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

    bool check(TransItem& item, Transaction& txn) override {
        if (is_bucket(item)) {
            bucket_entry &buck = *bucket_address(item);
            return buck.version.cp_check_version(txn, item);
        } else {
            internal_elem *el = item.key<internal_elem *>();
            return el->version.cp_check_version(txn, item);
        }
    }

    void install(TransItem& item, Transaction& txn) override {
        assert(!is_bucket(item));
        internal_elem *el = item.key<internal_elem*>();
        if (has_delete(item)) {
            el->deleted = true;
            fence();
            txn.set_version(el->version);
            return;
        }
        if (!has_insert(item)) {
            // update
            auto vptr = item.write_value<value_type *>();
            copy_row(el, vptr);
        }

        // hacks for upgrading version numbers from nonopaque to commit_tid
        // (if transaction running in opaque mode) no longer required
        // it's being done by Transaction::set_version_unlock
        txn.set_version_unlock(el->version, item);
    }

    void unlock(TransItem& item) override {
        assert(!is_bucket(item));
        internal_elem *el = item.key<internal_elem *>();
        el->version.cp_unlock(item);
    }

    void cleanup(TransItem& item, bool committed) override {
        if (committed ? has_delete(item) : has_insert(item)) {
            assert(!is_bucket(item));
            internal_elem *el = item.key<internal_elem *>();
            assert(!el->valid() || el->deleted);
            _remove(el);
            item.clear_needs_unlock();
        }
    }

    // non-transactional methods
    value_type* nontrans_get(const key_type& k) {
        bucket_entry& buck = map_[find_bucket_idx(k)];
        internal_elem *el = find_in_bucket(buck, k);
        if (el == nullptr)
            return nullptr;
        return &el->value;
    }
    void nontrans_put(const key_type& k, const value_type& v) {
        bucket_entry& buck = map_[find_bucket_idx(k)];
        buck.version.lock_exclusive();
        internal_elem *el = find_in_bucket(buck, k);
        if (el == nullptr) {
            internal_elem *new_head = new internal_elem(k, v, true);
            new_head->next = buck.head;
            buck.head = new_head;
        } else {
            copy_row(el, &v);
        }
        buck.version.unlock_exclusive();
    }

private:
    // remove a k-v node during transactions (with locks)
    void _remove(internal_elem *el) {
        bucket_entry& buck = map_[find_bucket_idx(el->key)];
        buck.version.lock_exclusive();
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
        buck.version.unlock_exclusive();
        Transaction::rcu_delete(curr);
    }
    // non-transactional remove by key
    bool remove(const key_type& k) {
        bucket_entry& buck = map_[find_bucket_idx(k)];
        buck.version.lock_exclusive();
        internal_elem *prev = nullptr;
        internal_elem *curr = buck.head;
        while (curr != nullptr && !pred_(curr->key, k)) {
            prev = curr;
            curr = curr->next;
        }
        if (curr == nullptr) {
            buck.version.unlock_exclusive();
            return false;
        }
        if (prev != nullptr)
            prev->next = curr->next;
        else
            buck.head = curr->next;
        buck.version.unlock_exclusive();
        delete curr;
        return true;
    }
    // insert a k-v node to a bucket
    void insert_in_bucket(bucket_entry& buck, const key_type& k, const value_type *v, bool valid) {
        assert(buck.version.is_locked());

        internal_elem *new_head = new internal_elem(k, v ? *v : value_type(), valid);
        internal_elem *curr_head = buck.head;

        new_head->next = curr_head;
        buck.head = new_head;

        buck.version.inc_nonopaque();
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
    static bucket_entry *bucket_address(const TransItem& item) {
        uintptr_t bucket_key = item.key<uintptr_t>();
        return reinterpret_cast<bucket_entry*>(bucket_key & ~bucket_bit);
    }

    static void copy_row(internal_elem *table_row, const value_type *value) {
        if (value == nullptr)
            return;
        table_row->value = *value;
    }
};

enum class RowAccess : int { None = 0, ObserveExists, ObserveValue, UpdateValue };

template <typename K, typename V, typename DBParams>
class ordered_index : public TObject {
public:
    typedef K key_type;
    typedef V value_type;

    //typedef typename get_occ_version<DBParams>::type occ_version_type;
    typedef typename get_version<DBParams>::type version_type;

    static constexpr typename version_type::type invalid_bit = TransactionTid::user_bit;
    static constexpr TransItem::flags_type insert_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit << 1u;
    static constexpr TransItem::flags_type row_update_bit = TransItem::user0_bit << 2u;
    static constexpr TransItem::flags_type row_cell_bit = TransItem::user0_bit << 3u;
    static constexpr uintptr_t internode_bit = 1;

    typedef typename value_type::NamedColumn NamedColumn;
    typedef IndexValueContainer<V, version_type> value_container_type;

    static constexpr bool value_is_small = is_small<V>::value;

    static constexpr bool index_read_my_write = DBParams::RdMyWr;

    struct internal_elem {
        key_type key;
        value_container_type row_container;
        bool deleted;

        internal_elem(const key_type& k, const value_type& v, bool valid)
            : key(k),
              row_container((valid ? Sto::initialized_tid() : (Sto::initialized_tid() | invalid_bit)),
                            !valid, v),
              deleted(false) {}

        version_type& version() {
            return row_container.row_version();
        }

        bool valid() {
            return !(version().value() & invalid_bit);
        }
    };

    struct column_access_t {
        int col_id;
        bool update;

        column_access_t(NamedColumn column, bool for_update)
                : col_id(static_cast<int>(column)), update(for_update) {}
    };

    struct cell_access_t {
        int cell_id;
        bool update;

        cell_access_t(int cid, bool for_update)
                : cell_id(cid), update(for_update) {}
    };

    // TransItem key format:
    // |----internal_elem pointer----|--cell id--|I|
    //           48 bits                15 bits   1

    // I: internode bit
    // cell id: valid range 0-32767 (0x7fff)
    // cell id 0 identifies the row item

    class item_key_t {
        typedef uintptr_t type;
        static constexpr unsigned shift = 16u;
        static constexpr type cell_mask = type(0xfffe);
        type key_;

    public:
        item_key_t() : key_() {};
        item_key_t(internal_elem *e, int cell_num) : key_((reinterpret_cast<type>(e) << shift)
                                                          | ((static_cast<type>(cell_num) << 1u) & cell_mask)) {};

        static item_key_t row_item_key(internal_elem *e) {
            return item_key_t(e, 0);
        }

        internal_elem *internal_elem_ptr() const {
            return reinterpret_cast<internal_elem *>(key_ >> shift);
        }

        int cell_num() const {
            return static_cast<int>((key_ & cell_mask) >> 1);
        }

        bool is_row_item() const {
            return (cell_num() == 0);
        }
    };

    static std::vector<cell_access_t>
    column_to_cell_accesses(std::function<int(int)> c_c_map, std::initializer_list<column_access_t> accesses) {
        // pair: {accessed, for update}
        std::vector<std::pair<bool, bool>> all_cells(value_container_type::num_versions, {false, false});
        // the returned list
        std::vector<cell_access_t> cell_accesses;

        for (auto ca : accesses) {
            int cell_id = c_c_map(ca.col_id);
            all_cells[cell_id].first = true;
            all_cells[cell_id].second |= ca.update;
        }

        for (auto it = all_cells.begin(); it != all_cells.end(); ++it) {
            if (it->first)
                cell_accesses.emplace_back(static_cast<int>(it-all_cells.begin()), it->second);
        }
        return cell_accesses;
    }

    struct table_params : public Masstree::nodeparams<15,15> {
        typedef internal_elem* value_type;
        typedef Masstree::value_print<value_type> value_print_type;
        typedef threadinfo threadinfo_type;
    };

    typedef Masstree::Str Str;
    typedef Masstree::basic_table<table_params> table_type;
    typedef Masstree::unlocked_tcursor<table_params> unlocked_cursor_type;
    typedef Masstree::tcursor<table_params> cursor_type;
    typedef Masstree::leaf<table_params> leaf_type;

    typedef typename table_type::node_type node_type;
    typedef typename unlocked_cursor_type::nodeversion_value_type nodeversion_value_type;

    typedef std::tuple<bool, bool, uintptr_t, const value_type*> sel_return_type;
    typedef std::tuple<bool, bool>                               ins_return_type;
    typedef std::tuple<bool, bool>                               del_return_type;

    static __thread typename table_params::threadinfo_type *ti;

    ordered_index(size_t init_size) {
        this->table_init();
        (void)init_size;
    }
    ordered_index() {
        this->table_init();
    }

    void table_init() {
        if (ti == nullptr)
            ti = threadinfo::make(threadinfo::TI_MAIN, -1);
        table_.initialize(*ti);
        key_gen_ = 0;
    }

    static void thread_init() {
        if (ti == nullptr)
            ti = threadinfo::make(threadinfo::TI_PROCESS, TThread::id());
        Transaction::tinfo[TThread::id()].trans_start_callback = []() {
            ti->rcu_start();
        };
        Transaction::tinfo[TThread::id()].trans_end_callback = []() {
            ti->rcu_stop();
        };
    }

    uint64_t gen_key() {
        return fetch_and_add(&key_gen_, 1);
    }

    sel_return_type
    select_row(const key_type& key, RowAccess acc) {
        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        internal_elem *e = lp.value();
        if (found) {
            return select_row(reinterpret_cast<uintptr_t>(e), acc);
        } else {
            if (!register_internode_version(lp.node(), lp.full_version_value()))
                goto abort;
            return sel_return_type(true, false, 0, nullptr);
        }

    abort:
        return sel_return_type(false, false, 0, nullptr);
    }

    sel_return_type
    select_row(const key_type& key, std::initializer_list<column_access_t> accesses) {
        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        internal_elem *e = lp.value();
        if (found) {
            return select_row(reinterpret_cast<uintptr_t>(e), accesses);
        } else {
            if (!register_internode_version(lp.node(), lp.full_version_value()))
                goto abort;
            return sel_return_type(true, false, 0, nullptr);
        }

    abort:
        return sel_return_type(false, false, 0, nullptr);
    }

    sel_return_type
    select_row(uintptr_t rid, RowAccess access) {
        auto e = reinterpret_cast<internal_elem *>(rid);
        bool ok = true;
        TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));

        if (is_phantom(e, row_item))
            goto abort;

        if (index_read_my_write) {
            if (has_delete(row_item)) {
                return sel_return_type(true, false, 0, nullptr);
            }
            if (has_row_update(row_item)) {
                value_type *vptr;
                if (has_insert(row_item))
                    vptr = &e->row_container.row;
                else
                    vptr = row_item.template raw_write_value<value_type *>();
                return sel_return_type(true, true, rid, vptr);
            }
        }

        switch (access) {
            case RowAccess::UpdateValue:
                ok = version_adapter::select_for_update(row_item, e->version());
                row_item.add_flags(row_update_bit);
                break;
            case RowAccess::ObserveExists:
            case RowAccess::ObserveValue:
                ok = row_item.observe(e->version());
                break;
            default:
                break;
        }

        if (!ok)
            goto abort;

        return sel_return_type(true, true, rid, &(e->row_container.row));

    abort:
        return sel_return_type(false, false, 0, nullptr);
    }

    sel_return_type
    select_row(uintptr_t rid, std::initializer_list<column_access_t> accesses) {
        auto e = reinterpret_cast<internal_elem *>(rid);
        TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));

        // Translate from column accesses to cell accesses
        // all buffered writes are only stored in the wdata_ of the row item (to avoid redundant copies)
        auto cell_accesses = column_to_cell_accesses(value_container_type::map, accesses);

        std::vector<TransProxy> cell_items;
        bool any_has_write;
        bool ok;
        std::tie(any_has_write, cell_items) = extract_item_list(cell_accesses, e);

        if (is_phantom(e, row_item))
            goto abort;

        if (index_read_my_write) {
            if (has_delete(row_item)) {
                return sel_return_type(true, false, 0, nullptr);
            }
            if (any_has_write || has_row_update(row_item)) {
                value_type *vptr;
                if (has_insert(row_item))
                    vptr = &e->row_container.row;
                else
                    vptr = row_item.template raw_write_value<value_type *>();
                return sel_return_type(true, true, rid, vptr);
            }
        }

        ok = access_all(cell_accesses, cell_items, e);
        if (!ok)
            goto abort;

        return sel_return_type(true, true, rid, &(e->row_container.row));

    abort:
        return sel_return_type(false, false, 0, nullptr);
    }

    void update_row(uintptr_t rid, value_type *new_row) {
        auto row_item = Sto::item(this, item_key_t::row_item_key(reinterpret_cast<internal_elem *>(rid)));
        if (value_is_small) {
            row_item.add_write(*new_row);
        } else {
            row_item.add_write(new_row);
        }
        // Just update the pointer, don't set the actual write flag
        // we don't want to confuse installs at commit time
        //row_item.clear_write();
    }

    // insert assumes common case where the row doesn't exist in the table
    // if a row already exists, then use select (FOR UPDATE) instead
    ins_return_type
    insert_row(const key_type& key, value_type *vptr, bool overwrite = false) {
        cursor_type lp(table_, key);
        bool found = lp.find_insert(*ti);
        if (found) {
            // NB: the insert method only manipulates the row_item. It is possible
            // this insert is overwriting some previous updates on selected columns
            // The expected behavior is that this row-level operation should overwrite
            // all changes made by previous updates (in the same transaction) on this
            // row. We achieve this by granting this row_item a higher priority.
            // During the install phase, if we notice that the row item has already
            // been locked then we simply ignore installing any changes made by cell items.
            // It should be trivial for a cell item to find the corresponding row item
            // and figure out if the row-level version is locked.
            internal_elem *e = lp.value();
            lp.finish(0, *ti);

            TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));

            if (is_phantom(e, row_item))
                goto abort;

            if (index_read_my_write) {
                if (has_delete(row_item)) {
                    auto proxy = row_item.clear_flags(delete_bit).clear_write();

                    if (value_is_small)
                        proxy.add_write(*vptr);
                    else
                        proxy.add_write(vptr);

                    return ins_return_type(true, false);
                }
            }

            if (overwrite) {
                bool ok;
                if (value_is_small)
                    ok = version_adapter::select_for_overwrite(row_item, e->version(), *vptr);
                else
                    ok = version_adapter::select_for_overwrite(row_item, e->version(), vptr);
                if (!ok)
                    goto abort;
                if (index_read_my_write) {
                    if (has_insert(row_item)) {
                        copy_row(e, vptr);
                    }
                }
            } else {
                // observes that the row exists, but nothing more
                if (!row_item.observe(e->version()))
                    goto abort;
            }

        } else {
            auto e = new internal_elem(key, vptr ? *vptr : value_type(),
                                       false /*!valid*/);
            lp.value() = e;

            node_type *node;
            nodeversion_value_type orig_nv;
            nodeversion_value_type new_nv;

            bool split_right = (lp.node() != lp.original_node());
            if (split_right) {
                node = lp.original_node();
                orig_nv = lp.original_version_value();
                new_nv = lp.updated_version_value();
            } else {
                node = lp.node();
                orig_nv = lp.previous_full_version_value();
                new_nv = lp.next_full_version_value(1);
            }

            fence();
            lp.finish(1, *ti);
            //fence();

            TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));
            //if (value_is_small)
            //    item.add_write<value_type>(*vptr);
            //else
            //    item.add_write<value_type *>(vptr);
            row_item.add_write();
            row_item.add_flags(insert_bit);

            // update the node version already in the read set and modified by split
            if (!update_internode_version(node, orig_nv, new_nv))
                goto abort;
        }

        return ins_return_type(true, found);

    abort:
        return ins_return_type(false, false);
    }

    del_return_type
    delete_row(const key_type& key) {
        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        if (found) {
            internal_elem *e = lp.value();
            TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));

            if (is_phantom(e, row_item))
                goto abort;

            if (index_read_my_write) {
                if (has_delete(row_item))
                    return del_return_type(true, false);
                if (!e->valid() && has_insert(row_item)) {
                    row_item.add_flags(delete_bit);
                    return del_return_type(true, true);
                }
            }

            // select_for_update will register an observation and set the write bit of
            // the TItem
            if (!version_adapter::select_for_update(row_item, e->version()))
                goto abort;
            fence();
            if (e->deleted)
                goto abort;
            row_item.add_flags(delete_bit);
        } else {
            if (!register_internode_version(lp.node(), lp.full_version_value()))
                goto abort;
        }

        return del_return_type(true, found);

    abort:
        return del_return_type(false, false);
    }

    template <typename Callback, bool Reverse>
    bool range_scan(const key_type& begin, const key_type& end, Callback callback,
                    std::initializer_list<column_access_t> accesses, bool phantom_protection = true, int limit = -1) {
        assert((limit == -1) || (limit > 0));
        auto node_callback = [&] (leaf_type* node,
            typename unlocked_cursor_type::nodeversion_value_type version) {
            return ((!phantom_protection) || register_internode_version(node, version));
        };

        auto cell_accesses = column_to_cell_accesses(value_container_type::map, accesses);

        auto value_callback = [&] (const lcdf::Str& key, internal_elem *e, bool& ret) {
            TransProxy row_item = index_read_my_write ? Sto::item(this, item_key_t::row_item_key(e))
                                                      : Sto::fresh_item(this, item_key_t::row_item_key(e));

            bool any_has_write;
            std::vector<TransProxy> cell_items;
            std::tie(any_has_write, cell_items) = extract_item_list(cell_accesses, e);

            if (index_read_my_write) {
                if (has_delete(row_item)) {
                    ret = true;
                    return true;
                }
                if (any_has_write) {
                    if (has_insert(row_item))
                        ret = callback(key_type(key), e->row_container.row);
                    else
                        ret = callback(key_type(key), *(row_item.template raw_write_value<value_type *>()));
                    return true;
                }
            }

            bool ok = access_all(cell_accesses, cell_items, e);
            if (!ok)
                return false;
            //bool ok = item.observe(e->version);
            //if (Adaptive) {
            //    ok = item.observe(e->version, true/*force occ*/);
            //} else {
            //    ok = item.observe(e->version);
            //}

            // skip invalid (inserted but yet committed) values, but do not abort
            if (!e->valid()) {
                ret = true;
                return true;
            }

            ret = callback(key_type(key), e->row_container.row);
            return true;
        };

        range_scanner<decltype(node_callback), decltype(value_callback), Reverse>
            scanner(end, node_callback, value_callback);
        if (Reverse)
            table_.rscan(begin, true, scanner, limit, *ti);
        else
            table_.scan(begin, true, scanner, limit, *ti);
        return scanner.scan_succeeded_;
    }

    template <typename Callback, bool Reverse>
    bool range_scan(const key_type& begin, const key_type& end, Callback callback,
                    RowAccess access, bool phantom_protection = true, int limit = -1) {
        assert((limit == -1) || (limit > 0));
        auto node_callback = [&] (leaf_type* node,
                                  typename unlocked_cursor_type::nodeversion_value_type version) {
            return ((!phantom_protection) || register_internode_version(node, version));
        };

        auto value_callback = [&] (const lcdf::Str& key, internal_elem *e, bool& ret) {
            TransProxy row_item = index_read_my_write ? Sto::item(this, item_key_t::row_item_key(e))
                                                      : Sto::fresh_item(this, item_key_t::row_item_key(e));

            if (index_read_my_write) {
                if (has_delete(row_item)) {
                    ret = true;
                    return true;
                }
                if (has_row_update(row_item)) {
                    if (has_insert(row_item))
                        ret = callback(key_type(key), e->row_container.row);
                    else
                        ret = callback(key_type(key), *(row_item.template raw_write_value<value_type *>()));
                    return true;
                }
            }

            bool ok = true;
            switch (access) {
                case RowAccess::ObserveValue:
                case RowAccess::ObserveExists:
                    ok = row_item.observe(e->version());
                    break;
                case RowAccess::None:
                    break;
                default:
                    always_assert(false, "unsupported access type in range_scan");
                    break;
            }

            if (!ok)
                return false;

            // skip invalid (inserted but yet committed) values, but do not abort
            if (!e->valid()) {
                ret = true;
                return true;
            }

            ret = callback(key_type(key), e->row_container.row);
            return true;
        };

        range_scanner<decltype(node_callback), decltype(value_callback), Reverse>
                scanner(end, node_callback, value_callback);
        if (Reverse)
            table_.rscan(begin, true, scanner, limit, *ti);
        else
            table_.scan(begin, true, scanner, limit, *ti);
        return scanner.scan_succeeded_;
    }

    value_type *nontrans_get(const key_type& k) {
        unlocked_cursor_type lp(table_, k);
        bool found = lp.find_unlocked(*ti);
        if (found) {
            internal_elem *e = lp.value();
            return &(e->row_container.row);
        } else
            return nullptr;
    }

    void nontrans_put(const key_type& k, const value_type& v) {
        cursor_type lp(table_, k);
        bool found = lp.find_insert(*ti);
        if (found) {
            internal_elem *e = lp.value();
            if (value_is_small)
                e->row_container.row = v;
            else
               copy_row(e, &v);
            lp.finish(0, *ti);
        } else {
            internal_elem *e = new internal_elem(k, v, true);
            lp.value() = e;
            lp.finish(1, *ti);
        }
    }

    // TObject interface methods
    bool lock(TransItem& item, Transaction &txn) override {
        assert(!is_internode(item));
        auto key = item.key<item_key_t>();
        auto e = key.internal_elem_ptr();
        if (key.is_row_item())
            return txn.try_lock(item, e->version());
        else
            return txn.try_lock(item, e->row_container.version_at(key.cell_num()));
    }

    bool check(TransItem& item, Transaction& txn) override {
        if (is_internode(item)) {
            node_type *n = get_internode_address(item);
            auto curr_nv = static_cast<leaf_type *>(n)->full_version_value();
            auto read_nv = item.template read_value<decltype(curr_nv)>();
            return (curr_nv == read_nv);
        } else {
            auto key = item.key<item_key_t>();
            auto e = key.internal_elem_ptr();
            if (key.is_row_item())
                return e->version().cp_check_version(txn, item);
            else
                return e->row_container.version_at(key.cell_num()).cp_check_version(txn, item);
        }
    }

    void install(TransItem& item, Transaction& txn) override {
        assert(!is_internode(item));
        auto key = item.key<item_key_t>();
        auto e = key.internal_elem_ptr();

        if (key.is_row_item()) {
            //assert(e->version.is_locked());
            if (has_delete(item)) {
                if (!has_insert(item)) {
                    assert(e->valid() && !e->deleted);
                    txn.set_version(e->version());
                    e->deleted = true;
                    fence();
                }
                return;
            }

            value_type *vptr;
            if (value_is_small) {
                vptr = &(item.write_value<value_type>());
            } else {
                vptr = item.write_value<value_type *>();
            }

            if (!has_insert(item)) {
                if (has_row_update(item)) {
                    if (value_is_small) {
                        e->row_container.row = *vptr;
                    } else {
                        copy_row(e, vptr);
                    }
                } else if (has_row_cell(item)) {
                    // install only the difference part
                    // not sure if works when there are more than 1 minor version fields
                    // should still work
                    e->row_container.install_cell(0, vptr);
                }
            }

            // like in the hashtable (unordered_index), no need for the hacks
            // treating opacity as a special case
            txn.set_version_unlock(e->version(), item);
        } else {
            // skip installation if row-level update is present
            auto row_item = Sto::item(this, item_key_t::row_item_key(e));
            if (!has_row_update(row_item)) {
                value_type *vptr;
                if (value_is_small)
                    vptr = &(row_item.template raw_write_value<value_type>());
                else
                    vptr = row_item.template raw_write_value<value_type *>();

                e->row_container.install_cell(key.cell_num(), vptr);
            }

            txn.set_version_unlock(e->row_container.version_at(key.cell_num()), item);
        }
    }

    void unlock(TransItem& item) override {
        assert(!is_internode(item));
        auto key = item.key<item_key_t>();
        auto e = key.internal_elem_ptr();
        if (key.is_row_item())
            e->version().cp_unlock(item);
        else
            e->row_container.version_at(key.cell_num()).cp_unlock(item);
    }

    void cleanup(TransItem& item, bool committed) override {
        if (committed ? has_delete(item) : has_insert(item)) {
            auto key = item.key<item_key_t>();
            assert(key.is_row_item());
            internal_elem *e = key.internal_elem_ptr();
            bool ok = _remove(e->key);
            if (!ok) {
                std::cout << committed << "," << has_delete(item) << "," << has_insert(item) << std::endl;
                always_assert(false, "insert-bit exclusive ownership violated");
            }
            item.clear_needs_unlock();
        }
    }

protected:
    template <typename NodeCallback, typename ValueCallback, bool Reverse>
    class range_scanner {
    public:
        range_scanner(const Str upper, NodeCallback ncb, ValueCallback vcb) :
            boundary_(upper), boundary_compar_(false), scan_succeeded_(true),
            node_callback_(ncb), value_callback_(vcb) {}

        template <typename ITER, typename KEY>
        void check(const ITER& iter, const KEY& key) {
            int min = std::min(boundary_.length(), key.prefix_length());
            int cmp = memcmp(boundary_.data(), key.full_string().data(), min);
            if (!Reverse) {
                if (cmp < 0 || (cmp == 0 && boundary_.length() <= key.prefix_length()))
                    boundary_compar_ = true;
                else if (cmp == 0) {
                    uint64_t last_ikey = iter.node()->ikey0_[iter.permutation()[iter.permutation().size() - 1]];
                    uint64_t slice = string_slice<uint64_t>::make_comparable(boundary_.data() + key.prefix_length(),
                        std::min(boundary_.length() - key.prefix_length(), 8));
                    boundary_compar_ = (slice <= last_ikey);
                }
            } else {
                if (cmp >= 0)
                    boundary_compar_ = true;
            }
        }

        template <typename ITER>
        void visit_leaf(const ITER& iter, const Masstree::key<uint64_t>& key, threadinfo&) {
            if (!node_callback_(iter.node(), iter.full_version_value())) {
                scan_succeeded_ = false;
            }
            if (this->boundary_) {
                check(iter, key);
            }
        }

        bool visit_value(const Masstree::key<uint64_t>& key, internal_elem *e, threadinfo&) {
            if (this->boundary_compar_) {
                if ((Reverse && (boundary_ >= key.full_string())) ||
                    (!Reverse && (boundary_ <= key.full_string())))
                    return false;
            }
            bool visited = false;
            if (!value_callback_(key.full_string(), e, visited)) {
                scan_succeeded_ = false;
                return false;
            } else {
                if (!visited)
                    scan_succeeded_ = false;
                return visited;
            }
        }

        Str boundary_;
        bool boundary_compar_;
        bool scan_succeeded_;

        NodeCallback node_callback_;
        ValueCallback value_callback_;
    };

private:
    table_type table_;
    uint64_t key_gen_;

    std::pair<bool, std::vector<TransProxy>>
    extract_item_list(const std::vector<cell_access_t>& cell_accesses, internal_elem *e) {
        bool any_has_write = false;
        std::vector<TransProxy> cell_items;
        cell_items.reserve(cell_accesses.size());
        for (auto& ca : cell_accesses) {
            auto item = Sto::item(this, item_key_t(e, ca.cell_id));
            if (index_read_my_write && !any_has_write && item.has_write())
                any_has_write = true;
            cell_items.push_back(std::move(item));
        }
        return {any_has_write, cell_items};
    }

    static bool
    access_all(std::vector<cell_access_t>& cell_accesses, std::vector<TransProxy>& cell_items, internal_elem *e) {
        for (auto it = cell_items.begin(); it != cell_items.end(); ++it) {
            auto idx = it - cell_items.begin();
            auto& access = cell_accesses[idx];
            if (access.update) {
                if (!version_adapter::select_for_update(*it, e->row_container.version_at(access.cell_id)))
                    return false;
                if (it->item().key<item_key_t>().is_row_item()) {
                    it->item().add_flags(row_cell_bit);
                }
            } else {
                if (!it->observe(e->row_container.version_at(access.cell_id)))
                    return false;
            }
        }
        return true;
    }

    static bool has_insert(const TransItem& item) {
        return (item.flags() & insert_bit) != 0;
    }
    static bool has_delete(const TransItem& item) {
        return (item.flags() & delete_bit) != 0;
    }
    static bool has_row_update(const TransItem& item) {
        return (item.flags() & row_update_bit) != 0;
    }
    static bool has_row_cell(const TransItem& item) {
        return (item.flags() & row_cell_bit) != 0;
    }
    static bool is_phantom(internal_elem *e, const TransItem& item) {
        return (!e->valid() && !has_insert(item));
    }

    bool register_internode_version(node_type *node, nodeversion_value_type nodeversion) {
        TransProxy item = Sto::item(this, get_internode_key(node));
        if (DBParams::Opaque)
            return item.add_read_opaque(nodeversion);
        else
            return item.add_read(nodeversion);
    }
    bool update_internode_version(node_type *node,
            nodeversion_value_type prev_nv, nodeversion_value_type new_nv) {
        TransProxy item = Sto::item(this, get_internode_key(node));
        if (!item.has_read()) {
            return true;
        }
        if (prev_nv == item.template read_value<nodeversion_value_type>()) {
            item.update_read(prev_nv, new_nv);
            return true;
        }
        return false;
    }

    bool _remove(const key_type& key) {
        cursor_type lp(table_, key);
        bool found = lp.find_locked(*ti);
        if (found) {
            internal_elem *el = lp.value();
            lp.finish(-1, *ti);
            Transaction::rcu_delete(el);
        } else {
            // XXX is this correct?
            lp.finish(0, *ti);
        }
        return found;
    }

    static uintptr_t get_internode_key(node_type* node) {
        return reinterpret_cast<uintptr_t>(node) | internode_bit;
    }
    static bool is_internode(TransItem& item) {
        return (item.key<uintptr_t>() & internode_bit) != 0;
    }
    static node_type *get_internode_address(TransItem& item) {
        assert(is_internode(item));
        return reinterpret_cast<node_type *>(item.key<uintptr_t>() & ~internode_bit);
    }

    static void copy_row(internal_elem *e, const value_type *new_row) {
        if (new_row == nullptr)
            return;
        e->row_container.row = *new_row;
    }
};

template <typename K, typename V, typename DBParams>
__thread typename ordered_index<K, V, DBParams>::table_params::threadinfo_type
*ordered_index<K, V, DBParams>::ti;

}; // namespace bench
