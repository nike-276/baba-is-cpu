// core/tick.hpp — apply one tick of simulation given a player input.
//
// Full 10-phase pipeline:
//   1.   PARSE_INITIAL
//   1.5  APPLY_DIRECTIONAL  — set facing of UP/DOWN/LEFT/RIGHT objects
//   2.   APPLY_INPUT        — move every YOU object; resolve push chain (PUSH / STOP)
//   2.5  APPLY_AUTO_MOVE    — self-propelled: MOVE (push+flip), AUTO (push+no flip)
//   2.53 APPLY_NUDGE
//   2.55 APPLY_FEAR
//   2.6  APPLY_SHIFT
//   2.7  APPLY_SWAP
//   3.   PARSE_POST_MOVE
//   3.5  APPLY_FOLLOW
//   4.   TRANSFORM          — apply X IS Y transformation rules + REVERT
//   5.   PARSE_POST_TRANSFORM
//   5.5  APPLY_FALL         — FALL* slides after REVERT resolves (wiki Order of Ops)
//   6.   DESTRUCT           — SINK / EAT / HOT+MELT / WEAK / DEFEAT / OPEN+SHUT
//   7.   PARSE_POST_DESTRUCT
//   8.   CHECK_WIN          — any (YOU, WIN) co-tile triggers `won`
//   9.   COMMIT             — TickReport.changes carries all mutations for undo stack
#pragma once

#include "change.hpp"
#include "direction.hpp"
#include "kind.hpp"
#include "world.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace baba::core {

// Named phases of the tick pipeline, used as indices into PhaseTimings.
enum class Phase : int {
    ParseInitial = 0,
    Directional, Input, AutoMove, Nudge, Fear, Shift, Swap,
    ParsePostMove, Follow, Transform, ParsePostTransform,
    Fall, Destruct, Has, Make, ParsePostDestruct, Play, CheckWin,
    Count  // sentinel
};

// Per-tick nanosecond timings for each phase. Always populated by apply_tick.
struct PhaseTimings {
    std::array<std::int64_t, static_cast<int>(Phase::Count)> ns{};
    std::int64_t total_ns{0};
};

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

// One note emitted by a PLAY rule during a tick.
// `noun` carries the subject kind for future per-object timbre lookup.
// `pos` carries the tile position for future spatial audio.
struct SoundEvent {
    Kind  noun;
    Kind  note;          // O_LetterA…O_LetterG
    int   octave{5};
    bool  sharp{false};
    bool  flat{false};
    Coord pos;
};

struct TickReport {
    bool won{false};
    int  moved_count{0};              // how many YOU objects actually moved
    std::vector<Change>     changes;  // all mutations this tick, in forward order
    std::vector<SoundEvent> sound_events;
    PhaseTimings            timings;  // nanosecond breakdown by phase
};

// Forward-tick the world once. Mutates `world` in place. Pure with respect
// to anything outside `world`. No I/O, no globals, no RNG.
TickReport apply_tick(World& world, Input input);

}  // namespace baba::core
