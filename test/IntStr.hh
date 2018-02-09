#pragma once
#include "str.hh"

struct IntStr {
    char s_[16];
    lcdf::Str str_;
    IntStr(int i) {
        int n = snprintf(s_, sizeof(s_), "%d", i);
        str_.assign(s_, n);
    }
    lcdf::Str& str() {
        return str_;
    }
    operator lcdf::Str&() {
        return str_;
    }
};
