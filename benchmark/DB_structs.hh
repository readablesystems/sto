#pragma once

#include "config.h"

#include <cstring>
#include <byteswap.h>

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

    var_string(const var_string &vstr) {
        initialize_from(vstr);
    }

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

    var_string &operator=(const var_string &rhs) {
        initialize_from(rhs);
        return *this;
    }

    var_string &operator=(const var_string &rhs) volatile {
        initialize_from(rhs);
        return *const_cast<var_string *>(this);
    }

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

    const char *c_str() const volatile {
        return s_;
    }

    char *c_str() volatile {
        return const_cast<char *>(s_);
    }

    friend ::std::hash<var_string>;

private:
    void initialize_from(const char *c_str) {
        bzero(s_, ML + 1);
        strncpy(s_, c_str, ML);
    }

    void initialize_from(const std::string &str) {
        initialize_from(str.c_str());
    }

    void initialize_from(const var_string &vstr) {
        memcpy(s_, vstr.s_, ML + 1);
    }

    void initialize_from(const var_string &vstr) volatile {
        memcpy(const_cast<char *>(s_), vstr.s_, ML + 1);
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
        memset(s_, ' ', FL);
        strncpy(s_, c_str, FL);
    }

    fix_string(const std::string &str) {
        memset(s_, ' ', FL);
        strncpy(s_, str.c_str(), FL);
    }

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

    fix_string &operator=(const fix_string &rhs) {
        initialize_from(rhs);
        return *this;
    }

    fix_string &operator=(const fix_string &rhs) volatile {
        initialize_from(rhs);
        return const_cast<fix_string &>(*this);
    }

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
        memset(s_, ' ', FL);
        size_t len = strlen(c_str);
        for (size_t i = 0; i < len && i < FL; ++i)
            s_[i] = c_str[i];
    }

    void initialize_from(const std::string &str) {
        initialize_from(str);
    }

    void initialize_from(const fix_string &fstr) {
        memcpy(s_, fstr.s_, FL);
    }

    void initialize_from(const fix_string &fstr) volatile {
        memcpy(const_cast<char *>(s_), fstr.s_, FL);
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
    else
        always_assert(false);
}

}; // namespace bench