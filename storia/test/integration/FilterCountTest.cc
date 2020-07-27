#include <string>
#include <thread>

#include "dataflow/Logics.hh"
#include "dataflow/Operators.hh"
#include "dataflow/State.hh"
#include "dataflow/Topology.hh"

#include "Tests.hh"
#include "test/TestUtil.hh"

namespace storia {

namespace test {

namespace filtercount {

TEST_BEGIN(testFilterCount) {
    class entry_type : public storia::Entry {
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

    typedef storia::BlindWrite<entry_type> update_type;
    typedef storia::nodes::CountNode<int, entry_type> count_node_type;
    typedef storia::nodes::FilterNode<int, entry_type> filter_node_type;

    count_node_type count_node;
    filter_node_type filter_node(
            [] (const update_type& value) -> bool {
                return value.value().value().length() < 6;
            });
    count_node.subscribe_to(
            &filter_node,
            &count_node_type::receive_update);

    {
        auto opt_v = count_node.get(1);
        ASSERT(!opt_v);
    }

    {
        entry_type entry(1, "test");
        auto update = typename filter_node_type::op_type::input_type(entry);
        filter_node.receive_update(std::shared_ptr<Update>(&update));
        auto opt_v = count_node.get(1);
        ASSERT(opt_v);
        ASSERT(opt_v->value().value() == 1);
    }
}
TEST_END

void testMain() {
    TestUtil::run_suite(
            "integration::FilterCount",
            testFilterCount);
}

};  // namespace filtercount

};  // namespace test

};  // namespace storia
