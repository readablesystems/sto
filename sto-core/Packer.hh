#pragma once
#include "compiler.hh"
#include <algorithm>

class TransactionBuffer;

// Packer
template <typename T>
struct __attribute__((may_alias)) Aliasable {
    T x;
};

template <typename T,
          bool simple = (mass::is_trivially_copyable<T>::value
                         && sizeof(T) <= sizeof(void*))>
    struct Packer {};


// TransactionBuffer
template <typename T> struct ObjectDestroyer {
    static void destroy(void* object) {
        reinterpret_cast<T*>(object)->~T();
    }
    static void destroy_and_free(void* object) {
        delete reinterpret_cast<T*>(object);
    }
    static void destroy_and_free_array(void* object) {
        delete[] reinterpret_cast<T*>(object);
    }
    static void free(void* object) {
        ::free(object);
    }
};

template <typename T> struct UniqueKey {
    T key;
    template <typename... Args>
    UniqueKey(Args&&... args)
        : key(std::forward<Args>(args)...) {
    }
    bool operator==(const T& x) const {
        return key == x;
    }
};

class TransactionBuffer {
    struct elt;
    struct item;

public:
    TransactionBuffer()
        : e_(), linked_size_(0) {
    }
    ~TransactionBuffer() {
        if (e_)
            hard_clear(true);
    }

    static constexpr size_t aligned_size(size_t x) {
        return (x + 7) & ~7;
    }

    template <typename T, typename... Args>
    inline T* allocate(Args&&... args);

    template <typename T, typename U = T>
    const T* find(const U& x) const;

    size_t buffer_size() const {
        return linked_size_ + (e_ ? e_->pos : 0);
    }
    void clear() {
        if (e_ && e_->pos)
            hard_clear(false);
    }

private:
    static constexpr size_t default_capacity = 4080;
    struct itemhdr {
        void (*destroyer)(void*);
        size_t size;
    };
    struct item : public itemhdr {
        char buf[0];
    };
    struct elthdr {
        elt* next;
        size_t pos;
        size_t capacity;
    };
    struct elt : public elthdr {
        char buf[0];
        void clear() {
            size_t off = 0;
            while (off < pos) {
                itemhdr* i = (itemhdr*) &buf[off];
                i->destroyer(i + 1);
                off += i->size;
            }
            pos = 0;
        }
    };
    elt* e_;
    size_t linked_size_;

    item* get_space(size_t needed) {
        if (!e_ || e_->pos + needed > e_->capacity)
            hard_get_space(needed);
        e_->pos += needed;
        return (item*) &e_->buf[e_->pos - needed];
    }
    void hard_get_space(size_t needed);
    void hard_clear(bool delete_all);
};

template <typename T, typename... Args>
T* TransactionBuffer::allocate(Args&&... args) {
    size_t isize = aligned_size(sizeof(itemhdr) + sizeof(T));
    item* space = this->get_space(isize);
    space->destroyer = ObjectDestroyer<T>::destroy;
    space->size = isize;
    return new (&space->buf[0]) T(std::forward<Args>(args)...);
}

template <typename T, typename U>
const T* TransactionBuffer::find(const U& x) const {
    void (*destroyer)(void*) = ObjectDestroyer<T>::destroy;
    for (elt* e = e_; e; e = e->next)
        for (size_t off = 0; off < e->pos; ) {
            item* i = (item*) &e->buf[off];
            if (i->destroyer == destroyer) {
                const Aliasable<T>* contents = (const Aliasable<T>*) &i->buf[0];
                if (contents->x == x)
                    return &contents->x;
            }
            off += i->size;
        }
    return nullptr;
}



template <typename T> struct Packer<T, true> {
    static constexpr bool is_simple = true;
    typedef T type;
    static void* pack(TransactionBuffer&, const T& x) {
        void* v = 0;
        memcpy(&v, &x, sizeof(T));
        return v;
    }
    static void* pack_unique(TransactionBuffer& buf, const T& x) {
        return pack(buf, x);
    }
    static void* repack(TransactionBuffer& buf, void*, const T& x) {
        return pack(buf, x);
    }
    static T& unpack(void*& p) {
        return *(T*) &p;
    }
    static const T& unpack(void* const& p) {
        return *(const T*) &p;
    }
};

template <typename T> struct Packer<T, false> {
    static constexpr bool is_simple = false;
    typedef T type;
    template <typename... Args>
    static void* pack(TransactionBuffer& buf, Args&&... args) {
        return buf.template allocate<T>(std::forward<Args>(args)...);
    }
    static void* pack_unique(TransactionBuffer& buf, const T& x) {
        if (const void* ptr = buf.template find<UniqueKey<T> >(x))
            return const_cast<void*>(ptr);
        else
            return buf.template allocate<UniqueKey<T> >(x);
    }
    static void* repack(TransactionBuffer&, void* p, const T& x) {
        unpack(p) = x;
        return p;
    }
    static void* repack(TransactionBuffer&, void* p, T&& x) {
        unpack(p) = std::move(x);
        return p;
    }
    static T& unpack(void* p) {
        return *(T*) p;
    }
};
