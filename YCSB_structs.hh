#pragma once

#include "str.hh" // lcdf::Str

namespace ycsb {

enum class mode_id : int { ReadOnly = 0, MediumContention, HighContention };

template <size_t FL>
class fix_string {
public:
    fix_string() {
        memset(s_, ' ', FL);
    }
    fix_string(const char *c_str) {
        memset(s_, ' ', FL);
        strncpy(s_, c_str, FL);
    }
    fix_string(const std::string& str) {
        memset(s_, ' ', FL);
        strncpy(s_, str.c_str(), FL);
    }

    bool operator==(const char *c_str) const {
        return strlen(c_str) == FL && !memcmp(s_, c_str, FL);
    }
    bool operator==(const fix_string& rhs) const {
        return !memcmp(s_, rhs.s_, FL);
    }
    char operator[](size_t idx) const {
        return s_[idx];
    }
    fix_string& operator=(const fix_string& rhs) {
        initialize_from(rhs);
        return *this;
    }
    fix_string& operator=(const fix_string& rhs) volatile {
        initialize_from(rhs);
        return const_cast<fix_string&>(*this);
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
    void initialize_from(const std::string& str) {
        initialize_from(str);
    }
    void initialize_from(const fix_string& fstr) {
        memcpy(s_, fstr.s_, FL);
    }
    void initialize_from(const fix_string& fstr) volatile {
        memcpy(const_cast<char *>(s_), fstr.s_, FL);
    }

    char s_[FL];
};

struct ycsb_key {
    ycsb_key(uint64_t id) {
    	// no scan operations for ycsb,
    	// so byte swap is not required.
        w_id = id;
    }
    bool operator==(const ycsb_key& other) const {
        return w_id == other.w_id;
    }
    bool operator!=(const ycsb_key& other) const {
        return !(*this == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t w_id;
};

struct ycsb_value {
    fix_string<100>  col0;
    fix_string<100>  col1;
    fix_string<100>  col2;
    fix_string<100>  col3;
    fix_string<100>  col4;
    fix_string<100>  col5;
    fix_string<100>  col6;
    fix_string<100>  col7;
    fix_string<100>  col8;
    fix_string<100>  col9;
};




}; // namespace ycsb
