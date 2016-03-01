#include "Packer.hh"

void TransactionBuffer::hard_get_space(size_t needed) {
    size_t s = std::max(needed, e_ ? e_->size * 2 : 256);
    elt* ne = (elt*) new char[sizeof(elthdr) + s];
    ne->next = e_;
    ne->pos = 0;
    ne->size = s;
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
    if (e_ && delete_all) {
        delete[] (char*) e_;
        e_ = 0;
    }
}
