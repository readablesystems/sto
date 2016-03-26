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
}

template <typename K, typename V, typename Compare=DefaultCompare<K>>
class List;

template <typename K, typename V>
class ListNode {
public:
    template <typename KK, typename VV, typename C>
    friend class List;

    typedef StoSnapshot::ObjectId<ListNode<K, V>> oid_type;
    typedef StoSnapshot::sid_type sid_type;

    ListNode(const K& key, const V& val, List_Node *next, bool invalid)
    : key(key), val(val), next_id(next, false),
      vers(Sto::initialized_tid() | (invalid ? (invalid_bit | TransactionTid::lock_bit | TThread:id()) : 0)) {}

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
template <typename T, typename V, typename Compare = DefaultCompare<T>>
class List : public TObject {
public:
    typedef ListNode<K, V> node_type;
    typedef StoSnapshot::ObjectId<node_type> oid_type;
    typedef StoSnapshot::sid_type sid_type;

    static constexpr TransactionTid::type invalid_bit = TransactionTid::user_bit;
    static constexpr uintptr_t list_key = 0;
    static constexpr uintptr_t size_key = 1;

    List(Compare comp = Compare())
    : head_(NULL), listvesion_(0), sizeversion_(0), comp_(comp) {}

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
        TVersion szv = sizeversion_;
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
            Sto::item(this, list_key).observe(szv);
        }
        return ret_pair;
    }
}
