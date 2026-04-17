// core/tick.hpp — apply one tick of simulation given a player input.
//
// Phase 1 implements a subset of the spec:
//   1. PARSE_INITIAL — RuleSet::parse(world)
//   2. APPLY_INPUT   — for every YOU object (ascending id), try to step in
//                      `input` direction; resolve push chain (PUSH / STOP).
//   3. PARSE_POST_MOVE — re-parse for win check
//   4. CHECK_WIN     — any (YOU, WIN) co-tile triggers `won`.
//
// TRANSFORM, DESTRUCT, OPEN/SHUT, SINK, HOT/MELT, DEFEAT all land later.
#pragma once

#include "direction.hpp"
#include "world.hpp"

#include <optional>

namespace baba::core {

enum class InputKind : std::uint8_t {
    Wait = 0,
    Move = 1,
};

struct Input {
    InputKind kind{InputKind::Wait};
    Direction dir{Direction::Right};

    static Input wait()                 { return {InputKind::Wait, Direction::Right}; }
    static Input move(Direction d)      { return {InputKind::Move, d}; }
};

struct TickReport {
    bool won{false};
    int  moved_count{0};   // diagnostic: how many YOU objects actually moved
};

// Forward-tick the world once. Mutates `world` in place. Pure with respect
// to anything outside `world`. No I/O, no globals, no RNG.
TickReport apply_tick(World& world, Input input);

}  // namespace baba::core
