// core/tick.hpp — apply one tick of simulation given a player input.
//
// Full 9-phase pipeline per rule-engine-spec.md §4:
//   1. PARSE_INITIAL
//   2. APPLY_INPUT   — move every YOU object; resolve push chain (PUSH / STOP)
//   3. PARSE_POST_MOVE
//   4. TRANSFORM     — apply X IS Y transformation rules
//   5. PARSE_POST_TRANSFORM
//   6. DESTRUCT      — SINK / HOT+MELT / DEFEAT / OPEN+SHUT
//   7. PARSE_POST_DESTRUCT
//   8. CHECK_WIN     — any (YOU, WIN) co-tile triggers `won`
//   9. COMMIT        — TickReport.changes carries all mutations for the undo stack
#pragma once

#include "change.hpp"
#include "direction.hpp"
#include "world.hpp"

#include <vector>

namespace baba::core {

enum class InputKind : std::uint8_t {
    Wait = 0,
    Move = 1,
};

struct Input {
    InputKind kind{InputKind::Wait};
    Direction dir{Direction::Right};

    static Input wait()            { return {InputKind::Wait, Direction::Right}; }
    static Input move(Direction d) { return {InputKind::Move, d}; }
};

struct TickReport {
    bool won{false};
    int  moved_count{0};              // how many YOU objects actually moved
    std::vector<Change> changes;      // all mutations this tick, in forward order
};

// Forward-tick the world once. Mutates `world` in place. Pure with respect
// to anything outside `world`. No I/O, no globals, no RNG.
TickReport apply_tick(World& world, Input input);

}  // namespace baba::core
