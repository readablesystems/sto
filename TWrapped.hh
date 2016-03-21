#pragma once
#include "Transaction.hh"
#include <utility>

template <typename T, bool Opaque = true,
          bool Trivial = mass::is_trivially_copyable<T>::value,
          bool Small = sizeof(T) <= sizeof(uintptr_t) && alignof(T) == sizeof(T)
          > class TWrapped;

namespace TWrappedAccess {
template <typename T, typename V>
static T read_atomic(const T* v, TransProxy item, const V& version, bool add_read) {
    // This version returns immediately if v1 is locked. We assume as a result
    // that we will quickly converge to either `v0 == v1` or `v1.is_locked()`,
    // and don't bother to back off.
    while (1) {
        V v0 = version;
        fence();
        T result = *v;
        fence();
        V v1 = version;
        if (v0 == v1 || v1.is_locked()) {
            item.observe(v1, add_read);
            return result;
        }
        relax_fence();
    }
}
template <typename T, typename V>
static T read_nonatomic(const T* v, TransProxy item, const V& version, bool add_read) {
    item.observe(version, add_read);
    fence();
    return *v;
}
template <typename T, typename V>
static T read_wait_atomic(const T* v, TransProxy item, const V& version, bool add_read) {
    unsigned n = 0;
    while (1) {
        V v0 = version;
        fence();
        T result = *v;
        fence();
        V v1 = version;
        if (v0 == v1 && !v1.is_locked_elsewhere(item.transaction())) {
            item.observe(v1, add_read);
            return result;
        }
#if STO_SPIN_EXPBACKOFF
        if (++n > STO_SPIN_BOUND_WAIT)
            Sto::abort();
        if (n > 3)
            for (unsigned x = 1 << std::min(15U, n - 2); x; --x)
                relax_fence();
#else
        if (++n > (1 << STO_SPIN_BOUND_WAIT))
            Sto::abort();
#endif
        relax_fence();
    }
}
template <typename T, typename V>
static T read_wait_nonatomic(const T* v, TransProxy item, const V& version, bool add_read) {
    unsigned n = 0;
    while (1) {
        V v0 = version;
        fence();
        if (!v0.is_locked_elsewhere(item.transaction())) {
            item.observe(v0, add_read);
            return *v;
        }
        relax_fence();
#if STO_SPIN_EXPBACKOFF
        if (++n > STO_SPIN_BOUND_WAIT)
            Sto::abort();
        if (n > 3)
            for (unsigned x = 1 << std::min(15U, n - 2); x; --x)
                relax_fence();
#else
        if (++n > (1 << STO_SPIN_BOUND_WAIT))
            Sto::abort();
#endif
    }
}
}

template <typename T>
class TWrapped<T, true /* opaque */, true /* trivial */, true /* small */> {
public:
    typedef T read_type;
    typedef TVersion version_type;

    template <typename... Args> TWrapped(Args&&... args)
        : v_(std::forward<Args>(args)...) {
    }

    const T& access() const {
        return v_;
    }
    T& access() {
        return v_;
    }
    read_type snapshot(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_atomic(&v_, item, version, false);
    }
    read_type wait_snapshot(TransProxy item, const version_type& version, bool add_read) const {
        return TWrappedAccess::read_wait_atomic(&v_, item, version, add_read);
    }
    read_type read(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_atomic(&v_, item, version, true);
    }
    static read_type read(const T* v, TransProxy item, const version_type& version) {
        return TWrappedAccess::read_atomic(v, item, version, true);
    }
    void write(const T& v) {
        v_ = v;
    }
    void write(T&& v) {
        v_ = std::move(v);
    }

protected:
    T v_;
};

template <typename T>
class TWrapped<T, true /* opaque */, true /* trivial */, false /* !small */> {
public:
    typedef T read_type;
    typedef TVersion version_type;

    template <typename... Args> TWrapped(Args&&... args)
        : v_(std::forward<Args>(args)...) {
    }

    const T& access() const {
        return v_;
    }
    T& access() {
        return v_;
    }
    read_type snapshot(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_atomic(&v_, item, version, false);
    }
    read_type wait_snapshot(TransProxy item, const version_type& version, bool add_read) const {
        return TWrappedAccess::read_wait_atomic(&v_, item, version, add_read);
    }
    read_type read(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_atomic(&v_, item, version, true);
    }
    void write(const T& v) {
        v_ = v;
    }
    void write(T&& v) {
        v_ = std::move(v);
    }

protected:
    T v_;
};

template <typename T>
class TWrapped<T, false /* !opaque */, true /* trivial */, true /* small */> {
public:
    typedef T read_type;
    typedef TNonopaqueVersion version_type;

    TWrapped()
        : v_() {
    }
    TWrapped(const T& v)
        : v_(v) {
    }
    TWrapped(T&& v)
        : v_(std::move(v)) {
    }
    template <typename... Args> TWrapped(Args&&... args)
        : v_(std::forward<Args>(args)...) {
    }

    const T& access() const {
        return v_;
    }
    T& access() {
        return v_;
    }
    read_type snapshot(TransProxy, const version_type&) const {
        return v_;
    }
    read_type wait_snapshot(TransProxy item, const version_type& version, bool add_read) const {
        return TWrappedAccess::read_wait_nonatomic(&v_, item, version, add_read);
    }
    read_type read(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_nonatomic(&v_, item, version, true);
    }
    static read_type read(const T* vp, TransProxy item, const version_type& version) {
        return TWrappedAccess::read_nonatomic(vp, item, version, true);
    }
    void write(T v) {
        v_ = v;
    }

private:
    T v_;
};

template <typename T>
class TWrapped<T, false /* !opaque */, true /* trivial */, false /* !small */> {
public:
    typedef T read_type;
    typedef TNonopaqueVersion version_type;

    TWrapped()
        : v_() {
    }
    TWrapped(const T& v)
        : v_(v) {
    }
    TWrapped(T&& v)
        : v_(std::move(v)) {
    }
    template <typename... Args> TWrapped(Args&&... args)
        : v_(std::forward<Args>(args)...) {
    }

    const T& access() const {
        return v_;
    }
    T& access() {
        return v_;
    }
    read_type snapshot(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_atomic(&v_, item, version, false);
    }
    read_type wait_snapshot(TransProxy item, const version_type& version, bool add_read) const {
        return TWrappedAccess::read_wait_atomic(&v_, item, version, add_read);
    }
    read_type read(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_atomic(&v_, item, version, true);
    }
    void write(const T& v) {
        v_ = v;
    }
    void write(T&& v) {
        v_ = std::move(v);
    }

private:
    T v_;
};

template <typename T, bool Small>
class TWrapped<T, true /* opaque */, false /* !trivial */, Small> {
public:
    typedef const T& read_type;
    typedef TVersion version_type;

    TWrapped()
        : vp_(new T) {
    }
    TWrapped(const T& v)
        : vp_(new T(v)) {
    }
    TWrapped(T&& v)
        : vp_(new T(std::move(v))) {
    }
    template <typename... Args> TWrapped(Args&&... args)
        : vp_(new T(std::forward<Args>(args)...)) {
    }
    ~TWrapped() {
        Transaction::rcu_delete(vp_);
    }

    const T& access() const {
        return *vp_;
    }
    T& access() {
        return *vp_;
    }
    read_type snapshot(TransProxy item, const version_type& version) const {
        return *TWrappedAccess::read_atomic(&vp_, item, version, false);
    }
    read_type wait_snapshot(TransProxy item, const version_type& version, bool add_read) const {
        return *TWrappedAccess::read_wait_atomic(&vp_, item, version, add_read);
    }
    read_type read(TransProxy item, const version_type& version) const {
        return *TWrappedAccess::read_atomic(&vp_, item, version, true);
    }
    void write(const T& v) {
        save(new T(v));
    }
    void write(T&& v) {
        save(new T(std::move(v)));
    }

private:
    T* vp_;

    void save(T* new_vp) {
        Transaction::rcu_delete(vp_);
        vp_ = new_vp;
    }
};

template <typename T, bool Small>
class TWrapped<T, false /* !opaque */, false /* !trivial */, Small> {
public:
    typedef const T& read_type;
    typedef TNonopaqueVersion version_type;

    TWrapped()
        : vp_(new T) {
    }
    TWrapped(const T& v)
        : vp_(new T(v)) {
    }
    TWrapped(T&& v)
        : vp_(new T(std::move(v))) {
    }
    template <typename... Args> TWrapped(Args&&... args)
        : vp_(new T(std::forward<Args>(args)...)) {
    }
    ~TWrapped() {
        Transaction::rcu_delete(vp_);
    }

    const T& access() const {
        return *vp_;
    }
    T& access() {
        return *vp_;
    }
    read_type snapshot(TransProxy, const version_type&) const {
        return *vp_;
    }
    read_type wait_snapshot(TransProxy item, const version_type& version, bool add_read) const {
        return *TWrappedAccess::read_wait_nonatomic(&vp_, item, version, add_read);
    }
    read_type read(TransProxy item, const version_type& version) const {
        return *TWrappedAccess::read_nonatomic(&vp_, item, version, true);
    }
    void write(const T& v) {
        save(new T(v));
    }
    void write(T&& v) {
        save(new T(std::move(v)));
    }

private:
    T* vp_;

    void save(T* new_vp) {
        Transaction::rcu_delete(vp_);
        vp_ = new_vp;
    }
};


template <typename T> using TOpaqueWrapped = TWrapped<T>;
template <typename T> using TNonopaqueWrapped = TWrapped<T, false>;
