#pragma once

#include <string>
#include "DB_structs.hh"
#include "str.hh"

namespace voter {

using namespace bench;

// Helper struct: phone number representation as fix_string

struct phone_number_str {
    std::string area_code;
    std::string number;
};

struct phone_number {
    fix_string<3> area_code;
    fix_string<7> number;

    phone_number() = default;
    explicit phone_number(const phone_number_str& phs)
            : area_code(phs.area_code), number(phs.number) {}
};


// Tables

// Table: Contestant

struct contestant_key_bare {
    int32_t number;

    explicit contestant_key_bare(int32_t id) : number(bswap(id)) {}
};

typedef masstree_key_adapter<contestant_key_bare> contestant_key;

struct contestant_row {
    enum class NamedColumn : int {name = 0};
    var_string<50> name;
};

// Table: Area code to state

struct area_code_state_key_bare {
    fix_string<3> area_code;

    explicit area_code_state_key_bare(const std::string& a)
            : area_code(a) {}
};

typedef masstree_key_adapter<area_code_state_key_bare> area_code_state_key;

struct area_code_state_row {
    enum class NamedColumn : int {state = 0};

    fix_string<2> state;
};

// Table: Votes

struct votes_key_bare {
    int32_t vote_id;

    explicit votes_key_bare(int32_t id) : vote_id(bswap(id)) {}
};

typedef masstree_key_adapter<votes_key_bare> votes_key;

struct votes_row {
    enum class NamedColumn : int { tel = 0,
                                   state,
                                   contestant_number,
                                   created };
    phone_number   tel;
    fix_string<2>  state;
    int32_t        contestant_number;
    var_string<14> created;
};

// Views

// View: Votes by phone number

struct v_votes_phone_key_bare {
    phone_number tel;

    explicit v_votes_phone_key_bare(const phone_number_str& ph)
            : tel(ph) {}

    friend masstree_key_adapter<v_votes_phone_key_bare>;
private:
    v_votes_phone_key_bare() = default;
};

typedef masstree_key_adapter<v_votes_phone_key_bare> v_votes_phone_key;

struct v_votes_phone_row {
    enum class NamedColumn : int {count = 0};
    int64_t count;
};

// View: Votes by contestant number and state

struct __attribute__((packed)) v_votes_id_state_key_bare {
    int32_t number;
    fix_string<2> state;

    explicit v_votes_id_state_key_bare(int32_t id, const fix_string<2>& st)
            : number(bswap(id)), state(st) {}
    friend masstree_key_adapter<v_votes_id_state_key_bare>;
private:
    v_votes_id_state_key_bare() = default;
};

typedef masstree_key_adapter<v_votes_id_state_key_bare> v_votes_id_state_key;

struct v_votes_id_state_row {
    enum class NamedColumn : int {count = 0};
    int64_t count;
};

};