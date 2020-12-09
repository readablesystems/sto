#pragma once

#include "config.h"

#include <string>
#include <iostream>
#include <cstring>
#if defined(__APPLE__)
#  include <libkern/OSByteOrder.h>
#  define __bswap_16 OSSwapInt16
#  define __bswap_32 OSSwapInt32
#  define __bswap_64 OSSwapInt64
#else
#include <byteswap.h>
#endif

#include "str.hh"

namespace bench {

template<size_t ML>
class __attribute__((packed)) var_string {
public:
    static constexpr size_t max_length = ML;

    var_string() {
        bzero(s_, ML + 1);
    }

    var_string(const char *c_str) {
        initialize_from(c_str);
    }

    var_string(const std::string &str) {
        initialize_from(str);
    }

    var_string(const var_string &vstr) = default;

    bool operator==(const char *c_str) const {
        return !strncmp(s_, c_str, ML);
    }

    bool operator==(const std::string &str) const {
        return !strncmp(s_, str.c_str(), ML);
    }

    bool operator==(const var_string &rhs) const {
        return !strncmp(s_, rhs.s_, ML);
    }

    char operator[](size_t idx) const {
        return s_[idx];
    }

    var_string &operator=(const var_string &rhs) = default;

    explicit operator std::string() {
        return std::string(s_);
    }

    bool contains(const char *substr) const {
        return (strstr(s_, substr) != nullptr);
    }

    size_t length() const {
        return strlen(s_);
    }

    bool place(const char *str, size_t pos) {
        size_t in_len = strlen(str);
        if (pos + in_len > length())
            return false;
        memcpy(s_ + pos, str, in_len);
        return true;
    }

    const char *c_str() const {
        return s_;
    }

    char *c_str() {
        return s_;
    }

    friend ::std::hash<var_string>;

private:
    void initialize_from(const char *c_str) {
        size_t l = std::min(strlen(c_str), ML);
        memcpy(s_, c_str, l + 1);
        memset(s_ + l + 1, 0, ML - l);
    }

    void initialize_from(const std::string &str) {
        size_t l = std::min(str.length(), ML);
        memcpy(s_, str.data(), l + 1);
        memset(s_ + l + 1, 0, ML - l);
    }

    char s_[ML + 1];
};

template<size_t FL>
class __attribute__((packed)) fix_string {
public:
    fix_string() {
        memset(s_, ' ', FL);
    }

    fix_string(const char *c_str) {
        initialize_from(c_str);
    }

    fix_string(const std::string &str) {
        initialize_from(str);
    }

    fix_string(const fix_string&) = default;

    bool operator==(const char *c_str) const {
        return strlen(c_str) == FL && !memcmp(s_, c_str, FL);
    }

    bool operator==(const fix_string &rhs) const {
        return !memcmp(s_, rhs.s_, FL);
    }

    char &operator[](size_t idx) {
        return s_[idx];
    }

    char operator[](size_t idx) const {
        return s_[idx];
    }

    fix_string &operator=(const fix_string &rhs) = default;

    explicit operator std::string() {
        return std::string(s_, FL);
    }

    void insert_left(const char *buf, size_t cnt) {
        memmove(s_ + cnt, s_, FL - cnt);
        memcpy(s_, buf, cnt);
    }

    friend ::std::hash<fix_string>;

private:
    void initialize_from(const char *c_str) {
        size_t l = std::min(strlen(c_str), FL);
        memcpy(s_, c_str, l);
        memset(s_ + l, ' ', FL - l);
    }

    void initialize_from(const std::string &str) {
        size_t l = std::min(str.length(), FL);
        memcpy(s_, str.data(), l);
        memset(s_ + l, ' ', FL - l);
    }

    char s_[FL];
};

// swap byte order so key can be used correctly in masstree
template<typename IntType>
static inline IntType bswap(IntType x) {
    if (sizeof(IntType) == 4)
        return __bswap_32(x);
    else if (sizeof(IntType) == 8)
        return __bswap_64(x);
    else if (sizeof(IntType) == 2)
        return __bswap_16(x);
    else
        always_assert(false);
}

struct dummy_row {
    enum class NamedColumn : int { dummy = 0 };
    uintptr_t dummy;
    static dummy_row row;
};

template <typename K>
struct masstree_key_adapter : public K {
    // Conversions from and to masstree key type
    explicit masstree_key_adapter(const lcdf::Str& mt_key) {
        assert(mt_key.length() == sizeof(*this));
        memcpy(this, mt_key.data(), sizeof(*this));
    }

    template <typename... Args>
    explicit masstree_key_adapter(Args&&... args)
            : K(std::forward<Args>(args)...) {}

    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }
};

}; // namespace bench

template <size_t FL>
std::ostream& operator<<(std::ostream& w, const bench::fix_string<FL>& str) {
    (void)str;
    w << "fix_string<" << FL << ">" << std::endl;
    return w;
}

