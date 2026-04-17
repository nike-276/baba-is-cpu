#include "tick.hpp"

#include "ruleset.hpp"

#include <algorithm>
#include <vector>

namespace baba::core {

namespace {

// Try to move a single YOU object `mover_id` by `step_v`. Returns true on
// success. Push chain rules:
//   * Walk forward from mover; collect every PUSH-property object in the way.
//   * Stop when a tile has no PUSH-property objects on it.
//   * If that resting tile contains a non-PUSH STOP-property object, the
//     entire move is aborted.
//   * Otherwise, slide every collected pushable forward by `step_v`, then
//     move the YOU object itself.
bool try_move(World& world, ObjectId mover_id, Coord step_v, RuleSet const& rs) {
    Object const* mover = world.get(mover_id);
    if (!mover) return false;
    Coord origin = mover->pos;

    // Walk forward collecting pushables.
    std::vector<ObjectId> chain;
    Coord cursor = {origin.x + step_v.x, origin.y + step_v.y};

    while (true) {
        auto const& cell = world.at(cursor);

        // Identify pushables and blockers in this tile.
        std::vector<ObjectId> pushables;
        bool any_blocker = false;
        for (ObjectId id : cell) {
            bool push = rs.object_has_property(world, id, Kind::P_Push);
            bool stop = rs.object_has_property(world, id, Kind::P_Stop);
            if (push) pushables.push_back(id);
            else if (stop) any_blocker = true;
        }

        if (any_blocker) return false;          // blocked, no move
        if (pushables.empty()) break;            // open tile, chain ends

        // Sort for determinism (ascending id).
        std::sort(pushables.begin(), pushables.end());
        for (ObjectId pid : pushables) chain.push_back(pid);

        cursor.x += step_v.x;
        cursor.y += step_v.y;
    }

    // Move the chain back-to-front so we never collide with ourselves.
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        Object const* o = world.get(*it);
        if (!o) continue;
        world.move(*it, {o->pos.x + step_v.x, o->pos.y + step_v.y});
    }
    world.move(mover_id, {origin.x + step_v.x, origin.y + step_v.y});
    return true;
}

bool check_win(World const& world, RuleSet const& rs) {
    for (Coord c : world.all_cells()) {
        auto const& ids = world.at(c);
        bool has_you{false}, has_win{false};
        for (ObjectId id : ids) {
            if (rs.object_has_property(world, id, Kind::P_You)) has_you = true;
            if (rs.object_has_property(world, id, Kind::P_Win)) has_win = true;
        }
        if (has_you && has_win) return true;
    }
    return false;
}

}  // namespace

TickReport apply_tick(World& world, Input input) {
    TickReport report;

    RuleSet rs = RuleSet::parse(world);

    if (input.kind == InputKind::Move) {
        Coord step_v = step(input.dir);

        // Snapshot YOU object ids in ascending order; multi-YOU resolved
        // deterministically per architecture.md §5.
        std::vector<ObjectId> you_ids;
        for (ObjectId id : world.all_ids()) {
            if (rs.object_has_property(world, id, Kind::P_You)) you_ids.push_back(id);
        }

        for (ObjectId yid : you_ids) {
            // Update facing first (independent of whether move succeeds).
            world.face(yid, input.dir);
            if (try_move(world, yid, step_v, rs)) {
                report.moved_count++;
            }
        }

        // Re-parse if rules may have changed.
        rs = RuleSet::parse(world);
    }

    report.won = check_win(world, rs);
    return report;
}

}  // namespace baba::core
