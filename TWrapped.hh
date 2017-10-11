#pragma once
#include "Transaction.hh"
#include <utility>

template <typename T>
struct is_small {
    static constexpr bool value =
            (sizeof(T) <= sizeof(uintptr_t)) && (alignof(T) == sizeof(T));
};

template <typename T, bool Opaque = true,
          bool Trivial = std::is_trivially_copyable<T>::value,
          bool Small = is_small<T>::value
          > class TWrapped;

template <typename T, bool Opaque = true,
          bool Trivial = std::is_trivially_copyable<T>::value,
          bool Small = is_small<T>::value
          > class TLockWrapped;

template <typename T, bool Opaque = true,
          bool Trivial = std::is_trivially_copyable<T>::value,
          bool Small = is_small<T>::value
          > class TSwissWrapped;

class TWrappedAccess {
public:
    // forward declearations no longer needed now that TWrappedAccess is a class
    //template <typename T, typename V>
    //static inline T read_wait_atomic(const T*, TransProxy, const V&, bool);
    //template <typename T, typename V>
    //static inline T read_wait_nonatomic(const T*, TransProxy, const V&, bool);

    template <typename T, typename V>
    static std::pair<bool, T> read_atomic(const T* v, TransProxy item, const V& version, bool add_read) {
#if STO_ABORT_ON_LOCKED
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
                return std::make_pair(item.observe(v1, add_read), result);
            }
            relax_fence();
        }
#else
        return read_wait_atomic(v, item, version, add_read);
#endif
}

    template <typename T, typename V>
    static bool read_atomic(const T* v, TransProxy item, const V& version, bool add_read, T& ret) {
        while (1) {
            V v0 = version;
            fence();

            if (v0.is_locked()) {
                relax_fence();
                continue;
            }
            ret = *v;
            fence();
            V v1 = version;

            if (v0 == v1) {
               return item.observe(v1, add_read);
            }
            relax_fence();
        }
    }

    template <typename T, typename V>
    static std::pair<bool, T> read_nonatomic(const T* v, TransProxy item, const V& version, bool add_read) {
#if STO_ABORT_ON_LOCKED
        if (!item.observe(const_cast<V&>(version), add_read))
            return std::make_pair(false, *v);
        fence();
        return std::make_pair(true, *v);
#else
        return read_wait_nonatomic(v, item, version, add_read);
#endif
    }

    template <typename T, typename V>
    static bool read_nonatomic(const T* v, TransProxy item, const V& version, bool add_read, T& ret) {
        Transaction& t = item.transaction();
        TransItem& it = item.item();

        while (1) {
            V v0 = version;
            fence();

            if (v0.is_locked()) {
                relax_fence();
                continue;
            }

            ret = *v;
            fence();
            V v1 = version;

            if (v0 == v1) {
                if (add_read && !it.has_read()) {
                    it.__or_flags(TransItem::read_bit);
                    it.rdata_ = Packer<TNonopaqueVersion>::pack(t.buf_, std::move(v1));
                    t.any_nonopaque_ = true;
                }
                return true;
            }
            relax_fence();
        }
        //item.observe(version, add_read);
        //fence();
        //return *v;
    }

    template <typename T, typename V>
    static T read_wait_atomic(const T* v, TransProxy item, const V& version, bool add_read) {
        unsigned n = 0;
        while (true) {
            V v0 = version;
            fence();
            T result = *v;
            fence();
            V v1 = version;
            if (v0 == v1 && !v1.is_locked_elsewhere(item.transaction())) {
                item.observe(v1, add_read);
                return result;
            }

            #if !STO_ABORT_ON_LOCKED
            relax_fence();
            continue;
            #endif

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
        while (true) {
            V v0 = version;
            fence();
            if (!v0.is_locked_elsewhere(item.transaction())) {
                item.observe(v0, add_read);
                return *v;
            }
            relax_fence();

            #if !STO_ABORT_ON_LOCKED
            continue;
            #endif

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

    // helper function converting a pointer-read to a by-reference-read for non-trivially-copyable types
    template <typename T>
    static std::pair<bool, const T&> nontrivial_read_to_reference(std::pair<bool, const T*> read_result) {
        static_assert(!std::is_trivially_copyable<T>::value, "Type is trivially-copyable");
        return std::make_pair(read_result.first, *read_result.second);
    }

}; // class TWrappedAccess

template <typename T>
class TLockWrapped<T, true /* opaque */, true /* trivial */, true /* small */> {
public:
    typedef T read_type;
    typedef TLockVersion version_type;

    template <typename... Args> TLockWrapped(Args&&... args)
        : v_(std::forward<Args>(args)...) {}

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
    std::pair<bool, read_type> read(TransProxy item, const version_type& version) const {
        if (item.cc_mode_is_optimistic(version.is_optimistic()))
            return TWrappedAccess::read_atomic(&v_, item, version, true);
        else
            return TWrappedAccess::read_nonatomic(&v_, item, version, true);
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

template <typename T, bool Opaque, bool Trivial, bool Small>
class TLockWrapped {
public:
    template <typename... Args>
    explicit TLockWrapped(Args&&... args)
        : v_(std::forward<Args>(args)...) {
        static_assert(Opaque && Trivial && Small, "not implemented");
    }
protected:
    T v_;
};

template <typename T, bool Opaque, bool Trivial>
class TSwissWrapped<T, Opaque, Trivial, true /* small */> {
public:
    typedef T read_type;
    typedef TSwissVersion<Opaque> version_type;

    template <typename... Args>
    explicit TSwissWrapped(Args&&... args)
            : v_(std::forward<Args>(args)...) {}

    const T& access() const {
        return v_;
    }
    read_type snapshot(TransProxy item, const version_type& version) const {
        if (Opaque || Trivial)
            return TWrappedAccess::read_atomic(&v_, item, version, false);
        else
            return TWrappedAccess::read_nonatomic(&v_, item, version, false);
    }
    read_type wait_snapshot(TransProxy item, const version_type& version, bool add_read) const {
        if (Opaque || Trivial)
            return TWrappedAccess::read_wait_atomic(&v_, item, version, add_read);
        else
            return TWrappedAccess::read_wait_nonatomic(&v_, item, version, add_read);
    }
    std::pair<bool, read_type> read(TransProxy item, const version_type& version) const {
        if (Opaque || Trivial)
            return TWrappedAccess::read_atomic(&v_, item, version, true);
        else
            return TWrappedAccess::read_nonatomic(&v_, item, version, true);
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

template <typename T, bool Opaque, bool Trivial, bool Small>
class TSwissWrapped {
public:
    template <typename... Args>
    explicit TSwissWrapped(Args&&... args)
            : v_(std::forward<Args>(args)...) {
        static_assert(Small, "not implemented");
    }
protected:
    T v_;
};

template <typename T, bool Small>
class TWrapped<T, true /* opaque */, true /* trivial */, Small /* small */> {
public:
    typedef T read_type;
    typedef TVersion version_type;

    template <typename... Args>
    explicit TWrapped(Args&&... args)
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
    std::pair<bool, read_type> read(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_atomic(&v_, item, version, true);
    }
    static std::pair<bool, read_type> read(const T* v, TransProxy item, const version_type& version) {
        static_assert(Small, "static read only available for small types");
        return TWrappedAccess::read_atomic(v, item, version, true);
    }
    bool read(TransProxy item, const version_type& version, read_type& ret) const {
        return TWrappedAccess::read_atomic(&v_, item, version, true, ret);
    }
    static bool read(const T* v, TransProxy item, const version_type& version, read_type& ret) {
        return TWrappedAccess::read_atomic(v, item, version, true, ret);
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

#if 0
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
#endif

template <typename T, bool Small>
class TWrapped<T, false /* !opaque */, true /* trivial */, Small /* small */> {
public:
    typedef T read_type;
    typedef TNonopaqueVersion version_type;

    TWrapped()
        : v_() {
    }
    explicit TWrapped(const T& v)
        : v_(v) {
    }
    explicit TWrapped(T&& v)
        : v_(std::move(v)) {
    }
    template <typename... Args>
    explicit TWrapped(Args&&... args)
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
        if (Small)
            return TWrappedAccess::read_wait_nonatomic(&v_, item, version, add_read);
        else
            return TWrappedAccess::read_wait_atomic(&v_, item, version, add_read);
    }
    std::pair<bool, read_type> read(TransProxy item, const version_type& version) const {
        if (Small)
            return TWrappedAccess::read_nonatomic(&v_, item, version, true);
        else
            return TWrappedAccess::read_atomic(&v_, item, version, true);
    }
    bool read(TransProxy item, const version_type& version, read_type& ret) const {
        if (Small)
            return TWrappedAccess::read_nonatomic(&v_, item, version, true, ret);
        else
            return TWrappedAccess::read_atomic(&v_, item, version, true, ret);
    }
    static bool read(const T* vp, TransProxy item, const version_type& version, read_type& ret) {
        static_assert(Small, "static read only available for small types");
        return TWrappedAccess::read_nonatomic(vp, item, version, true, ret);
    }

    template <typename Q = T>
    typename std::enable_if<is_small<Q>::value, void>::type
    write(T v) {
        static_assert(Small, "write-by-value only available for small types");
        v_ = v;
    }

    template <typename Q = T>
    typename std::enable_if<!is_small<Q>::value, void>::type
    write(const T& v) {
        static_assert(!Small, "write-by-lvalue-reference only available for non-small types");
        v_ = v;
    }

    template <typename Q = T>
    typename std::enable_if<!is_small<Q>::value, void>::type
    write(T&& v) {
        static_assert(!Small, "write-with-move only available for non-small types");
        v_ = std::move(v);
    }

private:
    T v_;
};

#if 0
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
#endif

template <typename T, bool Opaque, bool Small>
class TWrapped<T, Opaque, false /* !trivial */, Small> {
public:
    typedef const T& read_type;
    typedef typename std::conditional<Opaque, TVersion, TNonopaqueVersion>::type version_type;

    TWrapped()
        : vp_(new T) {
    }
    explicit TWrapped(const T& v)
        : vp_(new T(v)) {
    }
    explicit TWrapped(T&& v)
        : vp_(new T(std::move(v))) {
    }
    template <typename... Args>
    explicit TWrapped(Args&&... args)
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
        if (Opaque)
            return *TWrappedAccess::read_wait_atomic(&vp_, item, version, false);
        else
            return *vp_;
    }
    read_type wait_snapshot(TransProxy item, const version_type& version, bool add_read) const {
        if (Opaque)
            return *TWrappedAccess::read_wait_atomic(&vp_, item, version, add_read);
        else
            return *TWrappedAccess::read_wait_nonatomic(&vp_, item, version, add_read);
    }
    std::pair<bool, read_type> read(TransProxy item, const version_type& version) const {
        if (Opaque) {
            return TWrappedAccess::nontrivial_read_to_reference(
                    TWrappedAccess::read_atomic(&vp_, item, version, true));
        } else {
            return TWrappedAccess::nontrivial_read_to_reference(
                    TWrappedAccess::read_nonatomic(&vp_, item, version, true));
        }
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

#if 0
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
    std::pair<bool, read_type> read(TransProxy item, const version_type& version) const {
        return TWrappedAccess::read_nonatomic(&vp_, item, version, true);
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
#endif

template <typename T> using TOpaqueWrapped = TWrapped<T>;
template <typename T> using TNonopaqueWrapped = TWrapped<T, false>;

template <typename T> using TOpaqueLockWrapped = TLockWrapped<T>;
template <typename T> using TSwissNonopaqueWrapped = TSwissWrapped<T, false>;
