#include "dataflow/Logics.hh"

#include "Tests.hh"
#include "test/TestUtil.hh"

namespace storia {

namespace test {

namespace logics {

TEST_BEGIN(testPredicateSimpleBinary) {
    auto pred = storia::PredicateUtil::Make<int, double>(
        [] (const int& x, const double& y) -> bool {
            return x > y;
        }, 0.f);

    ASSERT(pred.eval(1));
}
TEST_END

TEST_BEGIN(testPredicateSimpleUnary) {
    auto pred = storia::PredicateUtil::Make<int>(
        [] (const int& x) -> bool {
            return x > 0.f;
        });

    ASSERT(pred.eval(1));
}
TEST_END

void testMain() {
    TestUtil::run_suite(
            "Predicate",
            testPredicateSimpleUnary,
            testPredicateSimpleBinary);
}

};  // namespace logics

};  // namespace test

};  // namespace storia

