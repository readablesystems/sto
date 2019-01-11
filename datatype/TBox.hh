#pragma once

#include "Sto.hh"

template <typename T, typename W = TOpaqueWrapped<T> >
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

    std::pair<bool, read_type> read_nothrow() const {
        auto item = Sto::item(this, 0);
        if (item.has_write())
            return {true, item.template write_value<T>()};
        else
            return v_.read(item, vers_);
    }

    read_type read() const {
        auto result = read_nothrow();
        if (!result.first)
            throw Transaction::Abort();
        else
            return result.second;
    }

    void write(const T& x) {
        Sto::item(this, 0).acquire_write(vers_, x);
    }
    void write(T&& x) {
        Sto::item(this, 0).acquire_write(vers_, std::move(x));
    }
    template <typename... Args>
    void write(Args&&... args) {
        Sto::item(this, 0).template acquire_write<T>(vers_, std::forward<Args>(args)...);
    }

    operator read_type() const {
        return read();
    }
    TBox<T, W>& operator=(const T& x) {
        write(x);
        return *this;
    }
    TBox<T, W>& operator=(T&& x) {
        write(std::move(x));
        return *this;
    }
    //template <typename V>
    //TBox<T, W>& operator=(V&& x) {
    //    write(std::forward<V>(x));
    //    return *this;
    //}
    TBox<T, W>& operator=(const TBox<T, W>& x) {
        write(x.read());
        return *this;
    }

    const T& nontrans_read() const {
        return v_.access();
    }
    T& nontrans_access() {
        return v_.access();
    }
    void nontrans_write(const T& x) {
        v_.access() = x;
    }
    void nontrans_write(T&& x) {
        v_.access() = std::move(x);
    }

    // transactional methods
    bool lock(TransItem& item, Transaction& txn) override {
        return txn.try_lock(item, vers_);
    }
    bool check(TransItem& item, Transaction& txn) override {
        return vers_.cp_check_version(txn, item);
    }
    void install(TransItem& item, Transaction& txn) override {
        v_.write(std::move(item.template write_value<T>()));
        txn.set_version_unlock(vers_, item);
    }
    void unlock(TransItem& item) override {
        vers_.cp_unlock(item);
    }
    void print(std::ostream& w, const TransItem& item) const override {
        w << "{TBox<" << typeid(T).name() << "> " << (void*) this;
        if (item.has_read())
            w << " R" << item.read_value<version_type>();
        if (item.has_write())
            w << " =" << item.write_value<T>();
        w << "}";
    }

protected:
    version_type vers_;
    W v_;
};
