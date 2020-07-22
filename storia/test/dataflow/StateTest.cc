#include <string>

#include "dataflow/State.hh"

#include "Tests.hh"
#include "test/TestUtil.hh"

namespace storia {

namespace test {

namespace state {

TEST_BEGIN(testBlindWriteSimple) {
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

    entry_type entry(1, "test");
    auto update = storia::BlindWrite<entry_type>(entry);

    ASSERT(update.template key<int>() == 1);
    ASSERT(update.value().key() == 1);
    ASSERT(update.value().value() == "test");
    ASSERT(update.vp()->key() == 1);
    ASSERT(update.vp()->value() == "test");
}
TEST_END

TEST_BEGIN(testPartialBlindWriteSimple) {
    class entry_type : public storia::Entry {
    public:
        entry_type() : key_(0), value_("") {}
        entry_type(const int& key, const std::string& value)
            : key_(key), value_(value) {}

        void append(const std::string& value) {
            value_ += value;
        }

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

    auto update = storia::PartialBlindWrite<int, entry_type>(
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

TEST_BEGIN(testReadWriteSimple) {
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

    auto pred = PredicateUtil::Make<entry_type>(
            [] (const entry_type& entry) -> bool {
                return entry.value().length() < 6;
            });

    {
        entry_type entry(1, "test");
        auto update = storia::ReadWrite<entry_type>(pred, entry);

        ASSERT(update.template key<int>() == 1);
        ASSERT(update.value().key() == 1);
        ASSERT(update.value().value() == "test");
        ASSERT(update.predicate().eval(entry));
    }

    {
        entry_type entry(1, "testtest");
        auto update = storia::ReadWrite<entry_type>(pred, entry);

        ASSERT(update.template key<int>() == 1);
        ASSERT(update.value().key() == 1);
        ASSERT(update.value().value() == "testtest");
        ASSERT(!update.predicate().eval(entry));
    }
}
TEST_END

TEST_BEGIN(testTableBlindWriteSimple) {
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

    auto table = storia::Table<int, entry_type>();

    // Check that the key doesn't already exist
    {
        auto value = table.get(1);
        ASSERT(!value);
    }

    {
        entry_type entry(1, "test");
        auto update = storia::BlindWrite<entry_type>(entry);
        ASSERT(table.apply(update));
    }

    {
        auto value = table.get(1);
        ASSERT(value);
        ASSERT(value->value() == "test");
    }
}
TEST_END

TEST_BEGIN(testTablePartialBlindWriteSimple) {
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

    auto table = storia::Table<int, entry_type>();

    // Check that the key doesn't already exist
    {
        auto value = table.get(1);
        ASSERT(!value);
    }

    {
        entry_type entry(1, "test");
        auto update = storia::BlindWrite<entry_type>(entry);
        ASSERT(table.apply(update));
    }

    {
        auto value = table.get(1);
        ASSERT(value);
        ASSERT(value->value() == "test");
    }
}
TEST_END

TEST_BEGIN(testTablePartialReadWriteSimple) {
    class entry_type : public storia::Entry {
    public:
        entry_type() : key_(0), value_("") {}
        entry_type(const int& key, const std::string& value)
            : key_(key), value_(value) {}

        void append(const std::string& value) {
            value_ += value;
        }

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

    auto pred = PredicateUtil::Make<entry_type>(
            [] (const entry_type& entry) -> bool {
                return entry.value().length() < 6;
            });
    auto table = storia::Table<int, entry_type>();

    // Check that the key doesn't already exist
    {
        auto value = table.get(1);
        ASSERT(!value);
    }

    {
        entry_type entry(1, "test");
        table.nontrans_put(entry.key(), entry);
    }

    {
        auto value = table.get(1);
        ASSERT(value);
        ASSERT(value->value() == "test");
    }

    // Now attempt the update
    {
        auto update = storia::PartialReadWrite<int, entry_type>(
                1, pred, [] (entry_type& entry) -> entry_type& {
                    entry.append("test");
                    return entry;
                });
        ASSERT(table.apply(update));
    }

    {
        auto value = table.get(1);
        ASSERT(value);
        ASSERT(value->value() == "testtest");
    }
}
TEST_END

TEST_BEGIN(testTableReadWriteSimple) {
    class entry_type : public storia::Entry {
    public:
        entry_type() : key_(0), value_("") {}
        entry_type(const int& key, const std::string& value)
            : key_(key), value_(value) {}

        void append(const std::string& value) {
            value_ += value;
        }

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

    auto pred = PredicateUtil::Make<entry_type>(
            [] (const entry_type& entry) -> bool {
                return entry.value().length() < 6;
            });
    auto table = storia::Table<int, entry_type>();

    // Check that the key doesn't already exist
    {
        auto value = table.get(1);
        ASSERT(!value);
    }

    {
        entry_type entry(1, "test");
        table.nontrans_put(entry.key(), entry);
    }

    {
        auto value = table.get(1);
        ASSERT(value);
        ASSERT(value->value() == "test");
    }

    // Now attempt the update
    {
        entry_type entry(1, "testtest");
        auto update = storia::ReadWrite<entry_type>(pred, entry);
        ASSERT(table.apply(update));
    }

    {
        auto value = table.get(1);
        ASSERT(value);
        ASSERT(value->value() == "testtest");
    }
}
TEST_END

void testMain() {
    TestUtil::run_suite(
            "state::Update",
            testBlindWriteSimple,
            testReadWriteSimple,
            testPartialBlindWriteSimple);
    TestUtil::run_suite(
            "state::Table",
            testTableBlindWriteSimple,
            testTableReadWriteSimple,
            testTablePartialBlindWriteSimple,
            testTablePartialReadWriteSimple);
}

};  // namespace state

};  // namespace test

};  // namespace storia
