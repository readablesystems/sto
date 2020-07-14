#include <string>

#include "dataflow/State.hh"

#include "Tests.hh"
#include "test/TestUtil.hh"

namespace storia {

namespace test {

namespace state {

TEST_BEGIN(testBlindWriteUpdateSimple) {
    class entry_type : public storia::state::Entry {
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

    entry_type entry(1, "test");
    auto update = storia::state::BlindWriteUpdate<entry_type>(entry);

    ASSERT(update.template key<int>() == 1);
    ASSERT(update.value().key() == 1);
    ASSERT(update.value().value() == "test");
    ASSERT(update.vp()->key() == 1);
    ASSERT(update.vp()->value() == "test");
}
TEST_END

TEST_BEGIN(testDeltaWriteUpdateSimple) {
    class entry_type : public storia::state::Entry {
    public:
        entry_type() : key_(0), value_("") {}
        entry_type(const int& key, const std::string& value)
            : key_(key), value_(value) {}

        int key() const {
            return key_;
        }

        void append(const std::string& value) {
            value_ += value;
        }

        std::string value() const {
            return value_;
        }

    private:
        int key_;
        std::string value_;
    };

    auto update = storia::state::DeltaWriteUpdate<int, entry_type>(
        1,
        [] (entry_type& value) -> entry_type& {
            value.append("delta");
            return value;
        });
    auto updater = update.updater();

    entry_type entry(1, "test");
    updater.operate(entry);

    ASSERT(entry.key() == 1);
    ASSERT(entry.value() == "testdelta");
}
TEST_END

TEST_BEGIN(testTableSimple) {
    class entry_type : public storia::state::Entry {
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

    auto table = storia::state::Table<int, entry_type>();

    // Check that the key doesn't already exist
    {
        auto value = table.get(1);
        ASSERT(!value);
    }

    {
        entry_type entry(1, "test");
        auto update = storia::state::BlindWriteUpdate<entry_type>(entry);
        table.apply(update);
    }

    {
        auto value = table.get(1);
        ASSERT(value);
        ASSERT(value->value() == "test");
    }
}
TEST_END

void testMain() {
    TestUtil::run_suite(
            "state::Update",
            testBlindWriteUpdateSimple,
            testDeltaWriteUpdateSimple);
    TestUtil::run_suite(
            "state::Table",
            testTableSimple);
}

};  // namespace state

};  // namespace test

};  // namespace storia

