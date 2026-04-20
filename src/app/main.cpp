// app/main.cpp — entry point and subcommand dispatcher.
//
// Subcommands:
//   babaiwt --test <file>...       — headless scenario runner (CI path)
//   babaiwt --edit [<file.level>]  — GUI editor (empty world if no file)
//   babaiwt --play <file.level>    — GUI in play-only mode (future: palette hidden)
//   babaiwt -h / --help            — usage

#include "cli/test_runner.hpp"
#include "gui_loop.hpp"

#include <cstdio>
#include <cstring>
#include <cstddef>
#include <string>
#include <vector>

namespace {

void usage() {
    std::printf(
        "babaiwt — Baba Is True simulator\n"
        "\n"
        "Usage:\n"
        "  babaiwt --test <path.test> [<path.test>...]  Run scenario tests\n"
        "  babaiwt --edit [--no-undo] [<path.level>]   Open GUI editor\n"
        "  babaiwt --play [--no-undo] <path.level>     Open GUI in play mode\n"
        "\n"
        "Options:\n"
        "  --no-undo   Disable undo history (saves memory on large/long levels)\n"
        "\n"
        "Exit status:\n"
        "  0 = all scenarios passed (or clean GUI exit)\n"
        "  1 = at least one scenario failed\n"
        "  2 = bad arguments\n"
    );
}

int run_tests(std::vector<std::string> const& paths) {
    int total = 0, passed = 0;
    for (auto const& p : paths) {
        auto r = baba::cli::run_test_file(p);
        baba::cli::print_summary(r);
        if (!r.passed()) baba::cli::print_detail(r);
        ++total;
        if (r.passed()) ++passed;
    }
    std::printf("\n%d/%d scenarios passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 2; }

    std::string cmd = argv[1];

    if (cmd == "-h" || cmd == "--help") { usage(); return 0; }

    if (cmd == "--edit" || cmd == "--play") {
        std::string level_path;
        std::size_t undo_cap = 10'000;
        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "--no-undo") == 0) { undo_cap = 0; }
            else if (level_path.empty())                { level_path = argv[i]; }
        }
        return baba::app::run_gui(level_path, undo_cap);
    }

    // Legacy / CI path: --test or bare file paths.
    std::vector<std::string> test_paths;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--test") == 0) continue;
        test_paths.emplace_back(argv[i]);
    }
    if (test_paths.empty()) { usage(); return 2; }
    return run_tests(test_paths);
}
