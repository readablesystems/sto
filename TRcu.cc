#include "TRcu.hh"

TRcuSet::TRcuSet()
    : clean_epoch_(0) {
    unsigned capacity = (4080 - sizeof(TRcuGroup)) / sizeof(TRcuGroup::TRcuElement);
    current_ = first_ = TRcuGroup::make(capacity);
}

TRcuSet::~TRcuSet() {
    while (first_) {
        TRcuGroup* next = first_->next_;
        TRcuGroup::free(first_);
        first_ = next;
    }
}

void TRcuSet::grow() {
    if (!current_->next_) {
        unsigned capacity = (16368 - sizeof(TRcuGroup)) / sizeof(TRcuGroup::TRcuElement);
        current_->next_ = TRcuGroup::make(capacity);
    }
    current_ = current_->next_;
    assert(current_->head_ == 0 && current_->tail_ == 0);
}

void TRcuSet::hard_clean_until(epoch_type max_epoch) {
    TRcuGroup* empty_head = nullptr;
    TRcuGroup* empty_tail = nullptr;
    // clean [first_, current_]
    while (first_->clean_until(max_epoch)) {
        if (!empty_head)
            empty_head = first_;
        empty_tail = first_;
        if (first_ == current_) {
            current_ = empty_head;
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
