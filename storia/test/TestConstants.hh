#pragma once

#define TEST_BEGIN(name) \
    TestUtil::Result name() { \
        const auto FILENAME = __FILE__; \
        const auto FUNCTION = __func__; \
        try { \
            return ([FILENAME, FUNCTION] () -> TestUtil::Result {

#define TEST_END \
        return TestUtil::Result( \
                TestUtil::Status::SUCCESS, FILENAME, FUNCTION, __LINE__, ""); \
        })(); \
    } catch (const std::exception& e) { \
        return TestUtil::Result( \
                TestUtil::Status::ERROR, FILENAME, FUNCTION, __LINE__, e.what()); \
    } catch (...) { \
        return TestUtil::Result( \
                TestUtil::Status::ERROR, FILENAME, FUNCTION, __LINE__, "Unknown"); \
    } \
}

#define ASSERT_INNER(expr, message, ...) { \
    if (!(expr)) { \
        return TestUtil::Result( \
                TestUtil::Status::FAILURE, FILENAME, FUNCTION, __LINE__, message); \
    } \
}

#define ASSERT(...) ASSERT_INNER(__VA_ARGS__, "")
