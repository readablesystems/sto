#pragma once

#include "Sto.hh"
#include "MVCCAccess.hh"
#include "MVCCStructs.hh"

template <typename T>
class TMvBox : public TObject {
public:
    typedef T read_type;

    TMvBox() {
    }
    template <typename... Args>
    explicit TMvBox(Args&&... args)
        : v_(std::forward<Args>(args)...) {
    }

    std::pair<bool, read_type> read_nothrow() const {
        auto item = Sto::item(this, 0);
        if (item.has_write())
            return {true, item.template write_value<T>()};
        else {
            history_type *h = v_.find(Sto::read_tid());
            MvAccess::template read<T>(item, h);
            return {true, h->v()};
        }
    }

    read_type read() const {
        auto result = read_nothrow();
        if (!result.first)
            throw Transaction::Abort();
        else
            return result.second;
    }

    void write(const T& x) {
        auto item = Sto::item(this, 0);
        TransProxy(*Sto::transaction(), item).add_write(x);
    }
    void write(T&& x) {
        auto item = Sto::item(this, 0);
        TransProxy(*Sto::transaction(), item).add_write(std::move(x));
    }
    template <typename... Args>
    void write(Args&&... args) {
        auto item = Sto::item(this, 0);
        TransProxy(*Sto::transaction(), item).add_write<T, Args...>(std::forward<Args>(args)...);
    }

    operator read_type() const {
        return read();
    }
    TMvBox<T>& operator=(const T& x) {
        write(x);
        return *this;
    }
    TMvBox<T>& operator=(T&& x) {
        write(std::move(x));
        return *this;
    }
    //template <typename V>
    //TMvBox<T, W>& operator=(V&& x) {
    //    write(std::forward<V>(x));
    //    return *this;
    //}
    TMvBox<T>& operator=(const TMvBox<T>& x) {
        write(x.read());
        return *this;
    }

    const T& nontrans_read() const {
        return v_.nontrans_access();
    }
    T& nontrans_access() {
        return v_.nontrans_access();
    }
    void nontrans_write(const T& x) {
        v_.nontrans_access() = x;
    }
    void nontrans_write(T&& x) {
        v_.nontrans_access() = std::move(x);
    }

    // transactional methods
    bool lock(TransItem& item, Transaction& txn) override {
        history_type *hprev = nullptr;
        if (item.has_read()) {
            hprev = item.read_value<history_type*>();
        } else {
            hprev = v_.find(Sto::read_tid(), false);
        }
        if (Sto::commit_tid() < hprev->rtid()) {
            return false;
        }
        history_type *h = new history_type(
            Sto::commit_tid(), &v_, item.write_value<T>(), hprev);
        bool result = v_.cp_lock(Sto::commit_tid(), h);
        if (!result && !h->status_is(MvStatus::ABORTED)) {
            delete h;
            TransProxy(txn, item).clear_write();
        } else {
            TransProxy(txn, item).add_write(h);
        }
        return result;
    }
    bool check(TransItem& item, Transaction&) override {
        assert(item.has_read());
        fence();
        history_type *hprev = item.read_value<history_type*>();
        return v_.cp_check(Sto::commit_tid(), hprev);
    }
    void install(TransItem& item, Transaction&) override {
        auto h = item.template write_value<history_type*>();
        v_.cp_install(h);
    }
    void unlock(TransItem&) override {
        // no-op
    }
    void cleanup(TransItem& item, bool committed) override {
        if (!committed) {
            if (item.has_write()) {
                auto h = item.template write_value<history_type*>();
                v_.abort(h);
            }
        }
    }
    void print(std::ostream& w, const TransItem& item) const override {
        w << "{TMvBox<" << typeid(T).name() << "> " << (void*) this;
        //if (item.has_read())
        //    w << " R" << item.read_value<history_type*>();
        if (item.has_write())
            w << " =" << item.write_value<T>();
        w << "}";
    }

protected:
    typedef MvObject<T> object_type;
    typedef typename object_type::history_type history_type;

    object_type v_;
};
