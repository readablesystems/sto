#pragma once

#include <functional>
#include <iostream>
#include <string>

#include "TestConstants.hh"

namespace storia {

namespace test {

class TestUtil {
public:
    class Result {
    public:
        enum Status {
            SUCCESS=0, FAILURE, ERROR
        };

        Result(
                const Status status, const std::string& file,
                const std::string& test, const size_t line,
                const std::string& message)
            : file(file), line(line), message(message), status(status),
              test(test) {}

        char as_char() const {
            switch (status) {
                case SUCCESS:
                    return '.';
                case FAILURE:
                    return 'x';
                case ERROR:
                    return 'E';
            }
            return '?';
        }

        const std::string file;
        const size_t line;
        const std::string message;
        const Status status;
        const std::string test;
    };

    typedef std::function<Result()> test_type;
    typedef Result::Status Status;

    template <typename... Args>
    static void run_suite(const std::string& name, const Args&&... tests) {
        std::cout << "  " << name << ": " << std::flush;
        std::vector<Result> nonsuccesses;
        run_test(nonsuccesses, std::forward<Args>(tests)...);
    }

    template <typename... Args>
    static void run_test(
            std::vector<Result>& nonsuccesses, const test_type& test,
            const Args&&... tests) {
        auto result = test();
        switch (result.status) {
            case Result::Status::ERROR:
            case Result::Status::FAILURE:
                nonsuccesses.push_back(result);
                break;
            case Result::Status::SUCCESS:
                // no-op
                break;
        }
        std::cout << result.as_char() << std::flush;
        run_test(nonsuccesses, std::forward<Args>(tests)...);
    }

    static void run_test(std::vector<Result>& nonsuccesses) {
        if (nonsuccesses.size()) {
            std::cout << "\rx" << std::endl;
            for (const auto& result : nonsuccesses) {
                switch (result.status) {
                    case Result::Status::ERROR:
                        std::cout
                            << "  Error:  " << result.file << ":"
                            << result.test;
                        break;
                    case Result::Status::FAILURE:
                        std::cout
                            << "  Failed: " << result.file << ":"
                            << result.test << " on line " << result.line;
                        break;
                    case Result::Status::SUCCESS:
                        // no-op
                        break;
                }
                if (result.message.length()) {
                    std::cout << " " << result.message;
                }
                std::cout << std::endl;
            }
        } else {
            std::cout << "\r✓" << std::endl;
        }
    }
};

};  // namespace test

};  // namespace storia
