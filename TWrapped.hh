#pragma once
#include "Transaction.hh"
#include <utility>

template <typename T> class TLargeTrivialWrapped {
public:
    typedef T read_type;
    typedef TVersion version_type;

    template <typename... Args> TLargeTrivialWrapped(Args&&... args)
        : v_(std::forward(args)...) {
    }

    const T& access() const {
        return v_;
    }
    T& access() {
        return v_;
    }

    read_type snapshot(const version_type& version) const {
        version_type v0 = version, v1;
        T result;
        acquire_fence();
        while (1) {
            result = v_;
            release_fence();
            v1 = version;
            if (v0 == v1 && !v1.is_locked())
                break;
            v0 = v1;
            relax_fence();
        }
        return result;
    }
    read_type read(TransProxy item, const version_type& version) const {
        version_type v0 = version, v1;
        T result;
        acquire_fence();
        while (1) {
            result = v_;
            release_fence();
            v1 = version;
            if (v0 == v1 || v1.is_locked())
                break;
            v0 = v1;
            relax_fence();
        }
        item.observe(v1);
        return result;
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

template <typename T> class TSmallTrivialWrapped : public TLargeTrivialWrapped<T> {
public:
    typedef typename TLargeTrivialWrapped<T>::read_type read_type;
    typedef typename TLargeTrivialWrapped<T>::version_type version_type;

    template <typename... Args> TSmallTrivialWrapped(Args&&... args)
        : TLargeTrivialWrapped<T>(std::forward(args)...) {
    }

    read_type snapshot(const version_type&) const {
        return this->v_;
    }
};

template <typename T> class TSmallTrivialNonopaqueWrapped {
public:
    typedef T read_type;
    typedef TNonopaqueVersion version_type;

    TSmallTrivialNonopaqueWrapped()
        : v_() {
    }
    TSmallTrivialNonopaqueWrapped(const T& v)
        : v_(v) {
    }
    TSmallTrivialNonopaqueWrapped(T&& v)
        : v_(std::move(v)) {
    }
    template <typename... Args> TSmallTrivialNonopaqueWrapped(Args&&... args)
        : v_(std::forward<Args>(args)...) {
    }

    const T& access() const {
        return v_;
    }
    T& access() {
        return v_;
    }

    read_type snapshot(const version_type&) const {
        return v_;
    }
    read_type read(TransProxy item, const version_type& version) const {
        item.observe(version);
        acquire_fence();
        return v_;
    }
    void write(T v) {
        v_ = v;
    }

private:
    T v_;
};

template <typename T> class TLargeTrivialNonopaqueWrapped {
public:
    typedef T read_type;
    typedef TNonopaqueVersion version_type;

    TLargeTrivialNonopaqueWrapped()
        : v_() {
    }
    TLargeTrivialNonopaqueWrapped(const T& v)
        : v_(v) {
    }
    TLargeTrivialNonopaqueWrapped(T&& v)
        : v_(std::move(v)) {
    }
    template <typename... Args> TLargeTrivialNonopaqueWrapped(Args&&... args)
        : v_(std::forward<Args>(args)...) {
    }

    const T& access() const {
        return v_;
    }
    T& access() {
        return v_;
    }

    read_type snapshot(const version_type& version) const {
        version_type v0 = version, v1;
        T result;
        acquire_fence();
        while (1) {
            result = v_;
            release_fence();
            v1 = version;
            if (v0 == v1 && !v1.is_locked())
                break;
            v0 = v1;
            relax_fence();
        }
        return result;
    }
    read_type read(TransProxy item, const version_type& version) const {
        version_type v0 = version, v1;
        T result;
        acquire_fence();
        while (1) {
            result = v_;
            release_fence();
            v1 = version;
            if (v0 == v1 || v1.is_locked())
                break;
            v0 = v1;
            relax_fence();
        }
        item.observe(v1);
        return result;
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

template <typename T> class TNontrivialWrapped {
public:
    typedef const T& read_type;
    typedef TVersion version_type;

    TNontrivialWrapped()
        : vp_(new T) {
    }
    TNontrivialWrapped(const T& v)
        : vp_(new T(v)) {
    }
    TNontrivialWrapped(T&& v)
        : vp_(new T(std::move(v))) {
    }
    template <typename... Args> TNontrivialWrapped(Args&&... args)
        : vp_(new T(std::forward<Args>(args)...)) {
    }
    ~TNontrivialWrapped() {
        T* vp = vp_;
        Transaction::rcu_cleanup([vp](){ delete vp; });
    }

    const T& access() const {
        return *vp_;
    }
    T& access() {
        return *vp_;
    }

    read_type snapshot(const version_type&) const {
        return *vp_;
    }
    read_type read(TransProxy item, const version_type& version) const {
        version_type v0 = version, v1;
        T* resultp;
        acquire_fence();
        while (1) {
            resultp = vp_;
            release_fence();
            v1 = version;
            if (v0 == v1 || v1.is_locked())
                break;
            v0 = v1;
            relax_fence();
        }
        item.observe(v1);
        return *resultp;
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
        T* vp = vp_;
        Transaction::rcu_cleanup([vp](){ delete vp; });
        vp_ = new_vp;
    }
};

template <typename T> class TNontrivialNonopaqueWrapped {
public:
    typedef const T& read_type;
    typedef TNonopaqueVersion version_type;

    TNontrivialNonopaqueWrapped()
        : vp_(new T) {
    }
    TNontrivialNonopaqueWrapped(const T& v)
        : vp_(new T(v)) {
    }
    TNontrivialNonopaqueWrapped(T&& v)
        : vp_(new T(std::move(v))) {
    }
    template <typename... Args> TNontrivialNonopaqueWrapped(Args&&... args)
        : vp_(new T(std::forward<Args>(args)...)) {
    }
    ~TNontrivialNonopaqueWrapped() {
        T* vp = vp_;
        Transaction::rcu_cleanup([vp](){ delete vp; });
    }

    const T& access() const {
        return *vp_;
    }
    T& access() {
        return *vp_;
    }

    read_type snapshot(const version_type&) const {
        return *vp_;
    }
    read_type read(TransProxy item, const version_type& version) const {
        item.observe(version);
        acquire_fence();
        return *vp_;
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
        T* vp = vp_;
        Transaction::rcu_cleanup([vp](){ delete vp; });
        vp_ = new_vp;
    }
};


template <typename T, bool Opaque = true,
          bool Trivial = mass::is_trivially_copyable<T>::value,
          bool Small = sizeof(T) <= sizeof(uintptr_t) && alignof(T) == sizeof(T)
          > class TWrapped;

template <typename T> class TWrapped<T, true, true, true> : public TSmallTrivialWrapped<T> {};
template <typename T> class TWrapped<T, true, true, false> : public TLargeTrivialWrapped<T> {};
template <typename T, bool Small> class TWrapped<T, true, false, Small> : public TNontrivialWrapped<T> {};
template <typename T> class TWrapped<T, false, true, true> : public TSmallTrivialNonopaqueWrapped<T> {};
template <typename T> class TWrapped<T, false, true, false> : public TLargeTrivialNonopaqueWrapped<T> {};
template <typename T, bool Small> class TWrapped<T, false, false, Small> : public TNontrivialNonopaqueWrapped<T> {};


template <typename T, bool Trivial = mass::is_trivially_copyable<T>::value,
          bool Small = sizeof(T) <= sizeof(uintptr_t) && alignof(T) == sizeof(T)
          > class TNonopaqueWrapped;

template <typename T> class TNonopaqueWrapped<T, true, true> : public TSmallTrivialNonopaqueWrapped<T> {};
template <typename T> class TNonopaqueWrapped<T, true, false> : public TLargeTrivialNonopaqueWrapped<T> {};
template <typename T, bool Small> class TNonopaqueWrapped<T, false, Small> : public TNontrivialNonopaqueWrapped<T> {};
