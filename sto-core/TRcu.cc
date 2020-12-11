#include "TRcu.hh"

#include <limits>

TRcuSet::TRcuSet()
    : clean_epoch_(0) {
    unsigned capacity = (4080 - sizeof(TRcuGroup)) / sizeof(TRcuGroup::TRcuElement);
    current_ = first_ = TRcuGroup::make(capacity);
    // ngroups_ = 1;
}

TRcuSet::~TRcuSet() {
    while (first_) {
        TRcuGroup* next = first_->next_;
        TRcuGroup::free(first_);
        first_ = next;
    }
    current_ = nullptr;
    // ngroups_ = 0;
}

void TRcuSet::check() {
    // check invariants
    TRcuGroup* first = first_;
    unsigned n = (unsigned) -1; // ngroups_;
    bool found_current = false;
    while (n > 0 && first) {
        assert(first->head_ <= first->tail_);
        assert(first->tail_ <= first->capacity_);
        if (found_current)
            assert(first->head_ == 0 && first->tail_ == 0);
        else if (first != current_)
            assert(first->head_ != first->tail_);
        found_current = found_current || first == current_;
        --n;
        first = first->next_;
    }
    assert(!first);
    assert(found_current);
    // assert(n == 0);
    // assert(ngroups_ > 0);
}

void TRcuSet::grow() {
    if (!current_->next_) {
        unsigned capacity = (16368 - sizeof(TRcuGroup)) / sizeof(TRcuGroup::TRcuElement);
        current_->next_ = TRcuGroup::make(capacity);
        // ++ngroups_;
    }
    current_ = current_->next_;
    assert(current_->head_ == 0 && current_->tail_ == 0);
}

inline bool TRcuGroup::clean_until(epoch_type max_epoch) {
    while (head_ != tail_
           && signed_epoch_type(max_epoch - e_[head_].u.epoch) > 0) {
        ++head_;
        while (head_ != tail_ && e_[head_].function) {
            e_[head_].function(e_[head_].u.argument);
            ++head_;
        }
    }
    if (head_ == tail_) {
        head_ = tail_ = 0;
        return true;
    } else {
        return false;
    }
}

void TRcuSet::hard_clean_until(epoch_type max_epoch) {
    TRcuGroup* empty_head = nullptr;
    TRcuGroup* empty_tail = nullptr;
    // clean [first_, current_]
    while (first_->clean_until(max_epoch)) {
        if (!empty_head) {
            empty_head = first_;
        }
        empty_tail = first_;
        if (first_ == current_) {
            first_ = current_ = empty_head;
            return;
        }
        first_ = first_->next_;
    }
    // hook empties after current_; everything after current_ guaranteed empty
    if (empty_head) {
        empty_tail->next_ = current_->next_;
        current_->next_ = empty_head;
    }
}
