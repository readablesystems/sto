#include <iostream>
#include <vector>
#include <unordered_map>
#include <random>
#include <cstdlib>
#include <cassert>

#include "ListS.hh"

using list_type = List<int, int>;
using ref_type = std::unordered_map<int, int>;
using sid_type = TransactionTid::type;

void basic_test() {
    list_type list;
    sid_type sid1, sid2, sid3;
    TRANSACTION {
        list[1] = 2;
        list[2] = 3;
        list[3] = 1;
    } RETRY (false);

    sid1 = Sto::take_snapshot();

    TRANSACTION {
        list[2] = 2;
        list[1] = 1;
    } RETRY (false);

    TRANSACTION {
        assert(list[1] == 1);
        assert(list[2] == 2);
        assert(list[3] == 1);
    } RETRY (false);

    sid2 = Sto::take_snapshot();

    TRANSACTION {
        list[3] = 3;
        list[4] = 5;
    } RETRY (false);

    sid3 = Sto::take_snapshot();

    TRANSACTION {
        list[3] = 4;
        list[4] = 3;
    } RETRY (false);

    TRANSACTION {
        Sto::set_active_sid(sid1);
        auto v1 = list.trans_find(1).second;
        auto v2 = list.trans_find(2).second;
        auto v3 = list.trans_find(3).second;
        assert(v1 == 2);
        assert(v2 == 3);
        assert(v3 == 1);
    } RETRY (false);

    TRANSACTION {
        Sto::set_active_sid(sid2);
        auto v1 = list.trans_find(1).second;
        auto v2 = list.trans_find(2).second;
        auto v3 = list.trans_find(3).second;
        assert(v1 == 1);
        assert(v2 == 2);
        assert(v3 == 1);
        assert(!list.trans_find(4).first);
    } RETRY (false);

    TRANSACTION {
        Sto::set_active_sid(sid3);
        auto v1 = list.trans_find(1).second;
        auto v2 = list.trans_find(2).second;
        auto v3 = list.trans_find(3).second;
        auto v4 = list.trans_find(4).second;
        assert(v1 == 1);
        assert(v2 == 2);
        assert(v3 == 3);
        assert(v4 == 5);
    } RETRY (false);

    std::cout << "basic test pass." << std::endl;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    basic_test();
    return 0;
}
