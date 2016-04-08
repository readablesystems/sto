// ListS.hh
// Singly-linked list with StoSnapshot support
// Supports an STL map-style interface
// XXX no iterator for now

#include "TaggedLow.hh"
#include "Interface.hh"
#include "Transaction.hh"
#include "TWrapped.hh"
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

template <typename K, typename V>
class ListProxy;

template <typename K, typename V>
class ListNode {
public:
    template <typename KK, typename VV, typename C>
    friend class List;

    template <typename KK, typename VV>
    friend class ListProxy;

    typedef StoSnapshot::ObjectID<ListNode<K, V>> oid_type;
    typedef StoSnapshot::sid_type sid_type;

    static constexpr TransactionTid::type invalid_bit = TransactionTid::user_bit;

    ListNode(const K& key, const V& val, oid_type next_id, bool invalid)
    : key(key), val(val), next_id(next_id),
      vers(Sto::initialized_tid() | (invalid ? (invalid_bit | TransactionTid::lock_bit | TThread::id()) : 0)) {}

    void mark_invalid() {
        TVersion newv = vers | invalid_bit;
        fence();
        vers = newv;
    }
    TVersion& version() {
        return vers;
    }
    void set_version(const TVersion& new_v) {
        vers.set_version(new_v);
    }
    void set_version_unlock(const TVersion& new_v) {
        vers.set_version_unlock(new_v);
    }
    bool try_lock() {
        return vers.try_lock();
    }
    void unlock() {
        vers.unlock();
    }
    bool is_valid() {
        return !(vers.value() & invalid_bit);
    }

protected:
    oid_type next_id;
    K key;
    TWrapped<V> val;
    TVersion vers;
};

// proxy type for List::operator[]
template <typename K, typename V>
class ListProxy {
public:
    typedef ListNode<K, V> node_type;
    typedef StoSnapshot::NodeWrapper<node_type> wrapper_type;

    explicit ListProxy(node_type* node) noexcept : node_(node) {}

    // transactional non-snapshot read
    operator V() {
        auto item = Sto::item(this,
                        reinterpret_cast<wrapper_type*>(node_)->oid);
        if (item.has_write()) {
            return item.template write_value<V>();
        } else {
            return node_->val.read(node_->version());
        }
    }

    // transactional assignment
    ListProxy& operator=(const V& other) {
        Sto::item(this, node_).add_write(other);
        return *this;
    }

    ListProxy& operator=(const ListProxy& other) {
        Sto::item(this, node_).add_write((V)other);
        return *this;
    }
private:
    node_type* node_;
};

// always sorted, no duplicates
template <typename K, typename V, typename Compare>
class List : public TObject {
public:
    typedef ListNode<K, V> node_type;
    typedef StoSnapshot::ObjectID<node_type> oid_type;
    typedef StoSnapshot::NodeWrapper<node_type> wrapper_type;
    typedef StoSnapshot::sid_type sid_type;

    enum class TxnStage {execution, commit, cleanup, none};

    static constexpr uintptr_t list_key = 0;
    static constexpr uintptr_t size_key = 1;

    static constexpr TransItem::flags_type insert_tag = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_tag = TransItem::user0_bit<<1;

    List(Compare comp = Compare())
    : head_id_(nullptr), listlock_(0), listversion_(0), comp_(comp) {}

    std::pair<bool, V> nontrans_find(const K& k, sid_type sid) {
        node_type* n = _find(k, sid); 
        return std::make_pair(n, n ? n->val.access() : V());
    }

    std::pair<bool, V> trans_find(const K& k) {
        sid_type sid = Sto::active_sid();
        if (sid) {
            // snapshot reads are safe as nontrans
            return nontrans_find(k, sid);
        }
        // making sure size version hasn't changed
        std::pair<bool, V> ret_pair(false, V());
        TVersion lv = listversion_;
        fence();

        node_type* n =  _find(k, sid);
        oid_type oid = reinterpret_cast<wrapper_type*>(n)->oid;
        
        if (n) {
            auto item = Sto::item(this, oid);
            if (!validityCheck(n, item)) {
                Sto::abort();
            }
            if (has_delete(item)) {
                return ret_pair;
            }
            ret_pair.first = true;
            ret_pair.second = n->val.read(item, n->version());
        } else {
            Sto::item(this, list_key).observe(lv);
        }
        return ret_pair;
    }

    ListProxy<K, V> operator[](const K& key) {
        return ListProxy<K, V>(insert_position(key));
    }

    // "STAMP"-ish insert
    bool trans_insert(const K& key, const V& value) {
        lock(listlock_);
        auto results = _insert(key, V());
        unlock(listlock_);

        auto item = Sto::item(this,
                        reinterpret_cast<wrapper_type*>(results.second)->oid);
        if (results.first) {
            // successfully inserted
            auto litem = Sto::item(this, list_key);
            litem.add_write(0);

            item.add_write(value);
            item.add_flags(insert_tag);
            return true;
        }

        // not inserted if we fall through
        node_type* node = results.second;
        TVersion ver = node->version();
        fence();
        bool poisoned = !node->is_valid();
        if (!item.has_write() && poisoned) {
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
        TVersion lv = listversion_;
        fence();

        node_type* n = _find(key, 0);
        if (n == nullptr) {
            auto item = Sto::item(this, list_key);
            item.observe(lv);
            return false;
        }

        bool poisoned = !n->is_valid();
        auto item = Sto::item(this,
                        reinterpret_cast<wrapper_type*>(n)->oid);
        if (!has_write(item) && poisoned) {
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
            item.observe(lv);
        }
        return true;
    }

    bool lock(TransItem& item, Transaction&) override {
        uintptr_t n = item.key<uintptr_t>();
        if (n == list_key) {
            return listversion_.try_lock();
        } else if (!has_insert(item)) {
            return oid_type(n).base_ptr()->node().try_lock();
        }
        return true;
    }

    void unlock(TransItem& item) override {
        uintptr_t n = item.key<uintptr_t>();
        if (n == list_key) {
            return listversion_.unlock();
        } else if (!has_insert(item)) {
            oid_type(n).base_ptr()->node().unlock();
        }
    }

    bool check(TransItem& item, Transaction&) override {
        uintptr_t n = item.key<uintptr_t>();
        if (n == list_key) {
            auto lv = listversion_;
            return lv.check_version(item.template read_value<TVersion>());
        }
        node_type *nn = &(reinterpret_cast<oid_type>(n).base_ptr()->node());
        if (!nn->is_valid()) {
            return has_insert(item);
        }
        return nn->version().check_version(item.template read_value<TVersion>());
    }

    void install(TransItem& item, Transaction& t) override {
        if (item.key<uintptr_t>() == list_key) {
            listversion_.set_version(t.commit_tid());
            return;
        }
        oid_type oid = item.key<oid_type>();
        auto bp = oid.base_ptr();

        // copy-on-write for both deletes and updates
        if (bp->is_immutable())
            bp->save_copy_in_history();

        if (has_delete(item)) {
            bp->set_unlinked();
            reinterpret_cast<wrapper_type*>(&bp->node())->s_sid = 0;
        } else {
            bp->node().val.write(item.template write_value<V>());
            bp->node().set_version_unlock(t.commit_tid());
        }
    }

    void cleanup(TransItem& item, bool committed) override {
        if (has_insert(item)) {
            if (!committed || (committed && has_delete(item))) {
                lock(listlock_);
                _remove(item.key<oid_type>().base_ptr()->node().key, TxnStage::cleanup);
                unlock(listlock_);
            }
        }
    }

private:
    // returns the node the key points to, or abort if observes uncommitted state
    node_type* insert_position(const K& key) {
        lock(listlock_);
        auto results = _insert(key, V());
        unlock(listlock_);

        if (results.first) {
            return results.second;
        }
        auto item = Sto::item(this,
                        reinterpret_cast<wrapper_type*>(results.second)->oid);
        bool poisoned = !results.second->is_valid();
        if (!item.has_write() && poisoned) {
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

    node_type* _find(const K& key, sid_type sid) {
        node_type* cur = next_snapshot(head_id_, sid);
        bool cur_history = false;
        while (cur) {
            // to see if @cur is part of the history list
            cur_history = reinterpret_cast<wrapper_type*>(cur)->c_sid != 0;

            int c = comp_(cur->key, key);
            if (c == 0) {
                return cur;
            } else if (c > 0) {
                return nullptr;
            }

            // skip nodes that are not part of the snapshot we look for
            oid_type nid = cur->next_id;
            node_type* next = next_snapshot(nid, sid);

            // direct-link two immutable nodes if detected
            if (cur_history &&
                (!next || reinterpret_cast<wrapper_type*>(next)->c_sid)) {
                cur->next_id.set_direct_link(next);
            }

            cur = next;
        }
        return cur;
    }

    // XXX note to cleanup: never structurally remove a node until history is empty
    // needs lock
    std::pair<bool, node_type*> _insert(const K& key, const V& value) {
        oid_type prev_obj = nullptr;
        oid_type cur_obj = head_id_;

        while (!cur_obj.is_null()) {
            // it's okay to always look at the root level
            node_type* cur = cur_obj.deref(0).second;
            int c = comp_(cur->key, key);
            if (c == 0) {
                if (cur_obj.base_ptr()->is_unlinked()) {
                    // reuse root-level deleted node
                    oid_type nid = cur->next_id;
                    new (cur) node_type(key, value, nid, true);
                    cur_obj.base_ptr()->clear_unlinked();

                    return std::make_pair(true, cur);
                }
                return std::make_pair(false, cur);
            } else if (c > 0) {
                // insertion point
                break;
            }
            prev_obj = cur_obj;
            cur_obj = cur->next_id;
        }

        // allocate a new object for the newly inserted node (key)
        auto new_obj = StoSnapshot::new_object<node_type>();
        oid_type new_id = new_obj.first;
        node_type* new_node = new_obj.second;

        new (new_node) node_type(key, value, cur_obj, true);

        if (!prev_obj.is_null()) {
            prev_obj.deref(0).second->next_id = new_id;
        } else {
            head_id_ = new_id;
        }

        return std::make_pair(true, new_node);
    }

    // needs lock
    bool _remove(const K& key, TxnStage stage) {
        oid_type prev_obj = nullptr;
        oid_type cur_obj = head_id_;
        while (!cur_obj.is_null()) {
            node_type* cur = cur_obj.deref(0).second;
            auto bp = cur_obj.base_ptr();
            if (comp_(cur->key, key) == 0) {
                if (stage == TxnStage::commit) {
                    assert(cur->is_valid());
                    assert(!bp->is_unlinked());
                    // never physically unlink the object since it must contain snapshots!
                    // (the one we just saved!)
                    bp->save_copy_in_history();
                    cur->mark_invalid();
                    bp->set_unlinked();
                } else if (stage == TxnStage::cleanup) {
                    assert(!cur->is_valid());
                    assert(!bp->is_unlinked());
                    if (bp->history_is_empty()) {
                        // unlink the entire object if that's all we've instered
                        _do_unlink(prev_obj, cur_obj, stage);
                    } else {
                        cur->mark_invalid();
                        bp->set_unlinked();
                    }
                } else if (stage == TxnStage::none) {
                    _do_unlink(prev_obj, cur_obj, stage);
                }
                return true;
            }
            prev_obj = cur_obj;
            cur_obj = cur->next_id;
        }
        return false;
    }

    inline void _do_unlink(oid_type& prev_obj, oid_type cur_obj, TxnStage stage) {
        node_type* cur = cur_obj.deref(0).second;
        if (!prev_obj.is_null()) {
            prev_obj.deref(0).second->next_id = cur->next_id;
        } else {
            head_id_ = cur->next_id;
        }

        auto bp = cur_obj.base_ptr();
        if (stage == TxnStage::none) {
            delete cur;
            delete bp;
        } else {
            Transaction::rcu_delete(cur);
            Transaction::rcu_delete(bp);
        }
    }

    // skip nodes that are not part of the snapshot we look for
    node_type* next_snapshot(oid_type start, sid_type sid) {
        oid_type cur_obj = start;
        node_type* ret = nullptr;
        while (sid) {
            if (cur_obj.is_null()) {
                break;
            }
            // ObjectID::deref returns <next, root(if next not found given sid)>
            auto n = cur_obj.deref(sid);
            if (!n.first) {
                // object id exists but no snapshot is found at sid
                // this should cover nodes that are more up-to-date than sid (when doing a snapshot read)
                // or when the node is unlinked (when doing a non-snapshot read, or sid==0)
                cur_obj = n.second->next_id;
            } else {
                // snapshot node found
                ret = n.first;
                break;
            }
        }
        return ret;
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
