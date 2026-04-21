// app/main.cpp — entry point and subcommand dispatcher.
//
// Subcommands:
//   babaiwt --test <file>...       — headless scenario runner (CI path)
//   babaiwt --edit [<file.level>]  — GUI editor (empty world if no file)
//   babaiwt --play <file.level>    — GUI in play-only mode (future: palette hidden)
//   babaiwt --bench <file> [N]     — phase timing benchmark (headless)
//   babaiwt -h / --help            — usage

#include "cli/test_runner.hpp"
#include "core/loader.hpp"
#include "gui_loop.hpp"
#include "sim/simulator.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <ctime>
#include <string>
#include <variant>
#include <vector>

namespace {

void usage() {
    std::printf(
        "babaiwt — Baba Is True simulator\n"
        "\n"
        "Usage:\n"
        "  babaiwt --test <path.test> [<path.test>...]       Run scenario tests\n"
        "  babaiwt --edit [--no-undo] [<path.level>]        Open GUI editor\n"
        "  babaiwt --play [--no-undo] <path.level>          Open GUI in play mode\n"
        "  babaiwt --bench <path.level> [N=500] [--no-log]  Phase timing benchmark\n"
        "\n"
        "Options:\n"
        "  --no-undo   Disable undo history (saves memory on large/long levels)\n"
        "  --no-log    Skip writing to logs/benchmarks/ (bench only)\n"
        "\n"
        "Exit status:\n"
        "  0 = all scenarios passed (or clean GUI exit)\n"
        "  1 = at least one scenario failed\n"
        "  2 = bad arguments\n"
    );
}

// Returns the short git hash of HEAD, or "unknown" if git is unavailable.
static std::string git_short_hash() {
    char buf[64] = {};
    FILE* fp = popen("git rev-parse --short HEAD 2>/dev/null", "r");
    if (!fp) return "unknown";
    if (std::fgets(buf, sizeof(buf), fp)) {
        for (char& c : buf) if (c == '\n') { c = '\0'; break; }
    }
    pclose(fp);
    return buf[0] ? buf : "unknown";
}

// Write bench log header + report to out.
static void write_log_entry(FILE* out, std::string const& level, int n_ticks,
                            std::string const& git_hash, std::string const& timestamp,
                            baba::sim::BenchReport const& report) {
    std::fprintf(out, "Benchmark Run\n");
    std::fprintf(out, "=============\n");
    std::fprintf(out, "Timestamp: %s\n", timestamp.c_str());
    std::fprintf(out, "Git:       %s\n", git_hash.c_str());
    std::fprintf(out, "Level:     %s\n", level.c_str());
    std::fprintf(out, "Ticks:     %d\n", n_ticks);
    report.print(out);
}

int run_bench(std::string const& path, int n_ticks, bool write_log) {
    // Accept both .level and .test files.
    baba::core::World world;
    bool ok = false;

    if (path.size() >= 5 && path.substr(path.size() - 5) == ".test") {
        auto loaded = baba::core::load_test_file(path);
        if (std::holds_alternative<baba::core::ParseError>(loaded)) {
            auto& e = std::get<baba::core::ParseError>(loaded);
            std::fprintf(stderr, "bench: load error %s:%d: %s\n",
                         e.path.c_str(), e.line, e.message.c_str());
            return 1;
        }
        world = std::move(std::get<baba::core::TestScenario>(loaded).setup.world);
        ok = true;
    } else {
        auto loaded = baba::core::load_level_file(path);
        if (std::holds_alternative<baba::core::ParseError>(loaded)) {
            auto& e = std::get<baba::core::ParseError>(loaded);
            std::fprintf(stderr, "bench: load error %s:%d: %s\n",
                         e.path.c_str(), e.line, e.message.c_str());
            return 1;
        }
        world = std::move(std::get<baba::core::LoadedLevel>(loaded).world);
        ok = true;
    }
    if (!ok) return 1;

    baba::sim::Simulator sim(std::move(world));

    // Warm-up: 10 ticks excluded from stats.
    for (int i = 0; i < 10; ++i)
        sim.step_forward(baba::core::Input::wait());
    sim.reset_bench();

    for (int i = 0; i < n_ticks; ++i)
        sim.step_forward(baba::core::Input::wait());

    auto const& report = sim.bench_report();
    std::printf("Level: %s\n", path.c_str());
    report.print();

    if (write_log) {
        std::time_t now = std::time(nullptr);
        struct std::tm* tm_info = std::localtime(&now);
        char ts_file[32], ts_human[32];
        std::strftime(ts_file,  sizeof(ts_file),  "%Y-%m-%d_%H-%M-%S", tm_info);
        std::strftime(ts_human, sizeof(ts_human), "%Y-%m-%d %H:%M:%S", tm_info);

        std::string git_hash = git_short_hash();

        // Derive a short name from the level path for the filename.
        std::string basename = path;
        auto slash = basename.rfind('/');
        if (slash != std::string::npos) basename = basename.substr(slash + 1);
        auto dot = basename.rfind('.');
        if (dot != std::string::npos) basename = basename.substr(0, dot);

        std::string log_dir  = "logs/benchmarks";
        std::string log_path = log_dir + "/" + ts_file + "_" + basename + ".txt";
        std::string latest   = log_dir + "/latest.txt";

        auto try_write = [&](std::string const& fpath) {
            FILE* fp = std::fopen(fpath.c_str(), "w");
            if (!fp) {
                std::fprintf(stderr, "bench: cannot open log %s\n", fpath.c_str());
                return;
            }
            write_log_entry(fp, path, n_ticks, git_hash, ts_human, report);
            std::fclose(fp);
        };

        try_write(log_path);
        try_write(latest);
        std::printf("Log: %s\n", log_path.c_str());
    }

    return 0;
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

    if (cmd == "--bench") {
        std::string level_path;
        int n_ticks = 500;
        bool write_log = true;
        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "--no-log") == 0) { write_log = false; }
            else if (level_path.empty())               { level_path = argv[i]; }
            else                                       { n_ticks = std::atoi(argv[i]); }
        }
        if (level_path.empty()) { usage(); return 2; }
        return run_bench(level_path, n_ticks, write_log);
    }

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
