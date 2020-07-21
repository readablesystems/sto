#include "dataflow/Operators.hh"

#include "Tests.hh"
#include "test/TestUtil.hh"

namespace storia {

namespace test {

namespace operators {

TEST_BEGIN(testCountSimple) {
    typedef storia::BlindWrite<int> update_type;
    auto counter = storia::Count<int>();

    ASSERT(counter.consume(update_type(41))->value() == 1);

    ASSERT(counter.consume(update_type(99))->value() == 1);
}
TEST_END

TEST_BEGIN(testCountWithFunction) {
    typedef storia::BlindWrite<int> update_type;
    auto comparator = [] (const update_type& update) -> bool {
        return update.value() < 50;
    };
    auto counter = storia::Count<int, void>(comparator);

    ASSERT(counter.consume(update_type(41))->value() == 1);

    ASSERT(counter.consume(update_type(99))->value() == 0);
}
TEST_END

TEST_BEGIN(testCountWithPredicate) {
    typedef storia::BlindWrite<int> update_type;
    auto comparator = [] (const update_type& update) -> bool {
        return update.value() < 50;
    };
    auto predicate = storia::PredicateUtil::Make<update_type>(comparator);
    auto counter = storia::Count<int, void>(predicate);

    ASSERT(counter.consume(update_type(41))->value() == 1);

    ASSERT(counter.consume(update_type(99))->value() == 0);
}
TEST_END

TEST_BEGIN(testFilterSimpleFunction) {
    typedef storia::BlindWrite<int> update_type;
    auto comparator = [] (const update_type& update) -> bool {
        return update.value() > 0;
    };
    auto filter = storia::Filter<int>(comparator);

    ASSERT(!filter.consume(update_type(-1)));

    {
        auto update = filter.consume(update_type(2));
        ASSERT(update);
        ASSERT(update->value() == 2);
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

    ASSERT(!filter.consume(update_type(-1)));

    {
        auto update = filter.consume(update_type(2));
        ASSERT(update);
        ASSERT(update->value() == 2);
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
