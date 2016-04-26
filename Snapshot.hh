#pragma once

#include "Transaction.hh"
#include "Interface.hh"
#include "TWrapped.hh"
#include <deque>

namespace StoSnapshot {

using sid_type = TransactionTid::type;
using RWLock = TransactionTid::type;

void lock_read(RWLock& l) {
    TransactionTid::lock_read(l);
}
void lock_write(RWLock& l) {
    TransactionTid::lock_write(l);
}
void unlock_read(RWLock& l) {
    TransactionTid::unlock_read(l);
}
void unlock_write(RWLock& l) {
    TransactionTid::unlock_write(l);
}

// template arguments
// N: node type, L: structural link type

template <typename N, typename L>
class NodeBase;

template <typename N, typename L>
class NodeWrapper;

template <typename N, typename L>
class ObjectID {
public:
    static constexpr uintptr_t direct_bit = 1;
    static constexpr uintptr_t mask = ~(direct_bit);

    ObjectID(uintptr_t val) : oid_(val) {}

    ObjectID(NodeBase<N, L>* baseptr, bool direct = false) {
        oid_ = reinterpret_cast<uintptr_t>(baseptr);
        if (direct) {
            oid_ |= direct_bit;
        }
    }

    bool direct() const {
        return (oid_ & direct_bit);
    }

    // can only be called on an immutable object
    void set_direct_link(N* ptr) {
        oid_ = reinterpret_cast<uintptr_t>(ptr) | direct_bit;
    }
    void operator=(const ObjectID& rhs) {
        oid_ = rhs.oid_;
    }
    void operator=(N* ptr) {
        oid_ = reinterpret_cast<uintptr_t>(ptr);
    }

    bool is_null() const {
        return ((oid_ & mask) == 0);
    }

    uintptr_t value() const {
        return oid_;
    }

    NodeWrapper<N, L>* wrapper_ptr() const {
        assert(direct());
        return reinterpret_cast<NodeWrapper<N, L>*>(oid_ & mask);
    }

    NodeBase<N, L>* base_ptr() const {
        assert(!direct());
        return reinterpret_cast<NodeBase<N, L>*>(oid_);
    }

    // the only indirection needed: translating ObjectID to a reference to "node"
    // given the specific time in sid
    NodeWrapper<N, L>* deref(sid_type sid) {
        assert(!direct());
        NodeWrapper<N, L>* ret = nullptr;
        NodeBase<N, L>* baseptr = base_ptr();
        NodeWrapper<N, L>* root = baseptr->root_wrapper();
        sid_type root_sid = root->sid;

        if (!sid || (root_sid != Sto::invalid_snapshot && root_sid <= sid)) {
           if (!sid && baseptr->is_unlinked()) {
               // do not return an unlinked node if we are doing non-snapshot reads
               return ret;
           }
           // thanks to the pointer trick at the top level this should be safe
           return root;
        }

        if (root_sid != Sto::invalid_snapshot) {
            ret = baseptr->search_history(sid);
        }
        return ret;
    }

private:
    uintptr_t oid_;
};

template <typename N, typename L>
class NodeWrapper : public N {
public:
    typedef ObjectID<N, L> oid_type;
    L links;
    bool deleted;
    oid_type oid;
    sid_type sid; // bascially commit_tid

    explicit NodeWrapper(oid_type id) : N(), links(), deleted(false), oid(id), sid() {}
    N& node(){return *this;}
};

template <typename N, typename L>
class History {
public:
    typedef std::pair<sid_type, NodeWrapper<N, L>*> history_entry_type;

    void cleanup_until(sid_type sid);
    void add_snapshot(NodeWrapper<N, L>* n, sid_type time);
    NodeWrapper<N, L>* search(sid_type sid);
    bool is_empty() const {
        lock_read(lock_);
        bool ret = list_.empty();
        unlock_read(lock_);
        return ret;
    }
private:
    mutable RWLock lock_;
    std::deque<history_entry_type> list_;

    static bool sid_comp(sid_type sid, const history_entry_type& ent) {return sid < ent.first;};
};

template <typename N, typename L>
class NodeBase {
public:
    typedef ObjectID<N, L> oid_type;
    static constexpr TransactionTid::type invalid_bit = TransactionTid::user_bit;
    explicit NodeBase() : unlinked(false), vers_(),
        h_(), nw_(new NodeWrapper<N, L>(oid_type(this))) {}

    oid_type object_id() const {return oid_type(this);}
    NodeWrapper<N, L>* root_wrapper() {return nw_.access();}
    NodeWrapper<N, L>* atomic_root_wrapper(TransProxy& item, TVersion& vers) {return nw_.read(item, vers);}

    N& node() {return nw_.access()->node();}

    // Copy-on-Write: only allowed at STO install time!!
    bool is_immutable() {return nw_.access()->sid != Sto::invalid_snapshot && nw_.access()->sid < Sto::GSC_snapshot();};

    void save_copy_in_history() {
        assert(is_immutable());
        sid_type time = Sto::GSC_snapshot();
        NodeWrapper<N, L>* rcu = new NodeWrapper<N, L>(*nw_.access());
        NodeWrapper<N, L>* old = nw_.access();
        rcu->sid = time;
        nw_.write(rcu);
        h_.add_snapshot(old, time);
    }

    void set_unlinked() {unlinked = true;}
    void clear_unlinked() {unlinked = false;}
    bool is_unlinked() const {return unlinked;}
    TVersion& version() {return vers_;}

    NodeWrapper<N, L>* search_history(sid_type sid) {return h_.search(sid);}
    bool history_is_empty() const {return h_.is_empty();}
private:
    bool unlinked;
    TVersion vers_;
    History<N, L> h_;
    TWrapped<NodeWrapper<N, L>*> nw_;
public:
    L links;
};

// precondition: when properly casted, @oid must be pointing to a
// NodeBase-typed object in memory
//
// returns nullptr if no applicable history item found
// XXX seems equivalent and worse (interface-wise) than ObjectID::deref()
//template <typename N>
//N* get_object(oid_type oid, sid_type sid) {
//    auto obj = reinterpret_cast<NodeBase<N>*>(oid);
//    return obj->search_history(sid);
//}

template <typename N, typename L>
std::pair<ObjectID<N, L>, N*> new_object() {
    NodeBase<N, L>* obj = new NodeBase<N, L>();
    return std::make_pair(ObjectID<N, L>(obj), &(obj->node()));
}

// STOSnapshot::History methods
// add_snapshot: add pointer @n (pointing to a snapshot object)
// to history list
// @sid is the sid version of the root-level object, @time is the GSC (commit_tid) snapshot
template <typename N, typename L>
void History<N, L>::add_snapshot(NodeWrapper<N, L>* n, sid_type time) {
    //assert(n->c_sid == 0);
    //n->c_sid = time;
    (void)time;

    // XXX garbage collection / pool-chain fancy stuff happens here
    
    lock_write(lock_);
    // History::list_ should always be sorted
    assert(list_.back().first < n->sid);
    list_.push_back(std::make_pair(n->sid, n));
    unlock_write(lock_);
}

template <typename N, typename L>
void History<N, L>::cleanup_until(sid_type sid) {
    lock_write(lock_);
    while (list_.begin() != list_.end()) {
        auto e = list_.front();
        if(e.first <= sid) {
            // XXX rcu_deletes called outside of this right
            // in the gc loop
            //Transaction::rcu_delete(e.second);
            list_.pop_front();
        } else {
            break;
        }
    }
    unlock_write(lock_);
}

template <typename N, typename L>
NodeWrapper<N, L>* History<N, L>::search(sid_type sid) {
    NodeWrapper<N, L>* ret = nullptr;
    lock_read(lock_);
    if (!list_.empty()) {
        auto it = std::upper_bound(list_.begin(), list_.end(), sid, sid_comp);
        if (it != list_.begin())
            ret = (--it)->second;
        // looks like the following check is unecessary and actually wrong...
        //if (ret && ret->c_sid < sid) {
            // this implies a deleted snapshot
            //ret = nullptr;
        //}
    }
    unlock_read(lock_);
    return ret;
}

}; // namespace StoSnapshot
