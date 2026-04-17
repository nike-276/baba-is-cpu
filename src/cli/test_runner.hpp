// cli/test_runner.hpp — runs a .test file and reports pass/fail.
#pragma once

#include <string>
#include <vector>

namespace baba::cli {

struct TestFailure {
    int         line{0};
    std::string message;   // includes the assertion text
};

struct TestResult {
    std::string               path;
    std::string               scenario_name;
    int                       ticks_run{0};
    int                       assertions_total{0};
    int                       assertions_failed{0};
    bool                      load_error{false};
    std::string               load_error_text;
    std::vector<TestFailure>  failures;

    bool passed() const { return !load_error && assertions_failed == 0; }
};

// Run a single .test file. Never throws; surface errors via `load_error`.
TestResult run_test_file(std::string const& path);

// Print a one-line summary to stdout: PASS/FAIL with counts.
void print_summary(TestResult const& r);

// Print the long form (with each failed assertion + line number).
void print_detail(TestResult const& r);

}  // namespace baba::cli
