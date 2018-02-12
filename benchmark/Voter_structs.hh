#pragma once

#include <byteswap.h>

namespace voter {

struct contestant_key {
    contestant_key(uint16_t id) {
        c_id = bswap(id);
    }

    uint16_t c_id;
};

struct contestant_value {
    var_string<16> c_name;
};

struct phone_number {
    phone_number(const std::string& a, const std::string& n)
            : area_code(a), number(n) {}
    fix_string<3> area_code;
    fix_string<7> number;
};

struct area_code_key {
    fix_string<3> area_code;
};

struct area_code_value {
    fix_string<2> state;
};

struct votes_phone_key {
    votes_phone_key(const std::string& area_code, const std::string& number)
            : tel(area_code, number) {}

    phone_number tel;
};

struct votes_phone_value {
    uint64_t count;
};

struct votes_cn_state_key {
    votes_cn_state_key(uint16_t id, const std::string& st)
            : c_id(bswap(id)), state(st) {}

    uint16_t c_id;
    fix_string<2> state;
};

};