// Tiny assertion harness for unit tests. Avoids pulling in GoogleTest for
// Phase 1; Phase 2 may swap this for gtest if desirable.
//
// Usage:
//   TEST(Suite, Name) { CHECK(cond); CHECK_EQ(a, b); }
//   int main() { return baba::testing::run_all(); }
#pragma once

#include <cstddef>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace baba::testing {

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(char const* suite, char const* name, std::function<void()> fn) {
        registry().push_back({suite, name, std::move(fn)});
    }
};

struct Failure {
    std::string where;
    std::string message;
};

inline std::vector<Failure>& current_failures() {
    static thread_local std::vector<Failure> f;
    return f;
}

inline void report(std::string where, std::string msg) {
    current_failures().push_back({std::move(where), std::move(msg)});
}

inline int run_all() {
    int passed = 0, failed = 0;
    for (auto const& tc : registry()) {
        current_failures().clear();
        try {
            tc.fn();
        } catch (std::exception const& e) {
            report("(exception)", std::string{"threw: "} + e.what());
        } catch (...) {
            report("(exception)", "threw non-std exception");
        }
        if (current_failures().empty()) {
            ++passed;
            std::cout << "PASS  " << tc.suite << "." << tc.name << "\n";
        } else {
            ++failed;
            std::cout << "FAIL  " << tc.suite << "." << tc.name << "\n";
            for (auto const& f : current_failures()) {
                std::cout << "      " << f.where << ": " << f.message << "\n";
            }
        }
    }
    std::cout << "\n" << passed << " passed, " << failed << " failed ("
              << registry().size() << " total)\n";
    return failed == 0 ? 0 : 1;
}

}  // namespace baba::testing

#define BABA_CONCAT_INNER(a, b) a##b
#define BABA_CONCAT(a, b) BABA_CONCAT_INNER(a, b)

#define TEST(suite, name)                                                       \
    static void BABA_CONCAT(suite##_##name##_, __LINE__)();                     \
    static ::baba::testing::Registrar BABA_CONCAT(suite##_##name##_reg_,        \
                                                  __LINE__){                    \
        #suite, #name, BABA_CONCAT(suite##_##name##_, __LINE__)};               \
    static void BABA_CONCAT(suite##_##name##_, __LINE__)()

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::ostringstream _oss;                                            \
            _oss << __FILE__ << ":" << __LINE__;                                \
            ::baba::testing::report(_oss.str(), "CHECK(" #cond ") failed");     \
        }                                                                       \
    } while (0)

#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        auto const& _av = (a);                                                  \
        auto const& _bv = (b);                                                  \
        if (!(_av == _bv)) {                                                    \
            std::ostringstream _oss;                                            \
            _oss << __FILE__ << ":" << __LINE__;                                \
            std::ostringstream _msg;                                            \
            _msg << "CHECK_EQ(" #a ", " #b ") failed: "                         \
                 << _av << " != " << _bv;                                       \
            ::baba::testing::report(_oss.str(), _msg.str());                    \
        }                                                                       \
    } while (0)

#define CHECK_NE(a, b)                                                          \
    do {                                                                        \
        auto const& _av = (a);                                                  \
        auto const& _bv = (b);                                                  \
        if (_av == _bv) {                                                       \
            std::ostringstream _oss;                                            \
            _oss << __FILE__ << ":" << __LINE__;                                \
            ::baba::testing::report(_oss.str(),                                 \
                                    "CHECK_NE(" #a ", " #b ") failed");         \
        }                                                                       \
    } while (0)

#define CHECK_TRUE(cond)  CHECK((cond))
#define CHECK_FALSE(cond) CHECK(!(cond))
