// app/main.cpp — entry point.
//
// Phase 1 surface: `babaiwt --test <file>...`  runs each .test file and
// returns non-zero on any failure. The GUI/headless split lives here once
// raylib lands in Phase 2.
#include "../cli/test_runner.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

void usage() {
    std::printf(
        "babaiwt — Baba Is True simulator (Phase 1)\n"
        "\n"
        "Usage:\n"
        "  babaiwt --test <path.test> [<path.test>...]\n"
        "\n"
        "Exit status:\n"
        "  0 = all scenarios passed\n"
        "  1 = at least one scenario failed\n"
        "  2 = bad arguments\n"
    );
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 2; }
    std::vector<std::string> test_paths;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--test") == 0) continue;
        if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage(); return 0;
        }
        test_paths.emplace_back(argv[i]);
    }
    if (test_paths.empty()) { usage(); return 2; }

    int total = 0, passed = 0;
    for (auto const& p : test_paths) {
        auto r = baba::cli::run_test_file(p);
        baba::cli::print_summary(r);
        if (!r.passed()) baba::cli::print_detail(r);
        ++total;
        if (r.passed()) ++passed;
    }
    std::printf("\n%d/%d scenarios passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
