#pragma once
#include "Interface.hh"
#include "TWrapped.hh"

template <typename T, typename W = TWrapped<T> >
class TBox : public TObject {
public:
    typedef typename W::read_type read_type;
    typedef typename W::version_type version_type;

    TBox() {
    }
    template <typename... Args>
    explicit TBox(Args&&... args)
        : v_(std::forward<Args>(args)...) {
    }

    operator read_type() const {
        auto item = Sto::item(this, 0);
        if (item.has_write())
            return item.template write_value<T>();
        else
            return v_.read(item, vers_);
    }

    TBox<T, W>& operator=(const T& x) {
        Sto::item(this, 0).add_write(x);
        return *this;
    }
    TBox<T, W>& operator=(T&& x) {
        Sto::item(this, 0).add_write(std::move(x));
        return *this;
    }
    TBox<T, W>& operator=(const TBox<T, W>& x) {
        Sto::item(this, 0).add_write(x.operator read_type());
        return *this;
    }

    const T& nontrans_read() const {
        return v_.access();
    }
    void nontrans_write(const T& x) {
        v_.access() = x;
    }
    void nontrans_write(T&& x) {
        v_.access() = std::move(x);
    }

    // transactional methods
    bool lock(TransItem&, Transaction& txn) {
        return txn.try_lock(vers_);
    }
    bool check(const TransItem& item, const Transaction&) {
        return item.check_version(vers_);
    }
    void install(TransItem& item, const Transaction& txn) {
        v_.write(std::move(item.template write_value<T>()));
        vers_.set_version_unlock(txn.commit_tid());
        item.clear_needs_unlock();
    }
    void unlock(TransItem&) {
        vers_.unlock();
    }
    void print(std::ostream& w, const TransItem& item) const {
        w << "{TBox<" << typeid(T).name() << "> " << (void*) this;
        if (item.has_read())
            w << " R" << item.read_value<version_type>();
        if (item.has_write())
            w << " =" << item.write_value<T>();
        w << "}";
    }

private:
    version_type vers_;
    W v_;
};
