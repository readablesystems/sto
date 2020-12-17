#pragma once

#include <new>
#include "compiler.hh"
#include <assert.h>

struct TRcuGroup {
    typedef uint64_t epoch_type;
    typedef int64_t signed_epoch_type;
    typedef void (*callback_type)(void*);

    struct TRcuElement {
        callback_type function;
        union {
            void* argument;
            epoch_type epoch;
        } u;
    };

    unsigned head_;
    unsigned tail_;
    unsigned capacity_;
    epoch_type epoch_;
    TRcuGroup* next_;
    TRcuElement e_[1];

private:
    TRcuGroup(unsigned capacity)
        : head_(0), tail_(0), capacity_(capacity), next_(nullptr) {
    }
    TRcuGroup(const TRcuGroup&) = delete;
    ~TRcuGroup() {
        assert(head_ == tail_);
    }

public:
    static TRcuGroup* make(unsigned capacity) {
        void* x = new char[sizeof(TRcuGroup) + sizeof(TRcuElement) * (capacity - 1)];
        return new(x) TRcuGroup(capacity);
    }
    static void free(TRcuGroup* g) {
        g->~TRcuGroup();
        delete[] reinterpret_cast<char*>(g);
    }

    bool empty() const {
        return head_ == tail_;
    }

    void add(epoch_type epoch, callback_type function, void* argument) {
        assert(tail_ + 2 <= capacity_);
        if (head_ == tail_ || epoch_ != epoch) {
            e_[tail_].function = nullptr;
            e_[tail_].u.epoch = epoch;
            epoch_ = epoch;
            ++tail_;
        }
        e_[tail_].function = function;
        e_[tail_].u.argument = argument;
        ++tail_;
    }

    inline bool clean_until(epoch_type max_epoch);
};

class TRcuSet {
public:
    typedef TRcuGroup::epoch_type epoch_type;
    typedef TRcuGroup::signed_epoch_type signed_epoch_type;
    typedef TRcuGroup::callback_type callback_type;

    TRcuSet();
    ~TRcuSet();

    bool empty() const {
        return first_->empty();
    }

    void add(epoch_type epoch, callback_type function, void* argument) {
        if (unlikely(current_->tail_ + 2 > current_->capacity_)) {
            grow();
        }
        current_->add(epoch, function, argument);
    }

    void clean_until(epoch_type max_epoch) {
        if (clean_epoch_ != max_epoch) {
            hard_clean_until(max_epoch);
        }
        clean_epoch_ = max_epoch;
    }
    epoch_type clean_epoch() const {
        return clean_epoch_;
    }

private:
    TRcuGroup* current_;
    TRcuGroup* first_;
    epoch_type clean_epoch_;
    // unsigned ngroups_;

    TRcuSet(const TRcuSet&) = delete;
    TRcuSet& operator=(const TRcuSet&) = delete;
    void check();
    void grow();
    void hard_clean_until(epoch_type max_epoch);
};
