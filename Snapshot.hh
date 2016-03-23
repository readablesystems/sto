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

class ObjectID {
public:
    static constexpr uintptr_t direct_bit = 1;
    static constexpr uintptr_t mask = ~(direct_bit);

    template <typename N>
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
    template <typename N>
    void set_direct_link(N* ptr) {
        oid_ = reinterpret_cast<uintptr_t>(ptr) | direct_bit;
    }

    // the only indirection needed: translating ObjectID to a reference to "node"
    // given the specific time in sid
    template <typename N>
    N& dereference(sid_type sid) const {
        if (direct()) {
            return *reinterpret_cast<N*>(oid_ & mask);
        } else {
            NodeBase<N>* baseptr = reinterpret_cast<NodeBase<N>*>(oid_);
            NodeWrapper<N>* root = baseptr->root_wrapper();
            if (root->s_sid <= sid) {
               // thanks to rcu at the top level this is safe (should be)
               return root->node();
            }
            N* rawptr = baseptr->search_history(sid);
            assert(rawptr);
            return *rawptr;
        }
    }

    template <typename N>
    void unlink() {
        assert(!direct());
        reinterpret_cast<NodeBase<N>*>(oid_)->set_unlinked();
    }

private:
    uintptr_t oid_;
};

typedef ObjectID oid_type;

template <typename N>
class NodeWrapper {
public:
    oid_type oid;
    sid_type s_sid;
    sid_type c_sid;

    explicit NodeWrapper(oid_type id) : oid(id), s_sid(), c_sid(), n_() {}
    N& node(){return n_;}
private:
    N n_;
};

template <typename N>
class History {
public:
    void cleanup_until(sid_type sid);
    void add_snapshot(NodeWrapper<N>& n, sid_type time);
    N* search(sid_type sid);
private:
    RWLock lock_;
    std::deque<NodeWrapper<N>*> list_;

    static bool sid_comp(const NodeWrapper<N>* n, sid_type sid) {return n->s_sid < sid;};
};

template <typename N>
class NodeBase {
public:
    explicit NodeBase() : unlinked(false), h_(), nw_(new NodeWrapper<N>(object_id())) {}

    oid_type object_id() const {return reinterpret_cast<oid_type>(this);}
    NodeWrapper<N>* root_wrapper() {return nw_;}

    N& node() {return nw_->node();}

    // Copy-on-Write: only allowed at STO install time!!
    bool is_immutable() {return nw_->s_sid < Sto::GSC_snapshot();};

    void save_copy_in_history() {
        assert(is_immutable());
	sid_type time = Sto::GSC_snapshot();
	NodeWrapper<N>* rcu = new NodeWrapper<N>(*nw_);
	rcu->s_sid = time;
        h_.add_snapshot(nw_, time);
	nw_ = rcu;
    }

    void set_unlinked() {unlinked = true;}

    N* search_history(sid_type sid) {h_.search(sid);}
private:
    bool unlinked;
    History<N> h_;
    NodeWrapper<N> *nw_;
};

// precondition: when properly casted, @oid must be pointing to a
// NodeBase-typed object in memory
//
// returns nullptr if no applicable history item found
// XXX seems equivalent and worse (interface-wise) than ObjectID::dereference()
template <typename N>
N* get_object(oid_type oid, sid_type sid) {
    auto obj = reinterpret_cast<NodeBase<N>*>(oid);
    return obj->search_history(sid);
}

template <typename N>
std::pair<oid_type, N*> new_object() {
    NodeBase<N>* obj = new NodeBase<N>();
    return std::make_pair(oid_type(obj), &(obj->node()));
}

// STOSnapshot::History methods
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
    N* ret = nullptr;
    lock_read(lock_);
    if (!list_.empty()) {
        auto it = std::upper_bound(list_.begin(), list_.end(), sid, sid_comp);
        if (it != list_.begin())
            ret = *(--it);
    }
    unlock_read(lock_);
    return ret;
}

}; // namespace StoSnapshot
