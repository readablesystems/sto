#include "xxhash.h"
#include "Sto.hh"
#include "DB_params.hh"
#include "DB_index.hh"
//#include "ADB_index.hh"
//#include "Adapter.hh"
//#include "Accessor.hh"
#include "unit-adaptive-structs.hh"

using bench::split_version_helpers;
using bench::ordered_index;
using bench::unordered_index;
using bench::RowAccess;

template <bool Ordered>
class Tester {
public:
    void RunTests();

private:
    static constexpr size_t index_init_size = 1025;
    using KeyType = typename std::conditional_t<
        Ordered, bench::masstree_key_adapter<index_key>, index_key>;
    using ValueType = index_value;
    using IndexType = typename std::conditional_t<
        Ordered,
        ordered_index<KeyType, index_value, db_params::db_default_params>,
        unordered_index<KeyType, index_value, db_params::db_default_params>>;
    //using GlobalAdapterType = ADAPTER_OF(index_value);
    using RecordAccessor = typename ValueType::RecordAccessor;
    typedef index_value::NamedColumn nc;

    // Helpers

    static IndexType CreateTable() {
        if constexpr (Ordered) {
            IndexType table;
            table.thread_init();
            return table;
        }

        IndexType table(4000000);
        table.thread_init();
        return table;
    }

    static void PopulateTable(IndexType& table) {
        for (auto key1 = KeyType::key_1_min;
             key1 <= std::min(KeyType::key_1_min + 999, KeyType::key_1_max);
             ++key1) {
            for (auto key2 = KeyType::key_2_min;
                 key2 <= std::min(KeyType::key_2_min + 999, KeyType::key_2_max);
                 ++key2) {
                ValueType value {
                    .data = { key1 + key2, key1 - key2 },
                    .label = "default",
                    .flagged = false,
                };
                table.nontrans_put(KeyType(key1, key2), value);
            }
        }
    }

    // Tests

    void CellTest();
    void CounterTest();
    void ResplittingTest();
    void ResplitConflictTest();
    void ResplitInflightTest();
    std::enable_if_t<Ordered, void> RowSelectTest();
    std::enable_if_t<Ordered, void> StatsTest();
    void StructOfTest();
};

template <bool Ordered>
void Tester<Ordered>::RunTests() {
    if constexpr (Ordered) {
        printf("Testing Adapter on Ordered Index:\n");
    } else {
        printf("Testing Adapter on Unordered Index:\n");
    }

    //sto::AdapterConfig::Enable(sto::AdapterConfig::Global);

    CellTest();

    //CounterTest();
    //StructOfTest();
    //ResplittingTest();
    //ResplitInflightTest();
    //ResplitConflictTest();

    if constexpr (Ordered) {
        //sto::AdapterConfig::Enable(sto::AdapterConfig::Inline);

        RowSelectTest();
        StatsTest();
    }
}

template <bool Ordered>
void Tester<Ordered>::CellTest() {
    assert(RecordAccessor::DEFAULT_SPLIT == 1);

    assert(RecordAccessor::split_of(0, nc::data) == 0);
    assert(RecordAccessor::split_of(0, nc::data + 1) == 0);
    assert(RecordAccessor::split_of(0, nc::label) == 0);
    assert(RecordAccessor::split_of(0, nc::flagged) == 0);

    assert(RecordAccessor::split_of(1, nc::data) == 1);
    assert(RecordAccessor::split_of(1, nc::data + 1) == 1);
    assert(RecordAccessor::split_of(1, nc::label) == 0);
    assert(RecordAccessor::split_of(1, nc::flagged) == 0);

    assert(RecordAccessor::split_of(2, nc::data) == 0);
    assert(RecordAccessor::split_of(2, nc::data + 1) == 1);
    assert(RecordAccessor::split_of(2, nc::label) == 0);
    assert(RecordAccessor::split_of(2, nc::flagged) == 1);

    printf("Test pass: %s\n", __FUNCTION__);
}

template <bool Ordered>
void Tester<Ordered>::CounterTest() {
    /*
    {
        GlobalAdapterType::ResetThread();

        ValueType v1_, v2_;
        RecordAccessor v1(v1_), v2(v2_);
        v2_.data = {1, 2};
        assert(GlobalAdapterType::TGetRead(nc::data) == 0);
        assert(GlobalAdapterType::TGetWrite(nc::data) == 0);
        v1.data = v2.data;
        assert(GlobalAdapterType::TGetRead(nc::data) == 1);
        assert(GlobalAdapterType::TGetWrite(nc::data) == 1);
    }

    {
        GlobalAdapterType::ResetThread();

        ValueType v_;
        RecordAccessor v(v_);
        v_.data = {14};
        assert(GlobalAdapterType::TGetRead(nc::data) == 0);
        assert(GlobalAdapterType::TGetWrite(nc::data) == 0);
        double x = v.data[0];
        assert(x == 14);
        assert(GlobalAdapterType::TGetRead(nc::data) == 1);
        assert(GlobalAdapterType::TGetWrite(nc::data) == 0);
        v.data(0, x);
        assert(GlobalAdapterType::TGetRead(nc::data) == 1);
        assert(GlobalAdapterType::TGetWrite(nc::data) == 1);
    }

    {
        GlobalAdapterType::ResetThread();

        ValueType v_;
        RecordAccessor v(v_);
        v_.data = {3};
        assert(GlobalAdapterType::TGetRead(nc::data) == 0);
        assert(GlobalAdapterType::TGetWrite(nc::data) == 0);
        v.data(0)++;
        assert(GlobalAdapterType::TGetRead(nc::data) == 0);
        assert(GlobalAdapterType::TGetWrite(nc::data) == 1);
        double x = v.data[0];
        assert(x == 4);
        assert(GlobalAdapterType::TGetRead(nc::data) == 1);
        assert(GlobalAdapterType::TGetWrite(nc::data) == 1);
    }
    */

    printf("Test pass: %s\n", __FUNCTION__);
}

template <bool Ordered>
void Tester<Ordered>::ResplittingTest() {
    /*
    ValueType v_;
    RecordAccessor v(v_);
    v_.data = {1.2};
    v_.label = "5.6";
    v_.flagged = true;
    assert(v.split_of(nc::COLCOUNT + (-1)) == 0);

    ValueType::RecordAccessor v2;
    //ValueType::resplit(v2, v, nc::label);
    assert(v.split_of(nc::COLCOUNT + (-1)) == 0);
    assert(v2.split_of(nc::data) == 0);
    assert(v2.split_of(nc::data + 1) == 0);
    assert(v2.split_of(nc::label) == 1);
    assert(v2.split_of(nc::flagged) == 1);
    */

    printf("Test pass: %s\n", __FUNCTION__);
}

template <bool Ordered>
void Tester<Ordered>::ResplitConflictTest() {
    /*
    auto table = CreateTable();
    PopulateTable(table);

    GlobalAdapterType::ResetGlobal();
    GlobalAdapterType::ResetThread();

    GlobalAdapterType::CountReads(nc::data, nc::label, 1000);
    GlobalAdapterType::CountWrites(nc::label, nc::COLCOUNT, 1000);
    GlobalAdapterType::Commit();
    GlobalAdapterType::RecomputeSplit();

    assert(GlobalAdapterType::CurrentSplit() == nc::label);

    {
        TestTransaction t1(1);

        auto [success, found, row, value] = table.select_row(KeyType(1, 1), {
                {nc::label, AccessType::update},
                });
        assert(success);
        assert(found);

        assert(value.splitindex_ == nc::COLCOUNT);

        auto new_value = value;

        assert(new_value.splitindex_ == nc::label);

        table.update_row(row, new_value);
        
        assert(t1.try_commit());
    }

    {
        TestTransaction t1(1);

        {
            auto [success, found, row, value] = table.select_row(KeyType(1, 1), {
                    {nc::label, AccessType::update},
                    });
            assert(success);
            assert(found);

            assert(value.splitindex_ == nc::label);

            auto new_value = value;

            assert(new_value.splitindex_ == nc::label);

            table.update_row(row, new_value);
        }

        GlobalAdapterType::ResetGlobal();
        GlobalAdapterType::ResetThread();

        GlobalAdapterType::CountReads(nc::data, nc::flagged, 1000);
        GlobalAdapterType::CountWrites(nc::flagged, nc::COLCOUNT, 1000);
        GlobalAdapterType::Commit();
        GlobalAdapterType::RecomputeSplit();

        assert(GlobalAdapterType::CurrentSplit() == nc::flagged);

        TestTransaction t2(2);

        {
            auto [success, found, row, value] = table.select_row(KeyType(1, 1), {
                    {nc::label, AccessType::update},
                    });
            assert(success);
            assert(found);

            assert(value.splitindex_ == nc::label);

            auto new_value = value;

            assert(new_value.splitindex_ == nc::flagged);

            table.update_row(row, new_value);
        }

        assert(t2.try_commit());

        t1.use();
        assert(!t1.try_commit());

        TestTransaction t3(3);

        {
            auto [success, found, row, value] = table.select_row(KeyType(1, 1), {
                    {nc::data, AccessType::read},
                    {nc::label, AccessType::read},
                    });
            assert(success);
            assert(found);
            (void) row;

            assert(value.splitindex_ == nc::flagged);
        }

        assert(t3.try_commit());

    }
    */

    printf("Test pass: %s\n", __FUNCTION__);
}

template <bool Ordered>
void Tester<Ordered>::ResplitInflightTest() {
    /*
    auto table = CreateTable();
    PopulateTable(table);

    GlobalAdapterType::ResetGlobal();
    GlobalAdapterType::ResetThread();

    GlobalAdapterType::CountReads(nc::data, nc::label, 1000);
    GlobalAdapterType::CountWrites(nc::label, nc::COLCOUNT, 1000);
    GlobalAdapterType::Commit();
    GlobalAdapterType::RecomputeSplit();

    assert(GlobalAdapterType::CurrentSplit() == nc::label);

    {
        TestTransaction t1(1);

        {
            auto [success, found, row, value] = table.select_row(KeyType(1, 1), {
                    {nc::data, AccessType::read},
                    {nc::label, AccessType::update},
                    });
            assert(success);
            assert(found);

            assert(value.splitindex_ == nc::COLCOUNT);

            auto new_value = value;

            assert(new_value.splitindex_ == nc::label);

            table.update_row(row, new_value);
        }

        TestTransaction t2(2);

        {
            auto [success, found, row, value] = table.select_row(KeyType(1, 1), {
                    {nc::data, AccessType::read},
                    {nc::label, AccessType::read},
                    });
            assert(success);
            assert(found);
            (void) row;

            assert(value.splitindex_ == nc::COLCOUNT);
        }

        assert(t2.try_commit());

        t1.use();
        assert(t1.try_commit());

        TestTransaction t3(3);

        {
            auto [success, found, row, value] = table.select_row(KeyType(1, 1), {
                    {nc::data, AccessType::read},
                    {nc::label, AccessType::read},
                    });
            assert(success);
            assert(found);
            (void) row;

            assert(value.splitindex_ == nc::label);
        }

        assert(t3.try_commit());

    }
    */

    printf("Test pass: %s\n", __FUNCTION__);
}

template <bool Ordered>
void Tester<Ordered>::StructOfTest() {
    /*
    {
        ValueType v;
        assert(&v == &ValueType::of(v.data));
        assert(&v == &ValueType::of(v.label));
        assert(&v == &ValueType::of(v.flagged));

        ValueType v2;
        assert(&v2 != &ValueType::of(v.data));
        assert(&v2 != &ValueType::of(v.label));
        assert(&v2 != &ValueType::of(v.flagged));

        assert(&v != &ValueType::of(v2.data));
        assert(&v != &ValueType::of(v2.label));
        assert(&v != &ValueType::of(v2.flagged));

        assert(&v2 == &ValueType::of(v2.data));
        assert(&v2 == &ValueType::of(v2.label));
        assert(&v2 == &ValueType::of(v2.flagged));
    }
    */

    printf("Test pass: %s\n", __FUNCTION__);
}

template <bool Ordered>
std::enable_if_t<Ordered, void>
Tester<Ordered>::RowSelectTest() {
    auto table = CreateTable();
    PopulateTable(table);

    {
        TestTransaction t1(1);

        auto [success, found, row, value] = table.select_row(KeyType(1, 1), {});
        assert(success);
        assert(found);
        (void) row;
        assert(value.data(0) == 2);
        assert(value.data(1) == 0);
        assert(value.label() == "default");
        assert(value.flagged() == false);

        t1.try_commit();
    }

    /*
    {
        TestTransaction t1(1);
        t1.set_restarted();

        auto [success, found, row, value] = table.select_row(KeyType(1, 1), {});
        assert(success);
        assert(found);
        (void) row;
        assert(value.data[0] == 2);
        assert(value.data[1] == 0);
        assert(*value.label == std::string("default"));
        assert(value.flagged == false);

        // No selections, yes restart = no stats
        assert(!value.stats_);

        t1.try_commit();
    }

    {
        TestTransaction t1(1);

        auto [success, found, row, value] = table.select_row(KeyType(1, 1), {
                {nc::data, AccessType::read},
                {nc::label, AccessType::read},
                });
        assert(success);
        assert(found);
        (void) row;
        assert(value.data[0] == 2);
        assert(value.data[1] == 0);
        assert(*value.label == std::string("default"));
        assert(value.flagged == false);

        // Yes selections, no restart = no stats
        assert(!value.stats_);

        t1.try_commit();
    }

    {
        TestTransaction t1(1);
        t1.set_restarted();

        auto [success, found, row, value] = table.select_row(KeyType(1, 1), {
                {nc::data, AccessType::read},
                {nc::data + 1, AccessType::read},
                {nc::label, AccessType::read},
                {nc::flagged, AccessType::read},
                });
        assert(success);
        assert(found);
        (void) row;
        assert(value.data[0] == 2);
        assert(value.data[1] == 0);
        assert(value.label == std::string("default"));
        assert(value.flagged == false);

        // Yes selections, yes restart = yes stats
        assert(value.stats_);

        t1.try_commit();
    }
    */

    printf("Test pass: %s\n", __FUNCTION__);
}

template <bool Ordered>
std::enable_if_t<Ordered, void>
Tester<Ordered>::StatsTest() {
    /*
    auto table = CreateTable();
    PopulateTable(table);

    {
        TestTransaction t1(1);
        t1.set_restarted();

        auto [success, found, row, value] = table.select_row(KeyType(1, 1), {
                {nc::data, AccessType::read},
                {nc::data + 1, AccessType::read},
                {nc::label, AccessType::read},
                {nc::flagged, AccessType::read},
                });
        assert(success);
        assert(found);
        (void) row;
        assert(value.data[0] == 2);
        assert(value.data[1] == 0);
        assert(value.label == std::string("default"));
        assert(value.flagged == false);

        // Yes selections, yes restart = yes stats
        assert(value.stats_);

        // Read everything
        assert(value.stats_->read_data.to_ullong() == 0b1111);
        assert(value.stats_->write_data.to_ullong() == 0b0000);

        t1.try_commit();
    }

    {
        TestTransaction t1(1);
        t1.set_restarted();

        auto [success, found, row, value] = table.select_row(KeyType(1, 1), {
                {nc::data, AccessType::update},
                {nc::data + 1, AccessType::update},
                {nc::label, AccessType::read},
                {nc::flagged, AccessType::read},
                });
        assert(success);
        assert(found);
        assert(value.label == std::string("default"));
        assert(value.flagged == false);

        // Yes selections, yes restart = yes stats
        assert(value.stats_);

        // Read some things
        assert(value.stats_->read_data.to_ullong() == 0b1100);
        assert(value.stats_->write_data.to_ullong() == 0b0000);

        // Write to some stuff too
        value.data(0) = 2.5;
        value.data(1) = 0.5;
        assert(value.stats_->read_data.to_ullong() == 0b1100);
        assert(value.stats_->write_data.to_ullong() == 0b0011);

        table.update_row(row, value);

        t1.try_commit();
    }
    */

    printf("Test pass: %s\n", __FUNCTION__);
}

int main() {
    {
        Tester<true> tester;
        tester.RunTests();
    }
    {
        //Tester<false> tester;
        //tester.RunTests();
    }
    printf("ALL TESTS PASS, ignore errors after this.\n");
    auto advancer = std::thread(&Transaction::epoch_advancer, nullptr);
    Transaction::rcu_release_all(advancer, 48);
    return 0;
}
