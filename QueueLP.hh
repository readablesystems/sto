#pragma once

#include <list>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "TWrapped.hh"

template <typename T, bool Opacity, unsigned BUF_SIZE> class QueueLP;

template <typename T, bool Opacity = true, unsigned BUF_SIZE = 1000000>
class LazyPop {
public:
    typedef typename std::conditional<Opacity, TVersion, TNonopaqueVersion>::type version_type;
    typedef QueueLP<T, Opacity, BUF_SIZE> queue_t;
    typedef T value_type;
    static constexpr int popitem_key = -2;
    
    LazyPop(queue_t* q) : q_(q), popped_(false), fulfilled_(false), reassigned_(false) {
        auto item = Sto::item(q, popitem_key);
        // XXX for now just keep the item as a list
        if (!item.has_write()) {
            std::list<LazyPop*> write_list;
            write_list.push_back(this);
            item.add_write(write_list);
        } else {
            auto& write_list = item.template write_value<std::list<LazyPop*>>();
            write_list.push_back(this);
        }
    }
    // someone is reassigning a lazypop (used on LHS)
    LazyPop* operator=(bool b) {
        fulfilled_ = true;
        popped_ = b;
        reassigned_ = true;
        return *this;
    };
    bool is_fulfilled() const {
        return fulfilled_;
    }
    bool is_popped() const {
        return popped_;
    }
    bool is_reassigned() const {
        return reassigned_;
    }
    void set_fulfilled() {
        fulfilled_ = true;
    }
    void set_popped(bool b) {
        assert(fulfilled_);
        popped_ = b;
    }
    // someone is accessing a lazypop during a transaction (used on RHS)
    operator bool() {
        // we already instantiated this lazy pop
        if (fulfilled_) {
            // XXX should we check queueversion here to make sure it's still valid, or else abort
            return popped_;
        }

        // we haven't collapsed this promise yet
        version_type qv = q_->queueversion_;
        auto popitem = Sto::item(q_, popitem_key);
        assert(popitem.has_write());

        auto lazy_pops = popitem.template write_value<std::list<LazyPop*>>();
       
        // we should always have the fulfilled lazypops at the front of the list, since if we collapse
        // a lazypop at time t, all the lazypops in the list added before time t are also collapsed
        size_t popped_count = 0;
        auto head = q_->head_;
        auto tail = q_->tail_;
        for (auto i = lazy_pops.begin(); i != lazy_pops.end(); ++i) {
            auto lazypop = lazy_pops.front();
            if (lazypop->is_fulfilled()) {
                popped_count++;
                continue;
            }
            else {
                // XXX does this abort here if the read queueversion has changed? since we'll abort anyway?
                // this can also falsely abort if the queue is never empty, but we're pushing? (maybe we
                // should have an emptyversion or something? that only increments when the queue is empty?)
                popitem.observe(qv);
                lazypop->set_fulfilled();
                // check if we're out of space in the queue
                if ((head <= tail && ((head + popped_count) % BUF_SIZE > tail)) 
                        || (head > tail && ((head + popped_count) % BUF_SIZE <= tail))) {
                    lazypop->setpopped(false);
                } else {
                    lazypop->setpopped(true);
                    popped_count++;
                }
            }
        }
        // collapse this specific lazypop
        popped_ = !((head <= tail && ((head + popped_count) % BUF_SIZE > tail)) 
                || (head > tail && ((head + popped_count) % BUF_SIZE <= tail)));
        set_fulfilled();
    }

private:
    queue_t* q_;
    bool popped_;
    bool fulfilled_;
    bool reassigned_;
};

template <typename T, bool Opacity = true, unsigned BUF_SIZE = 1000000>
class QueueLP: public Shared {
public:
    typedef typename std::conditional<Opacity, TVersion, TNonopaqueVersion>::type version_type;
    typedef T value_type;
    typedef LazyPop<T, Opacity, BUF_SIZE> pop_type;

    QueueLP() : head_(0), tail_(0), queueversion_(0) {}

    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type read_writes = TransItem::user0_bit<<1;
    static constexpr TransItem::flags_type list_bit = TransItem::user0_bit<<2;
    static constexpr TransItem::flags_type empty_bit = TransItem::user0_bit<<3;
    static constexpr int pushitem_key = -1;
    static constexpr int popitem_key = -2;

    // NONTRANSACTIONAL PUSH/POP/EMPTY
    void nontrans_push(T v) {
        queueSlots[tail_] = v;
        tail_ = (tail_+1)%BUF_SIZE;
        assert(head_ != tail_);
    }
    
    T nontrans_pop() {
        assert(head_ != tail_);
        T v = queueSlots[head_];
        head_ = (head_+1)%BUF_SIZE;
        return v;
    }

    bool nontrans_empty() const {
        return head_ == tail_;
    }

    template <typename RandomGen>
    void nontrans_shuffle(RandomGen gen) {
        auto head = &queueSlots[head_];
        auto tail = &queueSlots[tail_];
        // don't support wrap-around shuffle
        assert(head < tail);
        std::shuffle(head, tail, gen);
    }

    void nontrans_clear() {
        while (!nontrans_empty())
            nontrans_pop();
    }

    // TRANSACTIONAL CALLS
    void push(const T& v) {
        auto item = Sto::item(this, pushitem_key);
        if (item.has_write()) {
            if (!is_list(item)) {
                auto& val = item.template write_value<T>();
                std::list<T> write_list;
                if (!is_empty(item)) {
                    write_list.push_back(val);
                    item.clear_flags(empty_bit);
                }
                write_list.push_back(v);
                item.clear_write();
                item.add_write(write_list);
                item.add_flags(list_bit);
            }
            else {
                auto& write_list = item.template write_value<std::list<T>>();
                write_list.push_back(v);
            }
        }
        else item.add_write(v);
    }

    pop_type* pop() {
        // this adds to the lazypops list
        pop_type* new_lp = new pop_type(this);
        return new_lp;
    }

    bool front(T& val) {
        // TODO
        //return LazyFront(&this);
        val = T();
        return true;
    }

private:
    bool has_delete(const TransItem& item) {
        return item.flags() & delete_bit;
    }
    
    bool is_rw(const TransItem& item) {
        return item.flags() & read_writes;
    }
 
    bool is_list(const TransItem& item) {
        return item.flags() & list_bit;
    }
 
    bool is_empty(const TransItem& item) {
        return item.flags() & empty_bit;
    }

    bool lock(TransItem& item, Transaction& txn) override {
        return txn.try_lock(item, queueversion_);
    }

    bool check(TransItem& item, Transaction& t) override {
        (void) t;
        // check if was a pop or front 
        if (item.key<int>() == popitem_key)
            return item.check_version(queueversion_);
        // shouldn't reach this
        assert(0);
        return false;
    }

    void install(TransItem& item, Transaction& txn) override {
        // install pops
        if (item.key<int>() == popitem_key) {
            auto popitem = Sto::item(this, popitem_key);
            auto lazy_pops = popitem.template write_value<std::list<pop_type*>>();
            
            bool found_empty = false;
            bool popped = false;
            for (auto i = lazy_pops.begin(); i != lazy_pops.end(); ++i) {
                auto lazypop = lazy_pops.front();
                
                // we already instantiated this lazypop, no need to assign it a value
                // either do the pop or don't if the queue is supposed to be empty
                if (lazypop->is_fulfilled()) {
                    if (lazypop->is_popped() && !lazypop->is_reassigned()) {
                        head_ = (head_+1) % BUF_SIZE;
                        popped = true;
                    } else {
                        assert(head_ == tail_);
                        found_empty = true;
                    }
                }
                
                lazypop->set_fulfilled();
                // we had found that the queue is empty. all unfulfilled pops should fail 
                if (found_empty) {
                    lazypop->set_popped(false);
                } 
                // the queue is empty at this point. set popped to false and 
                // mark that we found empty queue to short circuit the next round
                else if ((head_+1) % BUF_SIZE  == tail_) {
                    found_empty = true;
                    lazypop->set_popped(false);
                }
                // the queue is nonempty. pop and set popped=true
                else {
                    popped = true;
                    lazypop->set_popped(true);
                    head_ = (head_+1) % BUF_SIZE;
                }
                // XXX free the lazypop as soon as the thread is no longer using it.
                //Transaction::rcu_free(lazypop);
            }
            // update queueversion if we actually modified the head
            if (popped) {
                if (Opacity) {
                    queueversion_.set_version(txn.commit_tid());
                } else {
                    queueversion_.inc_nonopaque_version();
                }
            }
        }

        // install pushes
        else if (item.key<int>() == pushitem_key) {
            auto head_index = head_;
            // write all the elements
            if (is_list(item)) {
                auto& write_list = item.template write_value<std::list<T>>();
                while (!write_list.empty()) {
                    // assert queue is not out of space            
                    assert(tail_ != (head_index-1) % BUF_SIZE);
                    queueSlots[tail_] = write_list.front();
                    write_list.pop_front();
                    tail_ = (tail_+1) % BUF_SIZE;
                }
            }
            else if (!is_empty(item)) {
                auto& val = item.template write_value<T>();
                queueSlots[tail_] = val;
                tail_ = (tail_+1) % BUF_SIZE;
            }

            if (Opacity) {
                queueversion_.set_version(txn.commit_tid());
            } else {
                queueversion_.inc_nonopaque_version();
            }
        }
    }
    
    void unlock(TransItem&) override {
        queueversion_.unlock();
    }

    T queueSlots[BUF_SIZE];

    unsigned head_;
    unsigned tail_;
    version_type queueversion_;
};
