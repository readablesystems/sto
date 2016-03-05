#pragma once
#include "Transaction.hh"
#include <utility>

template <typename T, bool Opaque = true,
          bool Trivial = mass::is_trivially_copyable<T>::value,
          bool Small = sizeof(T) <= sizeof(uintptr_t) && alignof(T) == sizeof(T)
          > class TWrapped;

namespace TWrappedAccess {
template <typename T, typename V>
static T read_opaque(const T* v, TransProxy item, const V& version) {
    V v0 = version, v1;
    fence();
    while (1) {
        T result = *v;
        fence();
        v1 = version;
        if (v0 == v1 || v1.is_locked()) {
            item.observe(v1);
            return result;
        }
        v0 = v1;
        relax_fence();
    }
}
template <typename T, typename V>
static T read_nonopaque_small(const T* v, TransProxy item, const V& version) {
    item.observe(version);
    fence();
    return *v;
}
template <typename T, typename V>
static T read_snapshot_small(const T* v, TransProxy item, const V& version) {
    T result = *v;
    fence();
    item.observe_opacity(version);
    return result;
}
template <typename T, typename V>
static T read_snapshot_large(const T* v, TransProxy item, const V& version) {
    V v0 = version, v1;
    fence();
    while (1) {
        T result = *v;
        fence();
        v1 = version;
        if (v0 == v1 || v1.is_locked()) {
            item.observe_opacity(v1);
            return result;
        }
        v0 = v1;
        relax_fence();
    }
}
}

template <typename T>
class TWrapped<T, true /* opaque */, true /* trivial */, true /* small */> {
public:
    typedef T read_type;
    typedef TVersion version_type;

    template <typename... Args> TWrapped(Args&&... args)
        : v_{std::forward<Args>(args)...} {
    }

    const T& access() const {
        return v_;
    }
    T& access() {
        return v_;
    }
    read_type snapshot(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_snapshot_small(&v_, item, version);
    }
    read_type read(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_opaque(&v_, item, version);
    }
    static read_type read(const T* v, TransProxy item, const version_type& version) {
        return TWrappedAccess::read_opaque(v, item, version);
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
        : v_{std::forward<Args>(args)...} {
    }

    const T& access() const {
        return v_;
    }
    T& access() {
        return v_;
    }
    read_type snapshot(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_snapshot_large(&v_, item, version);
    }
    read_type read(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_opaque(&v_, item, version);
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
    read_type read(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_nonopaque_small(&v_, item, version);
    }
    static read_type read(const T* vp, TransProxy item, const version_type& version) {
        return TWrappedAccess::read_nonopaque_small(vp, item, version);
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
        return TWrappedAccess::read_snapshot_large(&v_, item, version);
    }
    read_type read(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_snapshot_large(&v_, item, version);
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
        return *TWrappedAccess::read_snapshot_small(&vp_, item, version);
    }
    read_type read(TransProxy item, const version_type& version) const {
        return *TWrappedAccess::read_opaque(&vp_, item, version);
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
    read_type read(TransProxy item, const version_type& version) const {
        return *TWrappedAccess::read_nonopaque_small(&vp_, item, version);
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
