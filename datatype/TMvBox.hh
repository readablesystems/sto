#pragma once

#include "Sto.hh"
#include "MVCC.hh"

class TMvBoxAccess;

template <typename T>
class TMvBox : public TObject {
public:
    typedef T read_type;
    typedef typename commutators::Commutator<T> comm_type;

    explicit TMvBox() : v_() {
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
        Sto::item(this, 0).add_write(x);
    }
    void write(T&& x) {
        Sto::item(this, 0).add_write(std::move(x));
    }
    template <typename... Args>
    void write(Args&&... args) {
        Sto::item(this, 0).template add_write<T, Args...>(std::forward<Args>(args)...);
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
        if (item.has_read()) {
            auto hprev = item.read_value<history_type*>();
            if (Sto::commit_tid() < hprev->rtid()) {
                TransProxy(txn, item).add_write(nullptr);
                return false;
            }
        }
        history_type *h;
        if (item.has_commute()) {
            auto wval = item.template write_value<comm_type>();
            h = v_.new_history(Sto::commit_tid(), std::move(wval));
        } else {
            h = v_.new_history(Sto::commit_tid(), item.write_value<T>());
        }
        bool result = v_.cp_lock(Sto::commit_tid(), h);
        if (!result && !h->status_is(MvStatus::ABORTED)) {
            v_.delete_history(h);
            TransProxy(txn, item).add_write(nullptr);
        } else {
            TransProxy(txn, item).add_write(h);
            TransProxy(txn, item).clear_commute();
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
                if (h) {
                    h->status_txn_abort();
                }
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

    friend class TMvBoxAccess;
};

class TMvCommuteIntegerBox : public TMvBox<int64_t> {
public:
    TMvCommuteIntegerBox& operator=(const int64_t& x) {
        this->nontrans_write(x);
        return *this;
    }

    void increment(int64_t delta) {
        typedef TMvBox<int64_t>::comm_type comm_type;
        auto item = Sto::item(this, 0);
        item.add_commute(comm_type(delta));
    }
};

// For unit tests to be able to access the underlying MVCC object...
class TMvBoxAccess {
public:
    template <typename T>
    static MvHistory<T>* head(const TMvBox<T> &box) {
        auto rtid = Sto::read_tid();
        return box.v_.find(rtid);
    }
};
