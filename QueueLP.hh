#pragma once

#include <list>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "TWrapped.hh"
#include "LazyPop.hh"

template <typename T, bool Opacity, unsigned BUF_SIZE> class QueueLP;

template <typename T, bool Opacity = true, unsigned BUF_SIZE = 1000000>
class QueueLP: public Shared {

public:
    typedef typename std::conditional<Opacity, TVersion, TNonopaqueVersion>::type version_type;
    typedef T value_type;
    typedef LazyPop<T, QueueLP<T, false, BUF_SIZE>> pop_type;

    QueueLP() : head_(0), tail_(0), headversion_(0), tailversion_(0) {
    }

    static constexpr TransItem::flags_type list_bit = TransItem::user0_bit<<2;
    static constexpr TransItem::flags_type pop_bit = TransItem::user0_bit<<3;
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
                write_list.push_back(val);
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

    pop_type& pop() {
        pop_type* my_lazypop = &global_thread_layzpops[TThread::id()];
        auto item = Sto::item(this, my_lazypop);
        item.add_flags(pop_bit);
        item.add_write();
        Sto::item(this, popitem_key).add_write();
        return *my_lazypop;
    }

    bool front(T& val) {
        // TODO
        //return LazyFront(&this);
        val = T();
        return true;
    }

private:
    bool is_pop(const TransItem& item) {
        return item.flags() & pop_bit;
    }
 
    bool is_list(const TransItem& item) {
        return item.flags() & list_bit;
    }
 
    bool lock(TransItem& item, Transaction& txn) override {
        if (item.key<int>() == popitem_key)
            return txn.try_lock(item, headversion_);
        else if (item.key<int>() == pushitem_key)
            return txn.try_lock(item, tailversion_);
        return true;
    }

    bool check(TransItem& item, Transaction& t) override {
        (void) t;
        (void) item;
        return true;
    }

    void install(TransItem& item, Transaction&) override {
        // install pops
        if (is_pop(item)) {
            auto curtail = tail_;
            auto lazypop = item.key<pop_type*>();
            lazypop->set_fulfilled();
            if (head_  == curtail) {
                lazypop->set_popped(false);
            } else {
                lazypop->set_popped(true);
                head_ = (head_+1) % BUF_SIZE;
            }
        }
        // install pushes
        else if (item.key<int>() == pushitem_key) {
            auto curhead = head_;
            // write all the elements
            if (is_list(item)) {
                auto& write_list = item.template write_value<std::list<T>>();
                while (!write_list.empty()) {
                    // assert queue is not out of space
                    assert(tail_ != (curhead-1) % BUF_SIZE);
                    queueSlots[tail_] = write_list.front();
                    write_list.pop_front();
                    tail_ = (tail_+1) % BUF_SIZE;
                }
            } else {
                auto& val = item.template write_value<T>();
                queueSlots[tail_] = val;
                tail_ = (tail_+1) % BUF_SIZE;
            }
        }
    }
    
    void unlock(TransItem& item) override {
        if (item.key<int>() == popitem_key)
            headversion_.unlock();
        else if (item.key<int>() == pushitem_key)
            tailversion_.unlock();
    }


    void cleanup(TransItem& item, bool committed) override {
        if (!committed && item.key<int>() == popitem_key)
            (global_thread_layzpops + TThread::id())->~pop_type();
        /*
        if (item.key<int>() == popitem_key) {
            auto lazy_pops = item.template write_value<std::list<pop_type*>>();
            while (!lazy_pops.empty()) {
                auto lazypop = lazy_pops.front();
                lazy_pops.pop_front();
                Transaction::rcu_free(lazypop);
            }
        }*/
    }

    T queueSlots[BUF_SIZE];
    
    unsigned head_;
    unsigned tail_;
    version_type headversion_;
    version_type tailversion_;
    pop_type global_thread_layzpops[24];
};
