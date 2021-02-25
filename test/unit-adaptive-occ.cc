#include "xxhash.h"
#include "Sto.hh"
#include "DB_params.hh"
#include "DB_index.hh"
#include "Adapter.hh"
#include "unit-adaptive-structs.hh"

using bench::split_version_helpers;
using bench::ordered_index;
using bench::unordered_index;
using bench::RowAccess;
using bench::access_t;

bool sto::AdapterConfig::Enabled = true;

INITIALIZE_ADAPTER(ADAPTER_OF(index_value));

template <bool Ordered>
class Tester {
public:
    void RunTests();

private:
    static constexpr size_t index_init_size = 1025;
    using key_type = typename std::conditional_t<
        Ordered, bench::masstree_key_adapter<index_key>, index_key>;
    using value_type = index_value;
    using index_type = typename std::conditional_t<
        Ordered,
        ordered_index<key_type, index_value, db_params::db_mvcc_params>,
        unordered_index<key_type, index_value, db_params::db_mvcc_params>>;
    using adapter_type = ADAPTER_OF(index_value);
    typedef index_value::NamedColumn nc;

    void CounterTest();
    void ResplittingTest();
};

template <bool Ordered>
void Tester<Ordered>::RunTests() {
    if constexpr (Ordered) {
        printf("Testing Adapter on Ordered Index:\n");
    } else {
        printf("Testing Adapter on Unordered Index:\n");
    }

    CounterTest();
    ResplittingTest();
}

template <bool Ordered>
void Tester<Ordered>::CounterTest() {
    {
        adapter_type::ResetThread();

        value_type v1, v2;
        assert(adapter_type::TGetRead(nc::data) == 0);
        assert(adapter_type::TGetWrite(nc::data) == 0);
        v1.data(0) = v2.data(0);
        assert(adapter_type::TGetRead(nc::data) == 1);
        assert(adapter_type::TGetWrite(nc::data) == 1);
    }

    {
        adapter_type::ResetThread();

        value_type v;
        assert(adapter_type::TGetRead(nc::data) == 0);
        assert(adapter_type::TGetWrite(nc::data) == 0);
        printf("TEST\n");
        double x = v.data(0);
        std::cout << adapter_type::TGetRead(nc::data) << ", " << adapter_type::TGetWrite(nc::data) << std::endl;
        assert(adapter_type::TGetRead(nc::data) == 1);
        assert(adapter_type::TGetWrite(nc::data) == 0);
    }

    printf("Test pass: CounterTest\n");
}

template <bool Ordered>
void Tester<Ordered>::ResplittingTest() {
    value_type v;
    v.data(0) = 1.2;
    v.data(1) = 3.4;
    v.label() = "5.6";
    v.flagged() = true;
    assert(v.split_of(nc::COLCOUNT + (-1)) == 0);

    value_type v2;
    value_type::resplit(v2, v, nc::label);
    assert(v.split_of(nc::COLCOUNT + (-1)) == 0);
    assert(v2.split_of(nc::data) == 0);
    assert(v2.split_of(nc::data + 1) == 0);
    assert(v2.split_of(nc::label) == 1);
    assert(v2.split_of(nc::flagged) == 1);

    printf("Test pass: ResplittingTest\n");
}

int main() {
    if constexpr (false) {
        Tester<true> tester;
        tester.RunTests();
    }
    {
        Tester<false> tester;
        tester.RunTests();
    }
    printf("ALL TESTS PASS, ignore errors after this.\n");
    auto advancer = std::thread(&Transaction::epoch_advancer, nullptr);
    Transaction::rcu_release_all(advancer, 48);
    return 0;
}
