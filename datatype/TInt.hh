#pragma once

#include "Sto.hh"

// template <typename W = TOpaqueWrapped<int> >
class TInt : public TObject {
public:
    typedef typename TOpaqueWrapped<int>::read_type read_type;
    typedef typename TOpaqueWrapped<int>::version_type version_type;
    static constexpr TransItem::flags_type assigned_bit = TransItem::user0_bit;

    TInt() {
    }
    template <typename... Args>
    explicit TInt(Args&&... args)
        : v_(std::forward<Args>(args)...) {
    }

    std::pair<bool, read_type> read_nothrow() const {
        auto item = Sto::item(this, 0);

        if (!item.has_flag(assigned_bit)) {
            auto x = v_.read(item, vers_);
            if (item.has_write()) {
                auto delta = item.template write_value<int>();
                x = {x.first, x.second + delta};
                // write(x.second);
            }
            return x;
        } else {
            if (item.has_write())
                return {true, item.template write_value<int>()};
            else
                return v_.read(item, vers_);
        }
    }

    read_type read() const {
        auto result = read_nothrow();
        if (!result.first)
            throw Transaction::Abort();
        else
            return result.second;
    }

    void write(const int& x) {
        auto item = Sto::item(this, 0);
        item.acquire_write(vers_, x);
        item.add_flags(assigned_bit);
    }
    void write(int&& x) {
        auto item = Sto::item(this, 0);
        item.acquire_write(vers_, std::move(x));
        item.add_flags(assigned_bit);
    }
    template <typename... Args>
    void write(Args&&... args) {
        auto item = Sto::item(this, 0);
        item.template acquire_write<int>(vers_, std::forward<Args>(args)...);
        item.add_flags(assigned_bit);
    }

    operator read_type() const {
        return read();
    }
    TInt& operator=(const int& x) {
        write(x);
        return *this;
    }
    TInt& operator=(int&& x) {
        write(std::move(x));
        return *this;
    }
    //template <typename V>
    //TInt<TOpaqueWrapped<int>>& operator=(V&& x) {
    //    write(std::forward<V>(x));
    //    return *this;
    //}
    TInt& operator=(const TInt& x) {
        write(x.read());
        return *this;
    }

    void inc(int n) {
        // write(this->read() + x);
        auto item = Sto::item(this, 0);
        if (item.has_read()) {
            write(this->read() + n);
        } else {
            // write(item.template write_value<int>() + n);
            if (item.has_write()) {
                item.acquire_write(vers_, item.template write_value<int>() + n);
            } else {
                item.acquire_write(vers_, n);
            }
        }
    }

    const int& nontrans_read() const {
        return v_.access();
    }
    int& nontrans_access() {
        return v_.access();
    }
    void nontrans_write(const int& x) {
        v_.access() = x;
    }
    void nontrans_write(int&& x) {
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
        v_.write(std::move(item.template write_value<int>()));
        txn.set_version_unlock(vers_, item);
    }
    void unlock(TransItem& item) override {
        vers_.cp_unlock(item);
    }
    void print(std::ostream& w, const TransItem& item) const override {
        w << "{TInt<" << typeid(int).name() << "> " << (void*) this;
        if (item.has_read())
            w << " R" << item.read_value<version_type>();
        if (item.has_write())
            w << " =" << item.write_value<int>();
        w << "}";
    }

protected:
    version_type vers_;
    TOpaqueWrapped<int> v_;
};
