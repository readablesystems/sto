#pragma once

#include "Transaction.hh"
#include "Interface.hh"
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

template <typename N>
class NodeBase;

template <typename N>
class NodeWrapper;

template <typename N>
class ObjectID {
public:
    static constexpr uintptr_t direct_bit = 1;
    static constexpr uintptr_t mask = ~(direct_bit);

    ObjectID(NodeBase<N>* baseptr, bool direct = false) {
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

    void operator=(N* ptr) {
        oid_ = reinterpret_cast<uintptr_t>(ptr);
    }

    bool is_null() const {
        return ((oid_ & mask) == 0);
    }

    NodeBase<N>* base_ptr() const {
        assert(!direct());
        return reinterpret_cast<NodeBase<N>*>(oid_);
    }

    // the only indirection needed: translating ObjectID to a reference to "node"
    // given the specific time in sid
    std::pair<N*, N*> deref(sid_type sid) {
        std::pair<N*, N*> ret(nullptr, nullptr);
        if (direct()) {
            assert(sid);
            ret.first = reinterpret_cast<N*>(oid_ & mask);
        } else {
            NodeBase<N>* baseptr = reinterpret_cast<NodeBase<N>*>(oid_);
            NodeWrapper<N>* root = baseptr->root_wrapper();
            sid_type root_sid = root->s_sid;

            ret.second = reinterpret_cast<N*>(root);

            if (!sid || (root_sid > 0 && root_sid <= sid)) {
               if (!sid && baseptr->is_unlinked()) {
                   // do not return an unlinked node if we are doing non-snapshot reads
                   return ret;
               }
               // thanks to rcu at the top level this is safe (should be)
               ret.first = &root->node();
               return ret;
            }

            if (root_sid > 0) {
                ret.first = baseptr->search_history(sid);
            }
        }
        return ret;
    }

private:
    uintptr_t oid_;
};

template <typename N>
class NodeWrapper : public N {
public:
    typedef ObjectID<N> oid_type;
    oid_type oid;
    sid_type s_sid;
    sid_type c_sid;

    explicit NodeWrapper(oid_type id) : N(), oid(id), s_sid(), c_sid() {}
    N& node(){return *this;}
};

template <typename N>
class History {
public:
    void cleanup_until(sid_type sid);
    void add_snapshot(NodeWrapper<N>* n, sid_type time);
    N* search(sid_type sid);
    bool is_empty() const {
        lock_read(lock_);
        bool ret = list_.empty();
        unlock_read(lock_);
        return ret;
    }
private:
    mutable RWLock lock_;
    std::deque<NodeWrapper<N>*> list_;

    static bool sid_comp(const NodeWrapper<N>* n, sid_type sid) {return n->s_sid < sid;};
};

template <typename N>
class NodeBase {
public:
    typedef ObjectID<N> oid_type;
    explicit NodeBase() : unlinked(false), h_(), nw_(new NodeWrapper<N>(object_id())) {}

    oid_type object_id() const {return reinterpret_cast<oid_type>(this);}
    NodeWrapper<N>* root_wrapper() {return nw_;}

    N& node() {return nw_->node();}

    // Copy-on-Write: only allowed at STO install time!!
    bool is_immutable() {return nw_->s_sid != 0 && nw_->s_sid < Sto::GSC_snapshot();};

    void save_copy_in_history() {
        assert(is_immutable());
        sid_type time = Sto::GSC_snapshot();
        NodeWrapper<N>* rcu = new NodeWrapper<N>(*nw_);
        NodeWrapper<N>* old = nw_;
        rcu->s_sid = time;
        nw_ = rcu;
        h_.add_snapshot(old, time);
    }

    void set_unlinked() {unlinked = true;}
    void clear_unlinked() {unlinked = false;}
    bool is_unlinked() const {return unlinked;}

    N* search_history(sid_type sid) {h_.search(sid);}
    bool history_is_empty() const {return h_.is_empty();}
private:
    bool unlinked;
    History<N> h_;
    NodeWrapper<N> *nw_;
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

template <typename N>
std::pair<ObjectID<N>, N*> new_object() {
    NodeBase<N>* obj = new NodeBase<N>();
    return std::make_pair(ObjectID<N>(obj), &(obj->node()));
}

// STOSnapshot::History methods
// add_snapshot: add pointer @n (pointing to a snapshot object)
// to history list
template <typename N>
void History<N>::add_snapshot(NodeWrapper<N>* n, sid_type time) {
    assert(n->c_sid == 0);
    n->c_sid = time;

    // XXX garbage collection / pool-chain fancy stuff happens here
    
    lock_write(lock_);
    // History::list_ should always be sorted
    assert(list_.back()->s_sid < n->s_sid);
    list_.push_back(n);
    unlock_write(lock_);
}

template <typename N>
void History<N>::cleanup_until(sid_type sid) {
    lock_write(lock_);
    while (list_.begin() != list_.end()) {
        NodeWrapper<N>* e = list_.front();
        if(e->s_sid <= sid) {
            // XXX rcu_deletes called outside of this right
            // in the gc loop
            //Transaction::rcu_delete(e);
            list_.pop_front();
        } else {
            break;
        }
    }
    unlock_write(lock_);
}

template <typename N>
N* History<N>::search(sid_type sid) {
    NodeWrapper<N>* ret = nullptr;
    lock_read(lock_);
    if (!list_.empty()) {
        auto it = std::upper_bound(list_.begin(), list_.end(), sid, sid_comp);
        if (it != list_.begin())
            ret = *(--it);
        if (ret && ret->c_sid < sid) {
            // this implies a deleted snapshot
            ret = nullptr;
        }
    }
    unlock_read(lock_);
    return reinterpret_cast<N*>(ret);
}

}; // namespace StoSnapshot
