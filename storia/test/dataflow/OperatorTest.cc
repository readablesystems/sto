#include "dataflow/Operators.hh"

#include "Tests.hh"
#include "test/TestUtil.hh"

namespace storia {

namespace test {

namespace operators {

TEST_BEGIN(testCountSimple) {
    typedef storia::BlindWrite<int> update_type;
    auto counter = storia::Count<int>();

    ASSERT(!counter.produce()->value());

    counter.consume(update_type(41));

    ASSERT(counter.produce()->value() == 1);

    counter.consume(update_type(99));

    ASSERT(counter.produce()->value() == 2);

    // Do nothing

    ASSERT(counter.produce()->value() == 2);
}
TEST_END

TEST_BEGIN(testCountWithFunction) {
    typedef storia::BlindWrite<int> update_type;
    auto comparator = [] (const update_type& update) -> bool {
        return update.value() < 50;
    };
    auto counter = storia::Count<int, void>(comparator);

    ASSERT(!counter.produce()->value());

    counter.consume(update_type(41));

    ASSERT(counter.produce()->value() == 1);

    counter.consume(update_type(99));

    ASSERT(counter.produce()->value() == 1);
}
TEST_END

TEST_BEGIN(testCountWithPredicate) {
    typedef storia::BlindWrite<int> update_type;
    auto comparator = [] (const update_type& update) -> bool {
        return update.value() < 50;
    };
    auto predicate = storia::PredicateUtil::Make<update_type>(comparator);
    auto counter = storia::Count<int, void>(predicate);

    ASSERT(!counter.produce()->value());

    counter.consume(update_type(41));

    ASSERT(counter.produce()->value() == 1);

    counter.consume(update_type(99));

    ASSERT(counter.produce()->value() == 1);
}
TEST_END

TEST_BEGIN(testFilterSimpleFunction) {
    typedef storia::BlindWrite<int> update_type;
    auto comparator = [] (const update_type& update) -> bool {
        return update.value() > 0;
    };
    auto filter = storia::Filter<int>(comparator);

    filter.consume(update_type(-1));
    {
        auto next = filter.produce();
        ASSERT(!next);
    }

    filter.consume(update_type(-1));
    filter.consume(update_type(2));
    filter.consume(update_type(1));
    filter.consume(update_type(-2));
    {
        auto next = filter.produce();
        ASSERT(next->value() == 2);
    }
    {
        auto next = filter.produce();
        ASSERT(next->value() == 1);
    }
    {
        auto next = filter.produce();
        ASSERT(!next);
    }
}
TEST_END

TEST_BEGIN(testFilterSimplePredicate) {
    typedef storia::BlindWrite<int> update_type;
    auto comparator = [] (const update_type& update) -> bool {
        return update.value() > 0;
    };
    auto predicate = storia::PredicateUtil::Make<update_type>(comparator);
    auto filter = storia::Filter<int>(predicate);

    filter.consume(update_type(-1));
    {
        auto next = filter.produce();
        ASSERT(!next);
    }

    filter.consume(update_type(-1));
    filter.consume(update_type(2));
    filter.consume(update_type(1));
    filter.consume(update_type(-2));

    {
        auto next = filter.produce();
        ASSERT(next->value() == 2);
    }
    {
        auto next = filter.produce();
        ASSERT(next->value() == 1);
    }
    {
        auto next = filter.produce();
        ASSERT(!next);
    }
}
TEST_END

void testMain() {
    TestUtil::run_suite(
            "operators::Count",
            testCountSimple,
            testCountWithPredicate,
            testCountWithFunction);
    TestUtil::run_suite(
            "operators::Filter",
            testFilterSimplePredicate,
            testFilterSimpleFunction);
}

};  // namespace operators

};  // namespace test

};  // namespace storia
