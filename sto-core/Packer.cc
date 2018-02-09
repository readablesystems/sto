#include "Packer.hh"

constexpr size_t TransactionBuffer::default_capacity;

void TransactionBuffer::hard_get_space(size_t needed) {
    size_t s = std::max(needed, e_ ? e_->capacity * 2 : default_capacity);
    elt* ne = (elt*) new char[sizeof(elthdr) + s];
    ne->next = e_;
    ne->pos = 0;
    ne->capacity = s;
    if (e_)
        linked_size_ += e_->pos;
    e_ = ne;
}

void TransactionBuffer::hard_clear(bool delete_all) {
    while (e_ && e_->next) {
        elt* e = e_->next;
        e_->next = e->next;
        e->clear();
        delete[] (char*) e;
    }
    if (e_)
        e_->clear();
    linked_size_ = 0;
    if (e_ && delete_all) {
        delete[] (char*) e_;
        e_ = 0;
    }
}
