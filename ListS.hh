// ListS.hh
// Singly-linked list with StoSnapshot support
// Supports an STL map-style interface
// XXX no iterator for now

#include "TaggedLow.hh"
#include "Interface.hh"
#include "Transaction.hh"
#include "Snapshot.hh"

template <typename T>
class DefaultCompare {
public:
    int operator()(const T& t1, const T& t2) {
        if (t1 < t2)
            return -1;
        return t2 < t1 ? 1 : 0;
    }
};

template <typename K, typename V, typename Compare=DefaultCompare<K>>
class List;

template <typename K, typename V, typename Compare>
class ListCursor;

template <typename K, typename V>
class ListProxy;

template <typename K, typename V>
class ListNode {
public:
    typedef StoSnapshot::sid_type sid_type;
    ListNode() : key(), val() {}
    ListNode(const K& key, const V& val) : key(key), val(val) {}

    K key;
    V val;
};

template <typename K, typename V>
class ListLink {
public:
    typedef StoSnapshot::ObjectID<ListNode<K, V>, ListLink<K, V>> oid_type;
    static constexpr uintptr_t use_base = 0x1;
    oid_type next_id;

    ListLink() : next_id(use_base) {}
    ListLink(oid_type next) : next_id(next) {}
};

// proxy type for List::operator[]
template <typename K, typename V>
class ListProxy {
public:
    typedef List<K, V> list_type;
    typedef ListNode<K, V> node_type;
    typedef ListLink<K, V> link_type;
    typedef StoSnapshot::ObjectID<node_type, link_type> oid_type;
    typedef StoSnapshot::NodeWrapper<node_type, link_type> wrapper_type;

    explicit ListProxy(list_type& l, oid_type ins_oid) noexcept : list(l), oid(ins_oid) {}

    // transactional non-snapshot read
    operator V() {
        auto item = Sto::item(&list, oid.value());
        if (item.has_write()) {
            return item.template write_value<V>();
        } else {
            return list.atomic_rootlv_value_by_oid(item, oid);
        }
    }

    // transactional assignment
    ListProxy& operator=(const V& other) {
        Sto::item(&list, oid.value()).add_write(other);
        return *this;
    }

    ListProxy& operator=(const ListProxy& other) {
        Sto::item(&list, oid.value()).add_write((V)other);
        return *this;
    }
private:
    list_type& list;
    oid_type oid;
};

// always sorted, no duplicates
template <typename K, typename V, typename Compare>
class List : public TObject {
public:
    typedef ListNode<K, V> node_type;
    typedef ListLink<K, V> link_type;
    typedef StoSnapshot::ObjectID<node_type, link_type> oid_type;
    typedef StoSnapshot::NodeWrapper<node_type, link_type> wrapper_type;
    typedef StoSnapshot::sid_type sid_type;
    typedef ListCursor<K, V, Compare> cursor_type;

    friend cursor_type;

    enum class TxnStage {execution, commit, cleanup, none};

    static constexpr uintptr_t list_key = 0;
    static constexpr uintptr_t size_key = 1;

    static constexpr TransactionTid::type poisoned_bit = TransactionTid::user_bit;
    static constexpr TransItem::flags_type insert_tag = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_tag = TransItem::user0_bit<<1;

    List(Compare comp = Compare())
    : head_id_(nullptr), listlock_(0), listversion_(0), comp_(comp) {}

    std::pair<bool, V> nontrans_find(const K& k, sid_type sid) {
        cursor_type cursor(*this);
        bool found = cursor.find(k, sid);
        return std::make_pair(found, found ? cursor.last_node->val : V());
    }

    V atomic_rootlv_value_by_oid(TransProxy& item, oid_type oid) {
        auto bp = oid.base_ptr();
        TVersion& r_ver = bp->version();
        return bp->atomic_root_wrapper(item, r_ver)->node().val;
    }

    std::pair<bool, V> trans_find(const K& k) {
        sid_type sid = Sto::active_sid();
        if (sid != Sto::disable_snapshot) {
            // snapshot reads are safe as nontrans
            return nontrans_find(k, sid);
        }

        // being conservative: making sure listversion_ doesn't change
        std::pair<bool, V> ret_pair(false, V());
        cursor_type cursor(*this);
        TVersion lv = listversion_;
        fence();

        bool found = cursor.find(k, sid);
        
        if (found) {
            auto item = Sto::item(this, cursor.last_oid.value());
            TVersion& r_ver = cursor.last_oid.base_ptr()->version();
            if (is_poisoned(r_ver) && !has_insert(item)) {
                std::cout << "poisoned in trans_find" << std::endl;
                Sto::abort();
            }
            if (has_delete(item)) {
                return ret_pair;
            }
            ret_pair.first = true;
            ret_pair.second = atomic_rootlv_value_by_oid(item, cursor.last_oid);
        } else {
            Sto::item(this, list_key).observe(lv);
        }
        return ret_pair;
    }

    ListProxy<K, V> operator[](const K& key) {
        return ListProxy<K, V>(*this, insert_position(key));
    }

    // "STAMP"-ish insert
    bool trans_insert(const K& key, const V& value) {
        lock(listlock_);
        auto results = _insert(key, V());
        unlock(listlock_);

        auto item = Sto::item(this, results.second.value());

        if (results.first) {
            // successfully inserted
            auto litem = Sto::item(this, list_key);
            litem.add_write(0);

            item.add_write(value);
            item.add_flags(insert_tag);
            return true;
        }

        // not inserted if we fall through
        auto bp = results.second.base_ptr();
        TVersion ver = bp->version();
        fence();
        bool poisoned = is_poisoned(ver);
        if (!item.has_write() && poisoned) {
            std::cout << "poisoned in trans_insert" << std::endl;
            Sto::abort();
        } else if (has_delete(item)) {
            item.clear_flags(delete_tag);
            if (poisoned) {
                // THIS txn inserted this node, now cleanup
                item.add_flags(insert_tag);
            }
            item.add_write(value);
            return true;
        } else if (has_insert(item)) {
            return false;
        }

        item.observe(ver);
        return false;
    }

    bool trans_erase(const K& key) {
        cursor_type cursor(*this);
        TVersion lv = listversion_;
        fence();

        bool found = cursor.find(key, Sto::disable_snapshot);
        if (!found) {
            auto item = Sto::item(this, list_key);
            item.observe(lv);
            return false;
        }

        TVersion nv = cursor.last_oid.base_ptr()->version();
        bool poisoned = is_poisoned(nv);
        auto item = Sto::item(this, cursor.last_oid.value());
        if (!item.has_write() && poisoned) {
            std::cout << "poisoned in trans_erase" << std::endl;
            Sto::abort();
        }
        if (has_delete(item)) {
            return false;
        } else if (has_insert(item)) {
            // deleting a key inserted by THIS transaction
            // defer cleanups to cleanup()
            item.remove_read().add_flags(delete_tag);
            // XXX insert_tag && delete_tag means needs cleanup after committed
        } else {
            // increment list version
            auto litem = Sto::item(this, list_key);
            litem.add_write(0);

            item.assign_flags(delete_tag);
            item.add_write(0);
            item.observe(nv);
        }
        return true;
    }

    bool lock(TransItem& item, Transaction&) override {
        uintptr_t n = item.key<uintptr_t>();
        if (n == list_key) {
            return listversion_.try_lock();
        } else if (!has_insert(item)) {
            return oid_type(n).base_ptr()->version().try_lock();
        }
        return true;
    }

    void unlock(TransItem& item) override {
        uintptr_t n = item.key<uintptr_t>();
        if (n == list_key) {
            return listversion_.unlock();
        } else if (!has_insert(item)) {
            oid_type(n).base_ptr()->version().unlock();
        }
    }

    bool check(TransItem& item, Transaction&) override {
        uintptr_t n = item.key<uintptr_t>();
        if (n == list_key) {
            TVersion lv = listversion_;
            return lv.check_version(item.template read_value<TVersion>());
        }
        TVersion vers = oid_type(n).base_ptr()->version();
        if (is_poisoned(vers)) {
            if (!has_insert(item)) {
                std::cout << "poisoned in check" << std::endl;
                return false;
            }
            return true;
        }
        return vers.check_version(item.template read_value<TVersion>());
    }

    void install(TransItem& item, Transaction& t) override {
        if (item.key<uintptr_t>() == list_key) {
            listversion_.set_version(t.commit_tid());
            return;
        }
        oid_type oid(item.key<uintptr_t>());
        auto bp = oid.base_ptr();

        // copy-on-write for both deletes and updates
        if (bp->is_immutable())
            bp->save_copy_in_history();

        auto rw = bp->root_wrapper();
        if (has_delete(item)) {
            bp->set_unlinked();
            rw->deleted = true;
        } else {
            bp->node().val = item.template write_value<V>();
        }
        rw->sid = t.commit_tid();

        if (has_insert(item))
            bp->version().set_version_unlock(t.commit_tid());
        else
            bp->version().set_version(t.commit_tid());
    }

    void cleanup(TransItem& item, bool committed) override {
        if (has_insert(item)) {
            if (!committed || (committed && has_delete(item))) {
                lock(listlock_);
                _remove(oid_type(item.key<uintptr_t>()).base_ptr()->node().key, TxnStage::cleanup);
                unlock(listlock_);
            }
        }
    }

private:
    // returns the node the key points to, or abort if observes uncommitted state
    oid_type insert_position(const K& key) {
        lock(listlock_);
        auto results = _insert(key, V());
        unlock(listlock_);

        auto item = Sto::item(this, results.second.value());
        auto bp = results.second.base_ptr();
        bool poisoned = is_poisoned(bp->version());
        if (results.first) {
            item.add_write(V());
            if (poisoned) {
                item.add_flags(insert_tag);
            }
            return results.second;
        }

        if (!item.has_write() && poisoned) {
            std::cout << "poisoned in insert_position" << std::endl;
            Sto::abort();
        } else if (has_delete(item)) {
            item.clear_flags(delete_tag);
            if (poisoned) {
                item.add_flags(insert_tag);
            }
            item.add_write(V());
        }
        return results.second;
    }

    // XXX note to cleanup: never structurally remove a node until history is empty
    // needs lock
    std::pair<bool, oid_type> _insert(const K& key, const V& value) {
        oid_type prev_obj = nullptr;
        oid_type cur_obj = head_id_;

        cursor_type cursor(*this);

        // all the magic in one line :D
        bool found = cursor.find(key, Sto::disable_snapshot);

        auto cbp = cursor.last_oid.base_ptr();
        auto node = cursor.last_node;
        if (found && cbp->is_unlinked()) {
            // found an unlinked object with the same key, reuse this object
            mark_poisoned_init(cbp->version());
            new (node) node_type(key, value);
            cbp->clear_unlinked();

            return std::make_pair(true, cursor.last_oid);
        } else if (found) {
            return std::make_pair(false, cursor.last_oid);
        }

        // if we fall through we have to insert a new object
        // prev_oid, last_oid is the insertion point

        auto new_obj = StoSnapshot::new_object<node_type, link_type>();
        auto new_bp = new_obj.first.base_ptr();
        new_bp->links.next_id = cursor.last_oid;
        mark_poisoned_init(new_bp->version());
        new (new_obj.second) node_type(key, value);

        if (!cursor.prev_oid.is_null()) {
            cursor.prev_oid.base_ptr()->links.next_id = new_obj.first;
        } else {
            head_id_ = new_obj.first;
        }

        return std::make_pair(true, new_obj.first);
    }

    // needs lock
    bool _remove(const K& key, TxnStage stage) {
        cursor_type cursor(*this);
        bool found = cursor.find(key, Sto::disable_snapshot);
        if (found) {
            auto bp = cursor.last_oid.base_ptr();
            if (stage == TxnStage::cleanup) {
                assert(!bp->is_unlinked());
                if (bp->history_is_empty()) {
                    _do_unlink(cursor.prev_oid, cursor.last_oid, stage);
                }
            } else {
                assert(stage == TxnStage::none);
                _do_unlink(cursor.prev_oid, cursor.last_oid, stage);
            }
            return true;
        }
        return false;
    }

    inline void _do_unlink(oid_type& prev_obj, oid_type cur_obj, TxnStage stage) {
        auto pbp = prev_obj.base_ptr();
        auto cbp = cur_obj.base_ptr();
        if (!prev_obj.is_null()) {
            pbp->links.next_id = cbp->links.next_id;
        } else {
            head_id_ = cbp->links.next_id;
        }

        if (stage == TxnStage::none) {
            delete cbp->root_wrapper();
            delete cbp;
        } else {
            Transaction::rcu_delete(cbp->root_wrapper());
            Transaction::rcu_delete(cbp);
        }
    }

    static inline void mark_poisoned(TVersion& ver) {
        TVersion newv = ver | poisoned_bit;
        fence();
        ver = newv;
    }

    static inline void mark_poisoned_init(TVersion& ver) {
        ver = (Sto::initialized_tid() | (poisoned_bit | TransactionTid::lock_bit | TThread::id()));
    }

    static inline bool is_poisoned(TVersion& ver) {
        return (ver.value() & poisoned_bit);
    }

    static void lock(TVersion& ver) {
        ver.lock();
    }

    static void unlock(TVersion& ver) {
        ver.unlock();
    }

    static bool has_insert(const TransItem& item) {
        return item.flags() & insert_tag;
    }

    static bool has_delete(const TransItem& item) {
        return item.flags() & delete_tag;
    }

    oid_type head_id_;
    TVersion listlock_;
    TVersion listversion_;
    Compare comp_;
};

// ListCursor is read-only and can be used lock-free if not looking for insertion points
template <typename K, typename V, typename Compare>
class ListCursor {
public:
    typedef List<K, V, Compare> list_type;
    typedef ListNode<K, V> node_type;
    typedef ListLink<K, V> link_type;
    typedef StoSnapshot::ObjectID<node_type, link_type> oid_type;
    typedef StoSnapshot::NodeWrapper<node_type, link_type> wrapper_type;
    typedef StoSnapshot::sid_type sid_type;

    oid_type prev_oid;
    oid_type last_oid;
    wrapper_type* prev_node;
    wrapper_type* last_node;

    ListCursor(list_type& l) : prev_oid(nullptr, false), last_oid(nullptr, false),
        prev_node(nullptr), last_node(nullptr), list(l) {}

    void reset() {
        prev_oid = nullptr;
        last_oid = nullptr;
        prev_node = last_node = nullptr;
    }

    // find the node containing the key at the given sid
    // only searches at the root level if @sid == Sto::disable_snapshot
    // this will return unlinked nodes!
    bool find(const K& key, sid_type sid) {
        bool in_snapshot = false;
        last_oid = list.head_id_;
        last_node = last_oid.is_null() ? nullptr : last_oid.base_ptr()->root_wrapper();
        while (last_node) {
            int c = list.comp_(last_node->node().key, key);
            if (c == 0) {
                return (!in_snapshot || !last_node->deleted);
            } else if (c > 0) {
                // prev_oid, last_oid indicates (structural) insertion point!
                return false;
            }

            // cut the short path if we have a lazy direct link available
            bool direct_link = !link_use_base(last_node->links.next_id);
            oid_type cur_oid = last_oid;
            wrapper_type* cur_node = last_node;
            if (direct_link) {
                last_node = last_node->links.next_id.wrapper_ptr();
                last_oid = last_node->oid;
                in_snapshot = true;
                continue;
            }

            // skip nodes not part of the snapshot we look for
            //oid_type nid = last_oid.base_ptr()->links.next_id;
            bool next_in_snapshot = next_snapshot(last_oid, sid);

            // lazily fix up direct links when possible
            if (in_snapshot && (!last_node || next_in_snapshot)) {
                assert(cur_node->links.next_id.value() == link_type::use_base);
                cur_node->links.next_id.set_direct_link(last_node);
            }

            in_snapshot = next_in_snapshot;
        }

        return false;
    }
private:
    // skip nodes that are not part of the snapshot (based on sid) we look for
    // modifies last_oid and last_node: not found if last_node == nullptr
    // return value indicates whether the returned node is part of a history list (immutable)
    bool next_snapshot(oid_type start, sid_type sid) {
        prev_oid = start;
        auto cbp = prev_oid.base_ptr();

        // always return false if we are not doing a snapshot search
        if (sid == Sto::disable_snapshot) {
            cbp = prev_oid.base_ptr();
            if (!prev_oid.is_null()) {
                prev_node = cbp->root_wrapper();
                // no need to skip unlinked nodes
                last_oid = cbp->links.next_id;
                if (last_oid.is_null()) {
                    last_node = nullptr;
                    return false;
                } else {
                    cbp = last_oid.base_ptr();
                    last_node = cbp->root_wrapper();
                }
            } else {
                this->reset();
                return false;
            }

            return false;
        }

        // we are doing a snapshot search here; need to search through history in this case
        // not using prev_* fields here since we can't insert
        last_oid = start;
        while (!last_oid.is_null()) {
            cbp = last_oid.base_ptr();
            // ObjectID::deref searches through all available history (include root level)
            // and returns the wrapper containing the matching sid, if any
            last_node = last_oid.deref(sid);
            if (last_node == nullptr) {
                // ObjectID exists but no snapshot is found at sid
                last_oid = cbp->links.next_id;
            } else {
                // a wrapper is found with the exact match
                // the corresponding node is "in snapshot" (immutable), thus returning true
                return true;
            }
        }

        // maybe we can say that nullptr is always in snapshot?
        this->reset();
        return false;
    }

    static inline bool link_use_base(const oid_type& next_id) {
        return (next_id.value() == link_type::use_base);
    }

    list_type& list;
};
