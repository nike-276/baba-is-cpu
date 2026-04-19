#include "tick.hpp"

#include "ruleset.hpp"

#include <algorithm>
#include <map>
#include <unordered_map>
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

    // STILL objects cannot move themselves.
    if (rs.object_has_property(world, mover_id, Kind::P_Still)) return false;

    Coord origin = mover->pos;

    // SWAP movers ignore all solidity — move directly to adjacent tile, no chain.
    // The position exchange with whatever is there is handled in apply_swap.
    bool mover_is_swap = rs.object_has_property(world, mover_id, Kind::P_Swap);
    if (mover_is_swap) {
        do_move(world, mover_id, {origin.x + step_v.x, origin.y + step_v.y}, log);
        return true;
    }

    // OPEN chains can bypass SHUT blockers (wiki §OPEN).
    bool chain_has_open = rs.object_has_property(world, mover_id, Kind::P_Open);

    std::vector<ObjectId> chain;
    Coord cursor = {origin.x + step_v.x, origin.y + step_v.y};

    while (true) {
        auto const& cell = world.at(cursor);

        std::vector<ObjectId> pushables;
        bool any_blocker = false;
        for (ObjectId id : cell) {
            bool still = rs.object_has_property(world, id, Kind::P_Still);
            bool push  = rs.object_has_property(world, id, Kind::P_Push);
            bool stop  = rs.object_has_property(world, id, Kind::P_Stop);
            bool swap  = rs.object_has_property(world, id, Kind::P_Swap);

            if (still) {
                // STILL objects cannot be displaced. If they have PUSH (would need to be
                // pushed to make room) or STOP they block; STILL alone allows co-location.
                if (push || stop) {
                    bool shut = stop && rs.object_has_property(world, id, Kind::P_Shut);
                    if (!swap && !(shut && chain_has_open)) any_blocker = true;
                }
            } else if (push) {
                pushables.push_back(id);
                if (rs.object_has_property(world, id, Kind::P_Open)) chain_has_open = true;
            } else if (stop) {
                // SWAP property overrides STOP solidity (mover passes through).
                bool shut = rs.object_has_property(world, id, Kind::P_Shut);
                if (!swap && !(shut && chain_has_open)) any_blocker = true;
            }
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

// ── APPLY_DIRECTIONAL phase ────────────────────────────────────────────────
// UP/DOWN/LEFT/RIGHT set the facing of matching objects; no movement.

void apply_directional(World& world, RuleSet const& rs, std::vector<Change>& log) {
    for (ObjectId id : world.all_ids()) {
        Object const* o = world.get(id);
        if (!o || o->text) continue;
        if      (rs.object_has_property(world, id, Kind::P_Left))  do_face(world, id, Direction::Left,  log);
        else if (rs.object_has_property(world, id, Kind::P_Right)) do_face(world, id, Direction::Right, log);
        else if (rs.object_has_property(world, id, Kind::P_Up))    do_face(world, id, Direction::Up,    log);
        else if (rs.object_has_property(world, id, Kind::P_Down))  do_face(world, id, Direction::Down,  log);
    }
}

// ── APPLY_AUTO_MOVE phase ──────────────────────────────────────────────────

// FALL variant: slides until blocked; does NOT push (stops at PUSH or STOP).
void fall_slide(World& world, ObjectId id, Coord step_v, RuleSet const& rs,
                std::vector<Change>& log) {
    for (int i = 0; i < 1024; ++i) {
        Object const* o = world.get(id);
        if (!o) return;
        Coord next = {o->pos.x + step_v.x, o->pos.y + step_v.y};
        auto const& cell = world.at(next);
        for (ObjectId other : cell) {
            if (rs.object_has_property(world, other, Kind::P_Stop)) return;
            if (rs.object_has_property(world, other, Kind::P_Push)) return;
        }
        do_move(world, id, next, log);
    }
}

void apply_auto_move(World& world, RuleSet const& rs, std::vector<Change>& log) {
    struct Mover { ObjectId id; Coord step_v; bool slide; bool flip_on_block; };
    std::vector<Mover> movers;

    for (ObjectId id : world.all_ids()) {
        Object const* o = world.get(id);
        if (!o || o->text) continue;
        if (rs.object_has_property(world, id, Kind::P_Still)) continue;
        // FALL* overrides MOVE/AUTO — a sliding object ignores self-propulsion.
        if (rs.object_has_property(world, id, Kind::P_Fall))
            movers.push_back({id, step(Direction::Down),  true, false});
        else if (rs.object_has_property(world, id, Kind::P_Fallup))
            movers.push_back({id, step(Direction::Up),    true, false});
        else if (rs.object_has_property(world, id, Kind::P_Fallleft))
            movers.push_back({id, step(Direction::Left),  true, false});
        else if (rs.object_has_property(world, id, Kind::P_Fallright))
            movers.push_back({id, step(Direction::Right), true, false});
        else if (rs.object_has_property(world, id, Kind::P_Move))
            movers.push_back({id, step(o->facing), false, true});
        else if (rs.object_has_property(world, id, Kind::P_Auto))
            movers.push_back({id, step(o->facing), false, false});
    }

    for (auto const& m : movers) {
        Object const* o = world.get(m.id);
        if (!o) continue;
        if (m.slide) {
            fall_slide(world, m.id, m.step_v, rs, log);
        } else {
            if (!try_move(world, m.id, m.step_v, rs, log)) {
                if (m.flip_on_block) do_face(world, m.id, opposite(o->facing), log);
            }
        }
    }
}

// ── APPLY_SHIFT phase ─────────────────────────────────────────────────────
// SHIFT objects push all co-located non-text, non-SHIFT, non-STILL objects
// one step in the SHIFT object's facing direction (respects STOP/STILL).

void apply_shift(World& world, RuleSet const& rs, std::vector<Change>& log) {
    std::vector<ObjectId> shifters;
    for (ObjectId id : world.all_ids()) {
        Object const* o = world.get(id);
        if (!o || o->text) continue;
        if (rs.object_has_property(world, id, Kind::P_Shift))
            shifters.push_back(id);
    }
    for (ObjectId sid : shifters) {
        Object const* s = world.get(sid);
        if (!s) continue;
        Coord pos = s->pos;
        Coord step_v = step(s->facing);
        std::vector<ObjectId> targets;
        for (ObjectId id : world.at(pos)) {
            if (id == sid) continue;
            Object const* o = world.get(id);
            if (!o || o->text) continue;
            if (rs.object_has_property(world, id, Kind::P_Shift)) continue;
            targets.push_back(id);
        }
        std::sort(targets.begin(), targets.end());
        for (ObjectId tid : targets) {
            if (try_move(world, tid, step_v, rs, log))
                do_face(world, tid, s->facing, log);
        }
    }
}

// ── APPLY_SWAP phase ───────────────────────────────────────────────────────
// After all movement, objects sharing a tile with a SWAP object exchange
// positions: the non-SWAP goes to SWAP's origin (or SWAP goes to the mover's
// origin), based on which one moved into the shared tile.

// initial_swap: set of object IDs that had SWAP at PARSE_INITIAL (pre-movement).
// Conditional SWAP ("BABA ON FLAG IS SWAP") must not activate in the same tick
// that the object first lands on the condition noun — those cases are evaluated
// against the post-movement world, which would spuriously trigger exchanges.
void apply_swap(World& world, RuleSet const& rs,
                std::unordered_set<ObjectId> const& initial_swap,
                std::vector<Change>& log) {
    // Build original-position map from Move entries recorded so far.
    // emplace guarantees we capture the *first* Move per object (its true origin).
    std::unordered_map<ObjectId, Coord> original_pos;
    for (auto const& c : log) {
        if (c.kind == ChangeKind::Move)
            original_pos.emplace(c.id, c.from_pos);
    }

    struct Teleport { ObjectId id; Coord dest; };
    std::vector<Teleport> teleports;
    std::unordered_set<ObjectId> processed;

    for (ObjectId swap_id : world.all_ids()) {
        Object const* s = world.get(swap_id);
        if (!s || s->text) continue;
        if (!initial_swap.count(swap_id)) continue;
        if (processed.count(swap_id)) continue;

        bool swap_moved = original_pos.count(swap_id) > 0;
        Coord swap_pos = s->pos;

        for (ObjectId other_id : world.at(swap_pos)) {
            if (other_id == swap_id) continue;
            Object const* o = world.get(other_id);
            if (!o || o->text) continue;
            if (initial_swap.count(other_id)) continue;
            if (processed.count(other_id)) continue;

            bool other_moved = original_pos.count(other_id) > 0;
            if (!swap_moved && !other_moved) continue;

            if (swap_moved) {
                // SWAP arrived at other's tile → other teleports to SWAP's origin.
                if (!rs.object_has_property(world, other_id, Kind::P_Still))
                    teleports.push_back({other_id, original_pos.at(swap_id)});
            } else {
                // Other arrived at SWAP's tile → SWAP teleports to other's origin.
                if (!rs.object_has_property(world, swap_id, Kind::P_Still))
                    teleports.push_back({swap_id, original_pos.at(other_id)});
            }
            processed.insert(swap_id);
            processed.insert(other_id);
            break;
        }
    }

    std::sort(teleports.begin(), teleports.end(),
              [](Teleport const& a, Teleport const& b) { return a.id < b.id; });
    for (auto const& t : teleports)
        do_move(world, t.id, t.dest, log);
}

// ── APPLY_MAKE phase ───────────────────────────────────────────────────────

void apply_make(World& world, RuleSet const& rs, std::vector<Change>& log) {
    bool any = !rs.make_rules().empty() || !rs.conditional_make_rules().empty();
    if (!any) return;

    std::vector<Coord> cells = world.all_cells();

    // Unconditional MAKE rules.
    for (auto const& mr : rs.make_rules()) {
        for (Coord c : cells) {
            bool has_to = false;
            Direction src_facing = Direction::Right;
            bool has_from = false;
            for (ObjectId id : world.at(c)) {
                Object const* o = world.get(id);
                if (!o || o->text) continue;
                if (o->kind == mr.from) { has_from = true; src_facing = o->facing; }
                if (o->kind == mr.to)   has_to = true;
            }
            if (has_from && !has_to)
                do_spawn(world, c, mr.to, /*text=*/false, src_facing, log);
        }
    }

    // Conditional MAKE rules (ON/NOT ON).
    for (auto const& cmr : rs.conditional_make_rules()) {
        for (ObjectId id : world.all_ids()) {
            Object const* o = world.get(id);
            if (!o || o->text || o->kind != cmr.subject) continue;
            // Check all condition nouns. ON A AND B: all present. NOT ON A AND B: not all present.
            bool all_found = true;
            for (Kind cn : cmr.condition_nouns) {
                bool found = false;
                for (ObjectId other : world.at(o->pos)) {
                    if (other == id) continue;
                    Object const* ob = world.get(other);
                    if (ob && !ob->text && ob->kind == cn) { found = true; break; }
                }
                if (!found) { all_found = false; break; }
            }
            if (!(cmr.negated_condition ? !all_found : all_found)) continue;
            // Spawn target if not already present.
            bool already = false;
            for (ObjectId other : world.at(o->pos)) {
                Object const* ob = world.get(other);
                if (ob && !ob->text && ob->kind == cmr.target) { already = true; break; }
            }
            if (!already) do_spawn(world, o->pos, cmr.target, false, o->facing, log);
        }
    }
}

// ── TRANSFORM phase ────────────────────────────────────────────────────────

void apply_transforms(World& world, RuleSet const& rs, std::vector<Change>& log) {
    auto const& transforms = rs.transform_rules();
    if (transforms.empty() && rs.conditional_transform_rules().empty()) return;

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
    if (xmap.empty() && rs.conditional_transform_rules().empty()) return;

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

    // Conditional transforms (NOUN ON NOUN IS NOUN [AND NOUN]*): check per-object.
    // Collect ALL matching targets per object into one map entry so they are
    // applied atomically (like unconditional multi-target transforms), preventing
    // sequential single-target retypes from overwriting each other.
    {
        std::map<ObjectId, std::vector<Kind>> cond_targets;
        for (ObjectId id : world.all_ids()) {
            Object const* o = world.get(id);
            if (!o || o->text) continue;
            for (auto const& ctr : rs.conditional_transform_rules()) {
                if (ctr.subject != o->kind) continue;
                bool all_found = true;
                for (Kind cn : ctr.condition_nouns) {
                    bool found = false;
                    for (ObjectId other : world.at(o->pos)) {
                        if (other == id) continue;
                        Object const* ob = world.get(other);
                        if (ob && !ob->text && ob->kind == cn) { found = true; break; }
                    }
                    if (!found) { all_found = false; break; }
                }
                if (ctr.negated_condition ? !all_found : all_found) cond_targets[id].push_back(ctr.target);
            }
        }
        for (auto& [id, targets] : cond_targets) {
            // Sort + dedup handles duplicates from multiple scan starting positions.
            std::sort(targets.begin(), targets.end());
            targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
            Object const* o = world.get(id);
            if (o) pending.push_back({id, o->pos, o->facing, targets});
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

    // b. EAT — NOUN EAT NOUN: for each rule, destroy target-kind objects on tiles
    //    that also contain a subject-kind object. Subject survives.
    {
        std::unordered_set<ObjectId> doomed;
        for (auto const& er : rs.eat_rules()) {
            for (Coord c : world.all_cells()) {
                bool has_eater = false;
                for (ObjectId id : world.at(c)) {
                    Object const* o = world.get(id);
                    if (o && !o->text && o->kind == er.subject) { has_eater = true; break; }
                }
                if (!has_eater) continue;
                for (ObjectId id : world.at(c)) {
                    Object const* o = world.get(id);
                    if (o && !o->text && o->kind == er.target) doomed.insert(id);
                }
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

// ── APPLY_HAS phase ────────────────────────────────────────────────────────
// Fires after all DESTRUCT sub-steps complete. For each non-text object destroyed
// during DESTRUCT, spawn each HAS target at the destroyed object's tile.
// [DEVIATION] v1 processes HAS after the entire DESTRUCT phase; conditional HAS
// interactions within DESTRUCT are not modelled.
void apply_has(World& world, RuleSet const& rs,
               std::vector<Change>& log, std::size_t destruct_start) {
    if (rs.has_rules().empty()) return;
    std::size_t const n = log.size();
    for (std::size_t i = destruct_start; i < n; ++i) {
        Change const c = log[i];  // copy: do_spawn may reallocate log, invalidating refs
        if (c.kind != ChangeKind::Destroy) continue;
        if (c.obj_text) continue;  // text tiles do not trigger HAS
        for (auto const& hr : rs.has_rules()) {
            if (hr.subject != c.obj_kind) continue;
            do_spawn(world, c.obj_pos, hr.target, /*text=*/false, c.obj_facing, log);
        }
    }
}

}  // namespace

// ── apply_tick (9-phase pipeline) ─────────────────────────────────────────

TickReport apply_tick(World& world, Input input) {
    TickReport report;
    std::vector<Change>& log = report.changes;

    // Phase 1: PARSE_INITIAL
    RuleSet rs = RuleSet::parse(world);

    // Snapshot objects that have SWAP at tick-start so apply_swap doesn't
    // spuriously activate conditional SWAP acquired mid-tick by landing on the
    // condition noun (e.g. "BABA ON FLAG IS SWAP" must not fire the same tick
    // baba first reaches the flag tile via fall_slide).
    std::unordered_set<ObjectId> initial_swap;
    for (ObjectId id : world.all_ids()) {
        Object const* o = world.get(id);
        if (!o || o->text) continue;
        if (rs.object_has_property(world, id, Kind::P_Swap))
            initial_swap.insert(id);
    }

    // Phase 1.5: APPLY_DIRECTIONAL — set facing of UP/DOWN/LEFT/RIGHT objects
    apply_directional(world, rs, log);

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

    // Phase 2.6: APPLY_SHIFT
    apply_shift(world, rs, log);

    // Phase 2.7: APPLY_SWAP
    apply_swap(world, rs, initial_swap, log);

    // Phase 3: PARSE_POST_MOVE
    rs = RuleSet::parse(world);

    // Phase 4: TRANSFORM
    apply_transforms(world, rs, log);

    // Phase 5: PARSE_POST_TRANSFORM
    rs = RuleSet::parse(world);

    // Phase 6: DESTRUCT
    std::size_t const pre_destruct = log.size();
    apply_destructions(world, rs, log);

    // Phase 6.1: HAS (spawn on destruction, before APPLY_MAKE)
    apply_has(world, rs, log, pre_destruct);

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
