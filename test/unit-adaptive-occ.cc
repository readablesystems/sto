#include "xxhash.h"
#include "Sto.hh"
#include "DB_params.hh"
#include "DB_index.hh"
#include "Adapter.hh"
#include "Accessor.hh"
#include "unit-adaptive-structs.hh"

using bench::split_version_helpers;
using bench::ordered_index;
using bench::unordered_index;
using bench::RowAccess;
using bench::access_t;

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
    void ResplitInflightTest();
    void StructOfTest();
};

template <bool Ordered>
void Tester<Ordered>::RunTests() {
    if constexpr (Ordered) {
        printf("Testing Adapter on Ordered Index:\n");
    } else {
        printf("Testing Adapter on Unordered Index:\n");
    }

    CounterTest();
    StructOfTest();
    ResplittingTest();
    ResplitInflightTest();
}

template <bool Ordered>
void Tester<Ordered>::CounterTest() {
    {
        adapter_type::ResetThread();

        value_type v1, v2;
        v2.data.value_ = {1, 2};
        assert(adapter_type::TGetRead(nc::data) == 0);
        assert(adapter_type::TGetWrite(nc::data) == 0);
        v1.data = v2.data;
        assert(adapter_type::TGetRead(nc::data) == 1);
        assert(adapter_type::TGetWrite(nc::data) == 1);
    }

    {
        adapter_type::ResetThread();

        value_type v;
        v.data.value_ = {14};
        assert(adapter_type::TGetRead(nc::data) == 0);
        assert(adapter_type::TGetWrite(nc::data) == 0);
        double x = v.data[0];
        assert(x == 14);
        assert(adapter_type::TGetRead(nc::data) == 1);
        assert(adapter_type::TGetWrite(nc::data) == 0);
        v.data(0, x);
        assert(adapter_type::TGetRead(nc::data) == 1);
        assert(adapter_type::TGetWrite(nc::data) == 1);
    }

    {
        adapter_type::ResetThread();

        value_type v;
        v.data.value_ = {3};
        assert(adapter_type::TGetRead(nc::data) == 0);
        assert(adapter_type::TGetWrite(nc::data) == 0);
        v.data(0)++;
        assert(adapter_type::TGetRead(nc::data) == 0);
        assert(adapter_type::TGetWrite(nc::data) == 1);
        double x = v.data[0];
        assert(x == 4);
        assert(adapter_type::TGetRead(nc::data) == 1);
        assert(adapter_type::TGetWrite(nc::data) == 1);
    }

    printf("Test pass: CounterTest\n");
}

template <bool Ordered>
void Tester<Ordered>::ResplittingTest() {
    value_type v;
    v.data = {1.2};
    v.label = "5.6";
    v.flagged = true;
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

template <bool Ordered>
void Tester<Ordered>::ResplitInflightTest() {
    adapter_type::ResetGlobal();
    adapter_type::RecomputeSplit();

    assert(adapter_type::CurrentSplit() == nc::COLCOUNT);

    adapter_type::ResetThread();

    {

    }

    printf("Test pass: ResplitInflightTest\n");
}

template <bool Ordered>
void Tester<Ordered>::StructOfTest() {
    {
        value_type v;
        assert(&v == &value_type::of(v.data));
        assert(&v == &value_type::of(v.label));
        assert(&v == &value_type::of(v.flagged));

        value_type v2;
        assert(&v2 != &value_type::of(v.data));
        assert(&v2 != &value_type::of(v.label));
        assert(&v2 != &value_type::of(v.flagged));

        assert(&v != &value_type::of(v2.data));
        assert(&v != &value_type::of(v2.label));
        assert(&v != &value_type::of(v2.flagged));

        assert(&v2 == &value_type::of(v2.data));
        assert(&v2 == &value_type::of(v2.label));
        assert(&v2 == &value_type::of(v2.flagged));
    }

    printf("Test pass: StructOfTest\n");
}

int main() {
    sto::AdapterConfig::Enable(sto::AdapterConfig::Global);

    {
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
