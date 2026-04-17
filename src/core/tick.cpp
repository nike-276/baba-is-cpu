#include "tick.hpp"

#include "ruleset.hpp"

#include <algorithm>
#include <map>
#include <unordered_set>
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

// Phase 4: apply NOUN IS NOUN transformation rules simultaneously.
// Single-target: retype in place (preserves id). Multi-target: destroy + spawn.
void apply_transforms(World& world, RuleSet const& rs) {
    auto const& transforms = rs.transform_rules();
    if (transforms.empty()) return;

    // Build from-kind → [to-kinds] map (using sorted vector for determinism).
    std::map<Kind, std::vector<Kind>> xmap;
    for (auto const& tr : transforms) {
        xmap[tr.from].push_back(tr.to);
    }

    // Snapshot objects to transform against pre-transform state.
    struct XEntry { ObjectId id; Coord pos; Direction facing; std::vector<Kind> targets; };
    std::vector<XEntry> pending;
    for (ObjectId id : world.all_ids()) {
        Object const* o = world.get(id);
        if (!o || o->text) continue;
        auto it = xmap.find(o->kind);
        if (it != xmap.end()) {
            pending.push_back({id, o->pos, o->facing, it->second});
        }
    }

    for (auto const& e : pending) {
        if (!world.get(e.id)) continue;  // already destroyed this tick
        if (e.targets.size() == 1) {
            world.retype(e.id, e.targets[0]);
        } else {
            world.destroy(e.id);
            for (Kind target : e.targets) {
                world.spawn(e.pos, target, /*text=*/false, e.facing);
            }
        }
    }
}

// Phase 6: apply DEFEAT / SINK / HOT+MELT / OPEN+SHUT in precedence order.
// Within each sub-step, all destructions are gathered simultaneously, then applied.
void apply_destructions(World& world, RuleSet const& rs) {
    auto destroy_set = [&](std::unordered_set<ObjectId> const& ids) {
        // Apply in ascending id order for determinism.
        std::vector<ObjectId> sorted(ids.begin(), ids.end());
        std::sort(sorted.begin(), sorted.end());
        for (ObjectId id : sorted) world.destroy(id);
    };

    // a. SINK
    {
        std::unordered_set<ObjectId> doomed;
        for (Coord c : world.all_cells()) {
            auto const& cell = world.at(c);
            if (cell.size() < 2) continue;
            bool any_sink = false;
            for (ObjectId id : cell) {
                if (rs.object_has_property(world, id, Kind::P_Sink)) { any_sink = true; break; }
            }
            if (any_sink) {
                for (ObjectId id : cell) doomed.insert(id);
            }
        }
        destroy_set(doomed);
    }

    // b. HOT / MELT
    {
        std::unordered_set<ObjectId> doomed;
        for (Coord c : world.all_cells()) {
            auto const& cell = world.at(c);
            bool any_hot = false, any_melt = false;
            for (ObjectId id : cell) {
                if (rs.object_has_property(world, id, Kind::P_Hot))  any_hot  = true;
                if (rs.object_has_property(world, id, Kind::P_Melt)) any_melt = true;
            }
            if (any_hot && any_melt) {
                for (ObjectId id : cell) {
                    if (rs.object_has_property(world, id, Kind::P_Melt)) doomed.insert(id);
                }
            }
        }
        destroy_set(doomed);
    }

    // c. DEFEAT
    {
        std::unordered_set<ObjectId> doomed;
        for (Coord c : world.all_cells()) {
            auto const& cell = world.at(c);
            bool any_defeat = false;
            for (ObjectId id : cell) {
                if (rs.object_has_property(world, id, Kind::P_Defeat)) { any_defeat = true; break; }
            }
            if (any_defeat) {
                for (ObjectId id : cell) {
                    if (rs.object_has_property(world, id, Kind::P_You)) doomed.insert(id);
                }
            }
        }
        destroy_set(doomed);
    }

    // d. OPEN / SHUT
    {
        std::unordered_set<ObjectId> doomed;
        for (Coord c : world.all_cells()) {
            auto const& cell = world.at(c);
            bool any_open = false, any_shut = false;
            for (ObjectId id : cell) {
                if (rs.object_has_property(world, id, Kind::P_Open)) any_open = true;
                if (rs.object_has_property(world, id, Kind::P_Shut)) any_shut = true;
            }
            if (any_open && any_shut) {
                for (ObjectId id : cell) {
                    if (rs.object_has_property(world, id, Kind::P_Open) ||
                        rs.object_has_property(world, id, Kind::P_Shut)) doomed.insert(id);
                }
            }
        }
        destroy_set(doomed);
    }
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

    // Phase 1: PARSE_INITIAL
    RuleSet rs = RuleSet::parse(world);

    // Phase 2: APPLY_INPUT
    if (input.kind == InputKind::Move) {
        Coord step_v = step(input.dir);

        std::vector<ObjectId> you_ids;
        for (ObjectId id : world.all_ids()) {
            if (rs.object_has_property(world, id, Kind::P_You)) you_ids.push_back(id);
        }

        for (ObjectId yid : you_ids) {
            world.face(yid, input.dir);
            if (try_move(world, yid, step_v, rs)) {
                report.moved_count++;
            }
        }
    }

    // Phase 3: PARSE_POST_MOVE
    rs = RuleSet::parse(world);

    // Phase 4: TRANSFORM
    apply_transforms(world, rs);

    // Phase 5: PARSE_POST_TRANSFORM
    rs = RuleSet::parse(world);

    // Phase 6: DESTRUCT
    apply_destructions(world, rs);

    // Phase 7: PARSE_POST_DESTRUCT
    rs = RuleSet::parse(world);

    // Phase 8: CHECK_WIN
    report.won = check_win(world, rs);

    return report;
}

}  // namespace baba::core
