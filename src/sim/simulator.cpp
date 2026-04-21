#include "simulator.hpp"

#include <algorithm>
#include <cstring>

namespace baba::sim {

using namespace baba::core;

// Phase names in the same order as the Phase enum.
static constexpr const char* kPhaseNames[] = {
    "parse_initial",
    "apply_directional",
    "apply_input",
    "apply_auto_move",
    "apply_nudge",
    "apply_fear",
    "apply_shift",
    "apply_swap",
    "parse_post_move",
    "apply_follow",
    "apply_transform",
    "parse_post_transform",
    "apply_fall",
    "apply_destruct",
    "apply_has",
    "apply_make",
    "parse_post_destruct",
    "apply_play",
    "check_win",
};
static_assert(std::size(kPhaseNames) == static_cast<int>(Phase::Count),
              "kPhaseNames must have one entry per Phase");

Simulator::Simulator(World world, std::size_t undo_cap)
    : world_{std::move(world)}, undo_{undo_cap} {
    // Pre-fill phase names into bench_.
    for (int i = 0; i < static_cast<int>(Phase::Count); ++i)
        bench_.phases[i].name = kPhaseNames[i];
}

TickReport Simulator::step_forward(Input input) {
    last_report_ = apply_tick(world_, input);
    undo_.push(last_report_.changes);
    ++tick_;

    // Accumulate per-phase timings into bench_.
    auto const& t = last_report_.timings;
    for (int i = 0; i < static_cast<int>(Phase::Count); ++i) {
        std::int64_t ns = t.ns[i];
        auto& s = bench_.phases[i];
        s.min_ns    = std::min(s.min_ns, ns);
        s.max_ns    = std::max(s.max_ns, ns);
        s.total_ns += ns;
        ++s.count;
    }
    std::int64_t tot = t.total_ns;
    bench_.total.min_ns    = std::min(bench_.total.min_ns, tot);
    bench_.total.max_ns    = std::max(bench_.total.max_ns, tot);
    bench_.total.total_ns += tot;
    ++bench_.total.count;
    ++bench_.tick_count;

    return last_report_;
}

void Simulator::reset_bench() {
    bench_ = BenchReport{};
    for (int i = 0; i < static_cast<int>(Phase::Count); ++i)
        bench_.phases[i].name = kPhaseNames[i];
}

void BenchReport::print(std::FILE* out) const {
    if (tick_count == 0) {
        std::fprintf(out, "(no ticks recorded)\n");
        return;
    }

    // Build a sorted copy (descending by total_ns) for display.
    std::array<int, static_cast<int>(Phase::Count)> order{};
    for (int i = 0; i < static_cast<int>(Phase::Count); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return phases[a].total_ns > phases[b].total_ns;
    });

    std::fprintf(out, "\n=== Benchmark — %d ticks ===\n", tick_count);
    std::fprintf(out, "%-24s  %8s  %8s  %8s  %9s  %4s\n",
                 "Phase", "min(µs)", "mean(µs)", "max(µs)", "total(ms)", "%");
    std::fprintf(out, "%s\n", std::string(68, '-').c_str());

    for (int idx : order) {
        auto const& s = phases[idx];
        if (s.count == 0) continue;
        double pct = total.total_ns > 0
                     ? 100.0 * s.total_ns / total.total_ns
                     : 0.0;
        std::fprintf(out, "%-24s  %8.1f  %8.1f  %8.1f  %9.2f  %3.0f%%\n",
                     s.name, s.min_us(), s.mean_us(), s.max_us(), s.total_ms(), pct);
    }

    std::fprintf(out, "%s\n", std::string(68, '-').c_str());
    std::fprintf(out, "%-24s  %8.1f  %8.1f  %8.1f  %9.2f  100%%\n",
                 total.name, total.min_us(), total.mean_us(), total.max_us(), total.total_ms());
    std::fprintf(out, "\n");
}

bool Simulator::step_back() {
    auto maybe = undo_.pop();
    if (!maybe) return false;

    auto const& changes = *maybe;
    // Replay in reverse order, applying the inverse of each Change.
    for (auto it = changes.rbegin(); it != changes.rend(); ++it) {
        auto const& c = *it;
        switch (c.kind) {
            case ChangeKind::Move:
                world_.move(c.id, c.from_pos);
                break;
            case ChangeKind::Face:
                world_.face(c.id, c.from_facing);
                break;
            case ChangeKind::Spawn:
                // Inverse of spawn is destroy.
                world_.destroy(c.id);
                break;
            case ChangeKind::Destroy:
                // Inverse of destroy: restore original id so paired Move
                // records in the same tick can target it correctly.
                world_.respawn(c.id, c.obj_pos, c.obj_kind, c.obj_original_kind, c.obj_text, c.obj_facing);
                break;
            case ChangeKind::Retype:
                world_.retype(c.id, c.from_kind);
                break;
            case ChangeKind::FlipText:
                world_.flip_text(c.id);
                break;
        }
    }
    --tick_;
    return true;
}

}  // namespace baba::sim
