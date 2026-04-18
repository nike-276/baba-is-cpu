#include "test_runner.hpp"

#include "../core/loader.hpp"
#include "../core/tick.hpp"
#include "../sim/simulator.hpp"

#include <cstdio>
#include <sstream>
#include <string>

namespace baba::cli {

namespace {

using namespace baba::core;

std::string assertion_text(Assertion const& a) {
    std::ostringstream os;
    auto kn = kind_name(a.kind_arg);
    switch (a.kind) {
        case AssertionKind::At:        os << "at "         << a.pos.x << " " << a.pos.y << " " << kn; break;
        case AssertionKind::NotAt:     os << "not_at "     << a.pos.x << " " << a.pos.y << " " << kn; break;
        case AssertionKind::TextAt:    os << "text_at "    << a.pos.x << " " << a.pos.y << " " << kn; break;
        case AssertionKind::Count:     os << "count "      << kn      << " " << a.int_arg;            break;
        case AssertionKind::TextCount: os << "text_count " << kn      << " " << a.int_arg;            break;
        case AssertionKind::Won:       os << "won";                                                  break;
        case AssertionKind::NotWon:    os << "not_won";                                              break;
        case AssertionKind::Tick:      os << "tick "       << a.int_arg;                              break;
    }
    return os.str();
}

bool eval_assertion(Assertion const& a, World const& world, int forward_ticks, bool won, std::string& detail) {
    auto cell_has_kind = [&](Coord c, Kind k, bool want_text) -> bool {
        for (ObjectId id : world.at(c)) {
            Object const* o = world.get(id);
            if (!o) continue;
            if (o->text == want_text && o->kind == k) return true;
        }
        return false;
    };
    auto count_kind = [&](Kind k, bool want_text) -> int {
        int n = 0;
        for (ObjectId id : world.all_ids()) {
            Object const* o = world.get(id);
            if (!o) continue;
            if (o->text == want_text && o->kind == k) ++n;
        }
        return n;
    };

    switch (a.kind) {
        case AssertionKind::At:
            if (cell_has_kind(a.pos, a.kind_arg, /*text=*/false)) return true;
            detail = "no non-text object of that kind at that tile"; return false;
        case AssertionKind::NotAt:
            if (!cell_has_kind(a.pos, a.kind_arg, /*text=*/false)) return true;
            detail = "object exists at that tile"; return false;
        case AssertionKind::TextAt:
            if (cell_has_kind(a.pos, a.kind_arg, /*text=*/true)) return true;
            detail = "no text of that kind at that tile"; return false;
        case AssertionKind::Count: {
            int n = count_kind(a.kind_arg, false);
            if (n == a.int_arg) return true;
            detail = "actual count = " + std::to_string(n); return false;
        }
        case AssertionKind::TextCount: {
            int n = count_kind(a.kind_arg, true);
            if (n == a.int_arg) return true;
            detail = "actual text count = " + std::to_string(n); return false;
        }
        case AssertionKind::Won:
            if (won) return true;
            detail = "win never fired during the run"; return false;
        case AssertionKind::NotWon:
            if (!won) return true;
            detail = "win flag fired"; return false;
        case AssertionKind::Tick:
            if (forward_ticks == a.int_arg) return true;
            detail = "tick count = " + std::to_string(forward_ticks); return false;
    }
    detail = "unhandled assertion kind";
    return false;
}

Input action_to_input(TestActionKind ak) {
    switch (ak) {
        case TestActionKind::Wait:      return Input::wait();
        case TestActionKind::MoveRight: return Input::move(Direction::Right);
        case TestActionKind::MoveUp:    return Input::move(Direction::Up);
        case TestActionKind::MoveLeft:  return Input::move(Direction::Left);
        case TestActionKind::MoveDown:  return Input::move(Direction::Down);
        case TestActionKind::Undo:      return Input::wait();  // undo is its own step (Phase 1: no-op)
    }
    return Input::wait();
}

}  // namespace

TestResult run_test_file(std::string const& path) {
    TestResult r;
    r.path = path;

    auto loaded = load_test_file(path);
    if (std::holds_alternative<ParseError>(loaded)) {
        auto& e = std::get<ParseError>(loaded);
        r.load_error = true;
        r.load_error_text = e.path + ":" + std::to_string(e.line) + ": " + e.message;
        return r;
    }

    auto& sc = std::get<TestScenario>(loaded);
    r.scenario_name = sc.setup.name;

    baba::sim::Simulator sim(std::move(sc.setup.world));

    bool won_ever      = false;
    int  forward_ticks = 0;

    for (auto const& act : sc.inputs) {
        if (act.kind == TestActionKind::Undo) {
            sim.step_back();  // undo step; does not count as a forward tick
            continue;
        }
        TickReport rep = sim.step_forward(action_to_input(act.kind));
        ++forward_ticks;
        if (rep.won) won_ever = true;
    }
    r.ticks_run = forward_ticks;

    r.assertions_total = static_cast<int>(sc.expected.size());
    for (auto const& a : sc.expected) {
        std::string detail;
        if (!eval_assertion(a, sim.world(), forward_ticks, won_ever, detail)) {
            r.failures.push_back({a.line, assertion_text(a) + " — " + detail});
            ++r.assertions_failed;
        }
    }
    return r;
}

void print_summary(TestResult const& r) {
    if (r.load_error) {
        std::printf("FAIL  %s  (load: %s)\n", r.path.c_str(), r.load_error_text.c_str());
        return;
    }
    if (r.passed()) {
        std::printf("PASS  %s  (%d/%d assertions, %d ticks)\n",
                    r.path.c_str(), r.assertions_total, r.assertions_total, r.ticks_run);
    } else {
        std::printf("FAIL  %s  (%d/%d failed, %d ticks)\n",
                    r.path.c_str(), r.assertions_failed, r.assertions_total, r.ticks_run);
    }
}

void print_detail(TestResult const& r) {
    if (r.load_error) {
        std::printf("  load error: %s\n", r.load_error_text.c_str());
        return;
    }
    for (auto const& f : r.failures) {
        std::printf("  %s:%d: %s\n", r.path.c_str(), f.line, f.message.c_str());
    }
}

}  // namespace baba::cli
