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
class ListNode {
public:
    template <typename KK, typename VV, typename C>
    friend class List;

    typedef StoSnapshot::ObjectID<ListNode<K, V>> oid_type;
    typedef StoSnapshot::sid_type sid_type;

    static constexpr TransactionTid::type invalid_bit = TransactionTid::user_bit;

    ListNode(const K& key, const V& val, ListNode *next, bool invalid)
    : key(key), val(val), next_id(next, false),
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

// always sorted, no duplicates
template <typename K, typename V, typename Compare>
class List : public TObject {
public:
    typedef ListNode<K, V> node_type;
    typedef StoSnapshot::ObjectID<node_type> oid_type;
    typedef StoSnapshot::NodeWrapper<node_type> wrapper_type;
    typedef StoSnapshot::sid_type sid_type;

    static constexpr uintptr_t list_key = 0;
    static constexpr uintptr_t size_key = 1;

    List(Compare comp = Compare())
    : head_id_(nullptr), listlock_(0), listversion_(0), comp_(comp) {}

    std::pair<bool, V> nontrans_find(const K& k, sid_type sid) {
        node_type* n = _find(k, sid); 
        return std::make_pair(n, n ? n->val.access() : V());
    }

    std::pair<bool, V> find(const K& k) {
        sid_type sid = Sto::active_snapshot();
        if (sid) {
            // snapshot reads are safe as nontrans
            return nontrans_find(k, sid);
        }
        // making sure size version hasn't changed
        std::pair<bool, V> ret_pair(false, V());
        TVersion lv = listversion_;
        fence();

        node_type* n =  _find(k, sid);
        
        if (n) {
            auto item = Sto::item(this, n);
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

private:
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

    oid_type head_id_;
    TVersion listlock_;
    TVersion listversion_;
    Compare comp_;
};
