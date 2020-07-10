#include <string>

#include "dataflow/StateTable.hh"

#include "Tests.hh"
#include "test/TestUtil.hh"

namespace storia {

namespace test {

namespace state {

TEST_BEGIN(testStateTableSimple) {
    class entry_type : public StateEntry {
    public:
        entry_type() : key_(0), value_("") {}
        entry_type(const int& key, const std::string& value)
            : key_(key), value_(value) {}

        int key() const {
            return key_;
        }

        std::string value() const {
            return value_;
        }

    private:
        int key_;
        std::string value_;
    };

    auto table = StateTable<int, entry_type>();

    // Check that the key doesn't already exist
    {
        auto value = table.get(1);
        ASSERT(!value);
    }

    {
        entry_type entry(1, "test");
        table.apply(StateUpdate<entry_type>::NewBlindWrite(entry));
    }

    {
        auto value = table.get(1);
        ASSERT(value->value() == "test");
    }
}
TEST_END

void testMain() {
    TestUtil::run_suite(
            "state::StateTable",
            testStateTableSimple);
}

};  // namespace state

};  // namespace test

};  // namespace storia

