#pragma once

#include "undo_buffer.hpp"
#include "core/ruleset.hpp"
#include "core/tick.hpp"
#include "core/world.hpp"

#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>

namespace baba::sim {

// Per-phase accumulated statistics across multiple ticks.
struct PhaseStats {
    const char*  name{""};
    std::int64_t min_ns{INT64_MAX};
    std::int64_t max_ns{0};
    std::int64_t total_ns{0};
    int          count{0};

    double min_us()  const { return min_ns  / 1000.0; }
    double max_us()  const { return max_ns  / 1000.0; }
    double mean_us() const { return count ? total_ns / 1000.0 / count : 0.0; }
    double total_ms() const { return total_ns / 1'000'000.0; }
};

// Snapshot of accumulated benchmark data, with a pretty-printer.
struct BenchReport {
    std::array<PhaseStats, static_cast<int>(core::Phase::Count)> phases{};
    PhaseStats total{"TOTAL"};
    int        tick_count{0};

    void print(std::FILE* out = stdout) const;
};

// Runtime wrapper around the core engine: owns a World, UndoBuffer, and tick counter.
// step_forward applies one tick and pushes the Change list to the undo buffer.
// step_back pops the most recent tick's changes and applies them in reverse.
class Simulator {
public:
    explicit Simulator(core::World world, std::size_t undo_cap = 10'000);

    core::TickReport step_forward(core::Input input);

    // Undo one tick. Returns false if nothing to undo.
    bool step_back();

    core::World const& world() const { return world_; }
    core::World&       world()       { return world_; }

    core::RuleSet current_rules() const { return core::RuleSet::parse(world_); }

    int  tick_count() const { return tick_; }
    bool last_won()   const { return last_report_.won; }

    // Benchmark accumulator. reset_bench() clears stats; bench_report() returns
    // a snapshot. step_forward() always accumulates timings from each tick.
    void        reset_bench();
    BenchReport bench_report() const { return bench_; }

private:
    core::World      world_;
    UndoBuffer       undo_;
    core::TickReport last_report_{};
    int              tick_{0};
    BenchReport      bench_{};
};

}  // namespace baba::sim
