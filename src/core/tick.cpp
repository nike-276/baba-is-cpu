#include "tick.hpp"

#include "ruleset.hpp"

#include <algorithm>
#include <map>
#include <unordered_set>
#include <vector>

namespace baba::core {

namespace {

// ── World mutator wrappers that also record a Change ──────────────────────

void do_move(World& world, ObjectId id, Coord new_pos, std::vector<Change>& log) {
    Object const* o = world.get(id);
    if (!o) return;
    Coord old_pos = o->pos;
    if (old_pos == new_pos) return;
    world.move(id, new_pos);
    log.push_back(Change::move(id, old_pos, new_pos));
}

void do_face(World& world, ObjectId id, Direction d, std::vector<Change>& log) {
    Object const* o = world.get(id);
    if (!o) return;
    Direction old_dir = o->facing;
    world.face(id, d);
    if (old_dir != d) log.push_back(Change::face(id, old_dir, d));
}

ObjectId do_spawn(World& world, Coord pos, Kind kind, bool text, Direction facing,
                  std::vector<Change>& log) {
    ObjectId id = world.spawn(pos, kind, text, facing);
    log.push_back(Change::spawn(id));
    return id;
}

void do_destroy(World& world, ObjectId id, std::vector<Change>& log) {
    Object const* o = world.get(id);
    if (!o) return;
    log.push_back(Change::destroy(id, o->pos, o->kind, o->text, o->facing));
    world.destroy(id);
}

void do_retype(World& world, ObjectId id, Kind new_kind, std::vector<Change>& log) {
    Object const* o = world.get(id);
    if (!o) return;
    Kind old_kind = o->kind;
    world.retype(id, new_kind);
    if (old_kind != new_kind) log.push_back(Change::retype(id, old_kind, new_kind));
}

// ── APPLY_INPUT helpers ────────────────────────────────────────────────────

bool try_move(World& world, ObjectId mover_id, Coord step_v, RuleSet const& rs,
              std::vector<Change>& log) {
    Object const* mover = world.get(mover_id);
    if (!mover) return false;
    Coord origin = mover->pos;

    std::vector<ObjectId> chain;
    Coord cursor = {origin.x + step_v.x, origin.y + step_v.y};

    while (true) {
        auto const& cell = world.at(cursor);

        std::vector<ObjectId> pushables;
        bool any_blocker = false;
        for (ObjectId id : cell) {
            bool push = rs.object_has_property(world, id, Kind::P_Push);
            bool stop = rs.object_has_property(world, id, Kind::P_Stop);
            if (push)       pushables.push_back(id);
            else if (stop)  any_blocker = true;
        }

        if (any_blocker) return false;
        if (pushables.empty()) break;

        std::sort(pushables.begin(), pushables.end());
        for (ObjectId pid : pushables) chain.push_back(pid);
        cursor.x += step_v.x;
        cursor.y += step_v.y;
    }

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        Object const* o = world.get(*it);
        if (!o) continue;
        do_move(world, *it, {o->pos.x + step_v.x, o->pos.y + step_v.y}, log);
    }
    do_move(world, mover_id, {origin.x + step_v.x, origin.y + step_v.y}, log);
    return true;
}

// ── APPLY_AUTO_MOVE phase ──────────────────────────────────────────────────

void apply_auto_move(World& world, RuleSet const& rs, std::vector<Change>& log) {
    struct Mover { ObjectId id; Coord step_v; bool flip_on_block; };
    std::vector<Mover> movers;

    for (ObjectId id : world.all_ids()) {
        Object const* o = world.get(id);
        if (!o || o->text) continue;
        if (rs.object_has_property(world, id, Kind::P_Move))
            movers.push_back({id, step(o->facing), true});
        else if (rs.object_has_property(world, id, Kind::P_Auto))
            movers.push_back({id, step(o->facing), false});
        else if (rs.object_has_property(world, id, Kind::P_Fall))
            movers.push_back({id, step(Direction::Down), false});
        else if (rs.object_has_property(world, id, Kind::P_Fallup))
            movers.push_back({id, step(Direction::Up), false});
        else if (rs.object_has_property(world, id, Kind::P_Fallleft))
            movers.push_back({id, step(Direction::Left), false});
        else if (rs.object_has_property(world, id, Kind::P_Fallright))
            movers.push_back({id, step(Direction::Right), false});
    }

    for (auto const& m : movers) {
        Object const* o = world.get(m.id);
        if (!o) continue;
        if (!try_move(world, m.id, m.step_v, rs, log)) {
            if (m.flip_on_block) do_face(world, m.id, opposite(o->facing), log);
        }
    }
}

// ── APPLY_MAKE phase ───────────────────────────────────────────────────────

void apply_make(World& world, RuleSet const& rs, std::vector<Change>& log) {
    auto const& makes = rs.make_rules();
    if (makes.empty()) return;

    // Snapshot all occupied cells to avoid iterating modified world.
    std::vector<Coord> cells = world.all_cells();
    for (auto const& mr : makes) {
        for (Coord c : cells) {
            bool has_from = false, has_to = false;
            for (ObjectId id : world.at(c)) {
                Object const* o = world.get(id);
                if (!o || o->text) continue;
                if (o->kind == mr.from) has_from = true;
                if (o->kind == mr.to)   has_to   = true;
            }
            if (has_from && !has_to)
                do_spawn(world, c, mr.to, /*text=*/false, Direction::Right, log);
        }
    }
}

// ── TRANSFORM phase ────────────────────────────────────────────────────────

void apply_transforms(World& world, RuleSet const& rs, std::vector<Change>& log) {
    auto const& transforms = rs.transform_rules();
    if (transforms.empty()) return;

    std::map<Kind, std::vector<Kind>> xmap;
    for (auto const& tr : transforms) {
        xmap[tr.from].push_back(tr.to);
    }

    // X IS X protection: remove any entry where the source has a self-transform.
    for (auto it = xmap.begin(); it != xmap.end(); ) {
        bool self = false;
        for (Kind t : it->second) if (t == it->first) { self = true; break; }
        if (self) it = xmap.erase(it);
        else      ++it;
    }
    if (xmap.empty()) return;

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
        if (!world.get(e.id)) continue;
        if (e.targets.size() == 1) {
            do_retype(world, e.id, e.targets[0], log);
        } else {
            do_destroy(world, e.id, log);
            for (Kind target : e.targets) {
                do_spawn(world, e.pos, target, /*text=*/false, e.facing, log);
            }
        }
    }
}

// ── DESTRUCT phase ─────────────────────────────────────────────────────────

void apply_destructions(World& world, RuleSet const& rs, std::vector<Change>& log) {
    auto destroy_set = [&](std::unordered_set<ObjectId> const& ids) {
        std::vector<ObjectId> sorted(ids.begin(), ids.end());
        std::sort(sorted.begin(), sorted.end());
        for (ObjectId id : sorted) do_destroy(world, id, log);
    };

    // Capture tiles that received a moving object this tick (for WEAK check below).
    // Read log before any destructions add to it.
    std::unordered_set<Coord, CoordHash> arrived;
    for (auto const& c : log) {
        if (c.kind == ChangeKind::Move) arrived.insert(c.to_pos);
    }

    // a. SINK
    {
        std::unordered_set<ObjectId> doomed;
        for (Coord c : world.all_cells()) {
            auto const& cell = world.at(c);
            if (cell.size() < 2) continue;
            bool any_sink = false;
            for (ObjectId id : cell)
                if (rs.object_has_property(world, id, Kind::P_Sink)) { any_sink = true; break; }
            if (any_sink)
                for (ObjectId id : cell) doomed.insert(id);
        }
        destroy_set(doomed);
    }

    // b. EAT — an EAT object destroys all non-EAT, non-text objects on the same tile.
    {
        std::unordered_set<ObjectId> doomed;
        for (Coord c : world.all_cells()) {
            auto const& cell = world.at(c);
            bool any_eat = false;
            for (ObjectId id : cell)
                if (rs.object_has_property(world, id, Kind::P_Eat)) { any_eat = true; break; }
            if (!any_eat) continue;
            for (ObjectId id : cell) {
                Object const* o = world.get(id);
                if (!o || o->text) continue;
                if (!rs.object_has_property(world, id, Kind::P_Eat)) doomed.insert(id);
            }
        }
        destroy_set(doomed);
    }

    // c. HOT / MELT (re-labeled; was b)
    {
        std::unordered_set<ObjectId> doomed;
        for (Coord c : world.all_cells()) {
            auto const& cell = world.at(c);
            bool any_hot = false, any_melt = false;
            for (ObjectId id : cell) {
                if (rs.object_has_property(world, id, Kind::P_Hot))  any_hot  = true;
                if (rs.object_has_property(world, id, Kind::P_Melt)) any_melt = true;
            }
            if (any_hot && any_melt)
                for (ObjectId id : cell)
                    if (rs.object_has_property(world, id, Kind::P_Melt)) doomed.insert(id);
        }
        destroy_set(doomed);
    }

    // d. WEAK — destroyed when any object arrives on their tile this tick.
    if (!arrived.empty()) {
        std::unordered_set<ObjectId> doomed;
        for (Coord const& c : arrived) {
            for (ObjectId id : world.at(c)) {
                if (rs.object_has_property(world, id, Kind::P_Weak)) doomed.insert(id);
            }
        }
        destroy_set(doomed);
    }

    // e. DEFEAT
    {
        std::unordered_set<ObjectId> doomed;
        for (Coord c : world.all_cells()) {
            auto const& cell = world.at(c);
            bool any_defeat = false;
            for (ObjectId id : cell)
                if (rs.object_has_property(world, id, Kind::P_Defeat)) { any_defeat = true; break; }
            if (any_defeat)
                for (ObjectId id : cell)
                    if (rs.object_has_property(world, id, Kind::P_You)) doomed.insert(id);
        }
        destroy_set(doomed);
    }

    // f. OPEN / SHUT
    {
        std::unordered_set<ObjectId> doomed;
        for (Coord c : world.all_cells()) {
            auto const& cell = world.at(c);
            bool any_open = false, any_shut = false;
            for (ObjectId id : cell) {
                if (rs.object_has_property(world, id, Kind::P_Open)) any_open = true;
                if (rs.object_has_property(world, id, Kind::P_Shut)) any_shut = true;
            }
            if (any_open && any_shut)
                for (ObjectId id : cell)
                    if (rs.object_has_property(world, id, Kind::P_Open) ||
                        rs.object_has_property(world, id, Kind::P_Shut)) doomed.insert(id);
        }
        destroy_set(doomed);
    }
}

// ── CHECK_WIN ──────────────────────────────────────────────────────────────

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

// ── apply_tick (9-phase pipeline) ─────────────────────────────────────────

TickReport apply_tick(World& world, Input input) {
    TickReport report;
    std::vector<Change>& log = report.changes;

    // Phase 1: PARSE_INITIAL
    RuleSet rs = RuleSet::parse(world);

    // Phase 2: APPLY_INPUT
    if (input.kind == InputKind::Move) {
        Coord step_v = step(input.dir);

        std::vector<ObjectId> you_ids;
        for (ObjectId id : world.all_ids())
            if (rs.object_has_property(world, id, Kind::P_You)) you_ids.push_back(id);

        for (ObjectId yid : you_ids) {
            do_face(world, yid, input.dir, log);
            if (try_move(world, yid, step_v, rs, log)) report.moved_count++;
        }
    }

    // Phase 2.5: APPLY_AUTO_MOVE
    apply_auto_move(world, rs, log);

    // Phase 3: PARSE_POST_MOVE
    rs = RuleSet::parse(world);

    // Phase 4: TRANSFORM
    apply_transforms(world, rs, log);

    // Phase 5: PARSE_POST_TRANSFORM
    rs = RuleSet::parse(world);

    // Phase 6: DESTRUCT
    apply_destructions(world, rs, log);

    // Phase 6.5: APPLY_MAKE
    apply_make(world, rs, log);

    // Phase 7: PARSE_POST_DESTRUCT
    rs = RuleSet::parse(world);

    // Phase 8: CHECK_WIN
    report.won = check_win(world, rs);

    // Phase 9: COMMIT — changes are already in report.changes; caller pushes to undo stack

    return report;
}

}  // namespace baba::core
