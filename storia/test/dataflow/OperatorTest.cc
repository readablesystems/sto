#include "dataflow/Operators.hh"

#include "Tests.hh"
#include "test/TestUtil.hh"

namespace storia {

namespace test {

namespace operators {

TEST_BEGIN(testCountSimple) {
    auto counter = storia::Count<int>();

    ASSERT(!counter.process()->value);

    counter.receive(StateUpdate<int>::NewBlindWrite(41));

    ASSERT(counter.process()->value == 1);

    counter.receive(StateUpdate<int>::NewBlindWrite(99));

    ASSERT(counter.process()->value == 2);

    // Do nothing

    ASSERT(counter.process()->value == 2);
}
TEST_END

TEST_BEGIN(testCountWithPredicate) {
    auto comparator = [] (const StateUpdate<int>& update) -> bool {
        return update.value < 50;
    };
    auto predicate = storia::PredicateUtil::Make<StateUpdate<int>>(comparator);
    auto counter = storia::Count<int, void>(predicate);

    ASSERT(!counter.process()->value);

    counter.receive(StateUpdate<int>::NewBlindWrite(41));

    ASSERT(counter.process()->value == 1);

    counter.receive(StateUpdate<int>::NewBlindWrite(99));

    ASSERT(counter.process()->value == 1);
}
TEST_END

TEST_BEGIN(testFilterSimpleFunction) {
    auto comparator = [] (const StateUpdate<int>& update) -> bool {
        return update.value > 0;
    };
    auto filter = storia::Filter<int>(comparator);

    filter.receive(StateUpdate<int>::NewBlindWrite(-1));
    {
        auto next = filter.process();
        ASSERT(!next);
    }

    filter.receive(StateUpdate<int>::NewBlindWrite(-1));
    filter.receive(StateUpdate<int>::NewBlindWrite(2));
    filter.receive(StateUpdate<int>::NewBlindWrite(1));
    filter.receive(StateUpdate<int>::NewBlindWrite(-2));
    {
        auto next = filter.process();
        ASSERT(next->value == 2);
    }
    {
        auto next = filter.process();
        ASSERT(next->value == 1);
    }
    {
        auto next = filter.process();
        ASSERT(!next);
    }
}
TEST_END

TEST_BEGIN(testFilterSimplePredicate) {
    auto comparator = [] (const StateUpdate<int>& update) -> bool {
        return update.value > 0;
    };
    auto predicate = storia::PredicateUtil::Make<StateUpdate<int>>(comparator);
    auto filter = storia::Filter<int>(predicate);

    filter.receive(StateUpdate<int>::NewBlindWrite(-1));
    {
        auto next = filter.process();
        ASSERT(!next);
    }

    filter.receive(StateUpdate<int>::NewBlindWrite(-1));
    filter.receive(StateUpdate<int>::NewBlindWrite(2));
    filter.receive(StateUpdate<int>::NewBlindWrite(1));
    filter.receive(StateUpdate<int>::NewBlindWrite(-2));
    {
        auto next = filter.process();
        ASSERT(next->value == 2);
    }
    {
        auto next = filter.process();
        ASSERT(next->value == 1);
    }
    {
        auto next = filter.process();
        ASSERT(!next);
    }
}
TEST_END

void testMain() {
    TestUtil::run_suite(
            "operators::Count",
            testCountSimple,
            testCountWithPredicate);
    TestUtil::run_suite(
            "operators::Filter",
            testFilterSimplePredicate,
            testFilterSimpleFunction);
}

};  // namespace operators

};  // namespace test

};  // namespace storia
