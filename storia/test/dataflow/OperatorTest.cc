#include "dataflow/Operators.hh"

#include "Tests.hh"
#include "test/TestUtil.hh"

namespace storia {

namespace test {

namespace operators {

TEST_BEGIN(testCountSimple) {
    auto pred = storia::PredicateUtil::Make<int, double>(
        [] (const int& x, const double& y) -> bool {
            return x > y;
        }, 0.f);

    ASSERT(pred.eval(1));
}
TEST_END

void testMain() {
    TestUtil::run_suite(
            "Count",
            testCountSimple);
}

};  // namespace operators

};  // namespace test

};  // namespace storia
