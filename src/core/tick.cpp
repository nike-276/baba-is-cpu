#include "tick.hpp"

#include "ruleset.hpp"

#include <algorithm>
#include <chrono>
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
    log.push_back(Change::destroy(id, o->pos, o->kind, o->original_kind, o->text, o->facing));
    world.destroy(id);
}

void do_retype(World& world, ObjectId id, Kind new_kind, std::vector<Change>& log) {
    Object const* o = world.get(id);
    if (!o) return;
    Kind old_kind = o->kind;
    world.retype(id, new_kind);
    if (old_kind != new_kind) log.push_back(Change::retype(id, old_kind, new_kind));
}

// IS TEXT: convert a non-text object to a text tile of the same kind.
// Skips objects that are already text, and skips if a text tile already
// occupies the same cell (coexistence guard).
bool do_flip_text(World& world, ObjectId id, std::vector<Change>& log) {
    Object const* o = world.get(id);
    if (!o || o->text) return false;
    for (ObjectId other : world.at(o->pos)) {
        if (other == id) continue;
        Object const* ob = world.get(other);
        if (ob && ob->text) return false;
    }
    log.push_back(Change::flip_text(id));
    world.flip_text(id);
    return true;
}

// TEXT IS NOUN: convert a text tile back to a non-text object of target_kind.
// Retypes if target_kind differs from the current kind.
bool do_unflip_text(World& world, ObjectId id, Kind target_kind, std::vector<Change>& log) {
    Object const* o = world.get(id);
    if (!o || !o->text) return false;
    Kind old_kind = o->kind;
    log.push_back(Change::flip_text(id));
    world.flip_text(id);
    if (target_kind != Kind::None && target_kind != old_kind)
        do_retype(world, id, target_kind, log);
    return true;
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
    static constexpr std::pair<Kind, Direction> kPasses[] = {
        {Kind::P_Left,  Direction::Left},
        {Kind::P_Right, Direction::Right},
        {Kind::P_Up,    Direction::Up},
        {Kind::P_Down,  Direction::Down},
    };
    // Union of subject kinds across all four directional properties; iterate
    // only these kinds via the World index.
    std::unordered_set<Kind> dir_kinds;
    for (auto [prop, _] : kPasses)
        for (Kind k : rs.subjects_for_property(prop)) dir_kinds.insert(k);
    if (dir_kinds.empty()) return;

    for (Kind k : dir_kinds) {
        // Snapshot id list because do_face doesn't mutate buckets, but
        // copying is cheap and defensive.
        std::vector<ObjectId> ids = world.objects_of_kind(k);
        for (ObjectId id : ids) {
            if      (rs.object_has_property(world, id, Kind::P_Left))  do_face(world, id, Direction::Left,  log);
            else if (rs.object_has_property(world, id, Kind::P_Right)) do_face(world, id, Direction::Right, log);
            else if (rs.object_has_property(world, id, Kind::P_Up))    do_face(world, id, Direction::Up,    log);
            else if (rs.object_has_property(world, id, Kind::P_Down))  do_face(world, id, Direction::Down,  log);
        }
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
    // Union of subject kinds across MOVE and AUTO rules.
    std::unordered_set<Kind> auto_kinds;
    for (Kind k : rs.subjects_for_property(Kind::P_Move)) auto_kinds.insert(k);
    for (Kind k : rs.subjects_for_property(Kind::P_Auto)) auto_kinds.insert(k);
    if (auto_kinds.empty()) return;

    struct Mover { ObjectId id; Coord step_v; bool flip_on_block; };
    std::vector<Mover> movers;

    for (Kind k : auto_kinds) {
        for (ObjectId id : world.objects_of_kind(k)) {
            Object const* o = world.get(id);
            if (!o) continue;
            if (rs.object_has_property(world, id, Kind::P_Still)) continue;
            // FALL overrides MOVE/AUTO: falling objects are handled in apply_fall.
            if (rs.object_has_property(world, id, Kind::P_Fall)      ||
                rs.object_has_property(world, id, Kind::P_Fallup)    ||
                rs.object_has_property(world, id, Kind::P_Fallleft)  ||
                rs.object_has_property(world, id, Kind::P_Fallright)) continue;
            if (rs.object_has_property(world, id, Kind::P_Move))
                movers.push_back({id, step(o->facing), true});
            else if (rs.object_has_property(world, id, Kind::P_Auto))
                movers.push_back({id, step(o->facing), false});
        }
    }

    // Determinism: movers were appended in kind-hash order (auto_kinds is
    // unordered_set). Sort by id so execution order matches the historical
    // all_ids()-based iteration.
    std::sort(movers.begin(), movers.end(),
              [](Mover const& a, Mover const& b) { return a.id < b.id; });

    for (auto const& m : movers) {
        Object const* o = world.get(m.id);
        if (!o) continue;
        if (!try_move(world, m.id, m.step_v, rs, log)) {
            if (m.flip_on_block) do_face(world, m.id, opposite(o->facing), log);
        }
    }
}

// ── APPLY_FALL phase ──────────────────────────────────────────────────────────
// FALL* runs after TRANSFORM so that REVERT (and other IS transforms) resolve
// before objects slide. Wiki Order of Operations: fallblock() follows
// domovement()+REVERT+IS transforms.

void apply_fall(World& world, RuleSet const& rs, std::vector<Change>& log) {
    static constexpr Kind kFallProps[] = {
        Kind::P_Fall, Kind::P_Fallup, Kind::P_Fallleft, Kind::P_Fallright,
    };
    std::unordered_set<Kind> fall_kinds;
    for (Kind p : kFallProps)
        for (Kind k : rs.subjects_for_property(p)) fall_kinds.insert(k);
    if (fall_kinds.empty()) return;

    struct Faller { ObjectId id; Coord step_v; };
    std::vector<Faller> fallers;

    for (Kind k : fall_kinds) {
        for (ObjectId id : world.objects_of_kind(k)) {
            if (rs.object_has_property(world, id, Kind::P_Still)) continue;
            if (rs.object_has_property(world, id, Kind::P_Fall))
                fallers.push_back({id, step(Direction::Down)});
            else if (rs.object_has_property(world, id, Kind::P_Fallup))
                fallers.push_back({id, step(Direction::Up)});
            else if (rs.object_has_property(world, id, Kind::P_Fallleft))
                fallers.push_back({id, step(Direction::Left)});
            else if (rs.object_has_property(world, id, Kind::P_Fallright))
                fallers.push_back({id, step(Direction::Right)});
        }
    }

    std::sort(fallers.begin(), fallers.end(),
              [](Faller const& a, Faller const& b) { return a.id < b.id; });

    for (auto const& f : fallers) {
        if (!world.get(f.id)) continue;
        fall_slide(world, f.id, f.step_v, rs, log);
    }
}

// ── APPLY_SHIFT phase ─────────────────────────────────────────────────────
// SHIFT objects push all co-located non-text, non-SHIFT objects.
// Stacking rule: a target on multiple SHIFT tiles only ever moves in ONE
// direction per SHIFT substage. The first shifter to touch it (ascending
// shifter id) locks the direction; every additional shifter on the same
// target — regardless of its own facing — just increments the movement
// count. The target then try_moves `count` times in the locked direction,
// respecting STOP/PUSH (same try_move semantics as MOVE).

void apply_shift(World& world, RuleSet const& rs, std::vector<Change>& log) {
    auto const& shift_subjects = rs.subjects_for_property(Kind::P_Shift);
    if (shift_subjects.empty()) return;

    std::vector<ObjectId> shifters;
    for (Kind k : shift_subjects) {
        for (ObjectId id : world.objects_of_kind(k)) {
            if (rs.object_has_property(world, id, Kind::P_Shift))
                shifters.push_back(id);
        }
    }
    std::sort(shifters.begin(), shifters.end());
    if (shifters.empty()) return;

    // Accumulate per-target plan. Using std::map keeps execution order
    // deterministic (ascending target id).
    struct Plan { Direction dir; int count; };
    std::map<ObjectId, Plan> plans;

    for (ObjectId sid : shifters) {
        Object const* s = world.get(sid);
        if (!s) continue;
        Coord pos = s->pos;
        Direction sdir = s->facing;
        for (ObjectId tid : world.at(pos)) {
            if (tid == sid) continue;
            Object const* o = world.get(tid);
            if (!o) continue;
            if (rs.object_has_property(world, tid, Kind::P_Shift)) continue;
            auto it = plans.find(tid);
            if (it == plans.end()) plans.emplace(tid, Plan{sdir, 1});
            else it->second.count++;
        }
    }

    for (auto const& [tid, plan] : plans) {
        if (!world.get(tid)) continue;
        Coord step_v = step(plan.dir);
        // SHIFT sets facing once; subsequent failed moves don't unset it.
        do_face(world, tid, plan.dir, log);
        for (int i = 0; i < plan.count; ++i) {
            if (!try_move(world, tid, step_v, rs, log)) break;
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
    if (initial_swap.empty()) return;

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
    bool any = !rs.make_rules().empty() || !rs.conditional_make_rules().empty()
             || !rs.global_condition_make_rules().empty();
    if (!any) return;

    // Unconditional MAKE rules: iterate subjects via the World kind index.
    // Snapshot the id list because do_spawn mutates kind_index_.
    // Dedup cells so multiple FROM objects on the same tile only spawn once,
    // matching the old apply_make pre-CP-2 behaviour (which de-facto dedup'd
    // via the has_to check).
    for (auto const& mr : rs.make_rules()) {
        std::vector<ObjectId> subjects = world.objects_of_kind(mr.from);
        std::unordered_set<Coord, CoordHash> seen;
        for (ObjectId id : subjects) {
            Object const* o = world.get(id);
            if (!o) continue;
            Coord c = o->pos;
            if (!seen.insert(c).second) continue;
            // Match pre-CP-2 behaviour: src_facing = last FROM on cell (in
            // cell-insertion order from world.at), target pre-existence check.
            Direction src_facing = Direction::Right;
            bool has_to = false;
            for (ObjectId other : world.at(c)) {
                Object const* ob = world.get(other);
                if (!ob || ob->text) continue;
                if (ob->kind == mr.from) src_facing = ob->facing;
                if (ob->kind == mr.to)   has_to = true;
            }
            if (!has_to)
                do_spawn(world, c, mr.to, /*text=*/false, src_facing, log);
        }
    }

    // Conditional MAKE rules (ON/FACEDBY/NOT): iterate only objects of the subject kind.
    for (auto const& cmr : rs.conditional_make_rules()) {
        std::vector<ObjectId> subjects = world.objects_of_kind(cmr.subject);
        for (ObjectId id : subjects) {
            Object const* o = world.get(id);
            if (!o) continue;
            if (!RuleSet::eval_cond_clauses(world, id, cmr.clauses)) continue;
            bool already = false;
            for (ObjectId other : world.at(o->pos)) {
                Object const* ob = world.get(other);
                if (ob && !ob->text && ob->kind == cmr.target) { already = true; break; }
            }
            if (!already) do_spawn(world, o->pos, cmr.target, false, o->facing, log);
        }
    }

    // Global condition MAKE rules: [NOT] POWEREDx NOUN [ON …] MAKE NOUN
    for (auto const& gcmr : rs.global_condition_make_rules()) {
        bool all_met = true;
        for (auto const& cond : gcmr.conditions) {
            bool pw_exists = rs.any_has_power_kind(world, cond.power_kind);
            if (cond.negated ? pw_exists : !pw_exists) { all_met = false; break; }
        }
        if (!all_met) continue;

        std::vector<ObjectId> subjects = world.objects_of_kind(gcmr.subject);
        bool has_on = !gcmr.on_clauses.empty();
        for (ObjectId id : subjects) {
            Object const* o = world.get(id);
            if (!o) continue;
            if (has_on && !RuleSet::eval_cond_clauses(world, id, gcmr.on_clauses)) continue;
            bool already = false;
            for (ObjectId oid : world.at(o->pos)) {
                Object const* ob = world.get(oid);
                if (ob && !ob->text && ob->kind == gcmr.target) { already = true; break; }
            }
            if (!already) do_spawn(world, o->pos, gcmr.target, false, o->facing, log);
        }
    }
}

// ── TRANSFORM phase ────────────────────────────────────────────────────────

void apply_transforms(World& world, RuleSet const& rs, std::vector<Change>& log) {
    auto const& transforms = rs.transform_rules();
    if (transforms.empty() && rs.conditional_transform_rules().empty() && rs.facing_transform_rules().empty()) return;

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
    if (xmap.empty() && rs.conditional_transform_rules().empty() && rs.facing_transform_rules().empty()) return;

    struct XEntry { ObjectId id; Coord pos; Direction facing; std::vector<Kind> targets; Kind original_kind{Kind::None}; };
    std::vector<XEntry> pending;
    for (ObjectId id : world.all_ids()) {
        Object const* o = world.get(id);
        if (!o) continue;
        // Text objects match the N_Text subject; non-text objects match their own kind.
        Kind lookup = o->text ? Kind::N_Text : o->kind;
        auto it = xmap.find(lookup);
        if (it != xmap.end()) {
            pending.push_back({id, o->pos, o->facing, it->second, o->original_kind});
        }
    }

    // Conditional transforms (NOUN ON/FACEDBY NOUN IS NOUN [AND NOUN]*): check per-object.
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
                if (RuleSet::eval_cond_clauses(world, id, ctr.clauses))
                    cond_targets[id].push_back(ctr.target);
            }
        }
        for (auto& [id, targets] : cond_targets) {
            // Sort + dedup handles duplicates from multiple scan starting positions.
            std::sort(targets.begin(), targets.end());
            targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
            Object const* o = world.get(id);
            if (o) pending.push_back({id, o->pos, o->facing, targets, o->original_kind});
        }
    }

    // Facing transforms (NOUN [NOT] FACING <cond> IS NOUN).
    {
        auto facing_match = [](Object const* o, Kind cond, World const& w) -> bool {
            bool cond_is_dir = (cond == Kind::P_Left || cond == Kind::P_Right ||
                                cond == Kind::P_Up   || cond == Kind::P_Down);
            if (cond_is_dir) {
                Direction req = cond == Kind::P_Left  ? Direction::Left  :
                                cond == Kind::P_Right ? Direction::Right :
                                cond == Kind::P_Up    ? Direction::Up    :
                                                        Direction::Down;
                return o->facing == req;
            }
            Coord fp = {o->pos.x + step(o->facing).x, o->pos.y + step(o->facing).y};
            for (ObjectId oid : w.at(fp)) {
                Object const* ob = w.get(oid);
                if (ob && !ob->text && ob->kind == cond) return true;
            }
            return false;
        };
        std::map<ObjectId, std::vector<Kind>> ftargets;
        for (ObjectId id : world.all_ids()) {
            Object const* o = world.get(id);
            if (!o || o->text) continue;
            for (auto const& ftr : rs.facing_transform_rules()) {
                if (ftr.subject != o->kind) continue;
                bool matched = facing_match(o, ftr.condition, world);
                if (ftr.negated ? !matched : matched)
                    ftargets[id].push_back(ftr.target);
            }
        }
        for (auto& [id, targets] : ftargets) {
            std::sort(targets.begin(), targets.end());
            targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
            Object const* o = world.get(id);
            if (o) pending.push_back({id, o->pos, o->facing, targets, o->original_kind});
        }
    }

    // REVERT: override any pending transform for objects that have REVERT and whose
    // original_kind differs from their current kind. X IS X disables REVERT.
    {
        std::unordered_set<Kind> self_kinds;
        for (auto const& tr : rs.transform_rules())
            if (tr.from == tr.to) self_kinds.insert(tr.from);

        // Returns true if kind k has P_Revert via a non-negated property rule.
        auto kind_has_revert = [&](Kind k) -> bool {
            for (auto const& pr : rs.property_rules())
                if (pr.subject == k && pr.property == Kind::P_Revert && !pr.negated)
                    return true;
            return false;
        };

        std::map<ObjectId, size_t> idx;
        for (size_t i = 0; i < pending.size(); ++i) idx[pending[i].id] = i;

        // Phase A: pending transforms whose target kind has REVERT — intercept them
        // so the object reverts to original_kind instead of becoming the target.
        for (size_t i = 0; i < pending.size(); ++i) {
            auto& entry = pending[i];
            if (entry.targets.size() != 1) continue;
            Kind target = entry.targets[0];
            if (!kind_has_revert(target)) continue;
            if (self_kinds.count(target)) continue;
            Object const* o = world.get(entry.id);
            if (!o || o->text) continue;
            if (o->original_kind == target) continue; // already at original, no-op
            entry.targets = {o->original_kind};
        }

        // Phase B: objects currently a reverting kind not covered by a pending transform.
        if (rs.any_grants(Kind::P_Revert)) {
            std::unordered_set<Kind> revert_subjects;
            for (auto const& r : rs.property_rules())
                if (!r.negated && r.property == Kind::P_Revert) revert_subjects.insert(r.subject);
            for (auto const& r : rs.conditional_rules())
                if (r.property == Kind::P_Revert) revert_subjects.insert(r.subject);
            for (auto const& r : rs.facing_rules())
                if (r.property == Kind::P_Revert) revert_subjects.insert(r.subject);
            for (auto const& r : rs.global_condition_property_rules())
                if (r.property == Kind::P_Revert) revert_subjects.insert(r.subject);

            for (ObjectId id : world.all_ids()) {
                Object const* o = world.get(id);
                if (!o || o->text) continue;
                if (!revert_subjects.count(o->kind)) continue;
                if (!rs.object_has_property(world, id, Kind::P_Revert)) continue;
                if (self_kinds.count(o->kind)) continue;
                if (o->original_kind == o->kind) continue;
                XEntry e{id, o->pos, o->facing, {o->original_kind}};
                auto it = idx.find(id);
                if (it != idx.end()) pending[it->second] = e;
                else { idx[id] = pending.size(); pending.push_back(e); }
            }
        }
    }

    for (auto const& e : pending) {
        Object const* o = world.get(e.id);
        if (!o) continue;

        if (o->text) {
            // TEXT IS NOUN: unflip text tiles to non-text objects.
            if (e.targets.size() == 1) {
                Kind t = e.targets[0];
                if (t != Kind::N_Text)
                    do_unflip_text(world, e.id, t, log);
                // TEXT IS TEXT: X IS X protection removed this from xmap; no-op here.
            } else {
                // Multi-target TEXT IS A AND B: destroy and respawn as non-text objects.
                do_destroy(world, e.id, log);
                for (Kind t : e.targets) {
                    if (t != Kind::N_Text) {
                        ObjectId nid = do_spawn(world, e.pos, t, /*text=*/false, e.facing, log);
                        if (e.original_kind != Kind::None)
                            world.set_original_kind(nid, e.original_kind);
                    }
                }
            }
        } else if (e.targets.size() == 1) {
            if (e.targets[0] == Kind::N_Text)
                do_flip_text(world, e.id, log);
            else
                do_retype(world, e.id, e.targets[0], log);
        } else {
            // Multi-target: destroy and respawn. If any target is N_Text, flip
            // instead of spawning a fresh object with kind N_Text.
            bool has_text_target = false;
            for (Kind t : e.targets) if (t == Kind::N_Text) { has_text_target = true; break; }
            if (has_text_target && e.targets.size() == 2) {
                // e.g. BABA IS TEXT AND WALL — flip text flag + retype to other target.
                Kind other_kind = Kind::None;
                for (Kind t : e.targets) if (t != Kind::N_Text) other_kind = t;
                do_flip_text(world, e.id, log);
                if (other_kind != Kind::None) {
                    ObjectId nid = do_spawn(world, e.pos, other_kind, /*text=*/false, e.facing, log);
                    if (e.original_kind != Kind::None)
                        world.set_original_kind(nid, e.original_kind);
                }
            } else {
                do_destroy(world, e.id, log);
                for (Kind target : e.targets) {
                    if (target != Kind::N_Text) {
                        ObjectId nid = do_spawn(world, e.pos, target, /*text=*/false, e.facing, log);
                        if (e.original_kind != Kind::None)
                            world.set_original_kind(nid, e.original_kind);
                    }
                    // N_Text in a multi-target: skip (flip not meaningful when destroying)
                }
            }
        }
    }
}

// ── DESTRUCT phase ─────────────────────────────────────────────────────────

void apply_destructions(World& world, RuleSet const& rs, std::vector<Change>& log) {
    // Inline HAS emission (CP-11). After each destruction substage, spawn every
    // HAS target for the non-text objects destroyed in THIS substage, before
    // the next substage runs. `mark` is the log index taken immediately before
    // the substage's do_destroy loop.
    auto emit_has_since = [&](std::size_t mark) {
        if (rs.has_rules().empty()) return;
        std::size_t n = log.size();
        for (std::size_t i = mark; i < n; ++i) {
            Change const c = log[i];  // copy: do_spawn may reallocate log
            if (c.kind != ChangeKind::Destroy) continue;
            if (c.obj_text) continue;
            for (auto const& hr : rs.has_rules()) {
                if (hr.subject != c.obj_kind) continue;
                do_spawn(world, c.obj_pos, hr.target, /*text=*/false, c.obj_facing, log);
            }
        }
    };

    auto destroy_and_has = [&](std::unordered_set<ObjectId> const& ids) {
        if (ids.empty()) return;
        std::size_t mark = log.size();
        std::vector<ObjectId> sorted(ids.begin(), ids.end());
        std::sort(sorted.begin(), sorted.end());
        for (ObjectId id : sorted) do_destroy(world, id, log);
        emit_has_since(mark);
    };

    // Helper: collect candidate cells for a given property by scanning rule
    // subjects via the World kind index.
    auto candidate_cells = [&](Kind prop, std::unordered_set<Coord, CoordHash>& out_cells) {
        for (Kind subject : rs.subjects_for_property(prop)) {
            for (ObjectId id : world.objects_of_kind(subject)) {
                Object const* o = world.get(id);
                if (o) out_cells.insert(o->pos);
            }
        }
    };

    // Helper: dedup'd cells where any non-text object of `subject` lives.
    auto cells_of_kind = [&](Kind subject, std::unordered_set<Coord, CoordHash>& out_cells) {
        for (ObjectId id : world.objects_of_kind(subject)) {
            Object const* o = world.get(id);
            if (o) out_cells.insert(o->pos);
        }
    };

    // a. SINK — only check cells that contain an object whose kind can be SINK.
    if (rs.any_grants(Kind::P_Sink)) {
        std::unordered_set<Coord, CoordHash> sink_cells;
        candidate_cells(Kind::P_Sink, sink_cells);
        std::unordered_set<ObjectId> doomed;
        for (Coord c : sink_cells) {
            auto const& cell = world.at(c);
            if (cell.size() < 2) continue;
            bool any_sink = false;
            for (ObjectId id : cell)
                if (rs.object_has_property(world, id, Kind::P_Sink)) { any_sink = true; break; }
            if (any_sink)
                for (ObjectId id : cell) doomed.insert(id);
        }
        destroy_and_has(doomed);
    }

    // b. EAT — NOUN EAT NOUN: for each rule, destroy target-kind objects on tiles
    //    that also contain a subject-kind object. Subject survives.
    //    Self-eat (subject == target): all die if ≥2 copies on tile; nothing if only 1.
    {
        std::unordered_set<ObjectId> doomed;
        auto eat_on_tile = [&](Coord c, Kind eater, Kind target) {
            if (eater == target) {
                // Self-eat: all die when there are ≥2 on the tile.
                std::vector<ObjectId> same;
                for (ObjectId id : world.at(c)) {
                    Object const* o = world.get(id);
                    if (o && !o->text && o->kind == eater) same.push_back(id);
                }
                if (same.size() >= 2)
                    for (ObjectId id : same) doomed.insert(id);
            } else {
                bool has_eater = false;
                for (ObjectId id : world.at(c)) {
                    Object const* o = world.get(id);
                    if (o && !o->text && o->kind == eater) { has_eater = true; break; }
                }
                if (!has_eater) return;
                for (ObjectId id : world.at(c)) {
                    Object const* o = world.get(id);
                    if (o && !o->text && o->kind == target) doomed.insert(id);
                }
            }
        };

        // Iterate only cells containing the subject kind instead of all_cells.
        for (auto const& er : rs.eat_rules()) {
            std::unordered_set<Coord, CoordHash> cells;
            cells_of_kind(er.subject, cells);
            for (Coord c : cells)
                eat_on_tile(c, er.subject, er.target);
        }

        for (auto const& er : rs.conditional_eat_rules()) {
            std::unordered_set<Coord, CoordHash> cells;
            cells_of_kind(er.subject, cells);
            for (Coord c : cells) {
                // At least one subject object must satisfy all ON/FACEDBY clauses.
                bool condition_met = false;
                for (ObjectId id : world.at(c)) {
                    Object const* o = world.get(id);
                    if (!o || o->text || o->kind != er.subject) continue;
                    if (RuleSet::eval_cond_clauses(world, id, er.clauses)) {
                        condition_met = true; break;
                    }
                }
                if (!condition_met) continue;
                eat_on_tile(c, er.subject, er.target);
            }
        }

        destroy_and_has(doomed);
    }

    // b2. POWERED [ON] EAT — [NOT] POWEREDx NOUN [ON/NOT ON NOUN]* EAT NOUN
    {
        std::unordered_set<ObjectId> doomed;
        for (auto const& gcer : rs.global_condition_eat_rules()) {
            // Check power conditions first (cheap global check).
            bool all_met = true;
            for (auto const& cond : gcer.conditions) {
                bool pw_exists = rs.any_has_power_kind(world, cond.power_kind);
                if (cond.negated ? pw_exists : !pw_exists) { all_met = false; break; }
            }
            if (!all_met) continue;

            Kind eater = gcer.subject;
            Kind target = gcer.target;
            bool has_on = !gcer.on_clauses.empty();

            // Iterate only cells containing the eater kind.
            std::unordered_set<Coord, CoordHash> cells;
            cells_of_kind(eater, cells);
            for (Coord c : cells) {
                // For ON-conditioned rules, at least one eater on this tile must
                // satisfy all ON clauses (same logic as ConditionalEatRule).
                if (has_on) {
                    bool condition_met = false;
                    for (ObjectId id : world.at(c)) {
                        Object const* o = world.get(id);
                        if (!o || o->text || o->kind != eater) continue;
                        if (RuleSet::eval_cond_clauses(world, id, gcer.on_clauses)) {
                            condition_met = true; break;
                        }
                    }
                    if (!condition_met) continue;
                }

                if (eater == target) {
                    std::vector<ObjectId> same;
                    for (ObjectId id : world.at(c)) {
                        Object const* o = world.get(id);
                        if (o && !o->text && o->kind == eater) same.push_back(id);
                    }
                    if (same.size() >= 2)
                        for (ObjectId id : same) doomed.insert(id);
                } else {
                    bool has_eater = false;
                    for (ObjectId id : world.at(c)) {
                        Object const* o = world.get(id);
                        if (o && !o->text && o->kind == eater) { has_eater = true; break; }
                    }
                    if (!has_eater) continue;
                    for (ObjectId id : world.at(c)) {
                        Object const* o = world.get(id);
                        if (o && !o->text && o->kind == target) doomed.insert(id);
                    }
                }
            }
        }
        destroy_and_has(doomed);
    }

    // c. HOT / MELT — only check cells with a potential HOT-kind object.
    if (rs.any_grants(Kind::P_Hot) && rs.any_grants(Kind::P_Melt)) {
        std::unordered_set<Coord, CoordHash> hot_cells;
        candidate_cells(Kind::P_Hot, hot_cells);
        std::unordered_set<ObjectId> doomed;
        for (Coord c : hot_cells) {
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
        destroy_and_has(doomed);
    }

    // d. WEAK (CP-11) — static overlap: any WEAK object sharing a tile with
    // another non-text object is destroyed, regardless of whether a move
    // caused the overlap. Iterate WEAK candidates via the kind index.
    if (rs.any_grants(Kind::P_Weak)) {
        std::unordered_set<ObjectId> doomed;
        for (Kind k : rs.subjects_for_property(Kind::P_Weak)) {
            for (ObjectId id : world.objects_of_kind(k)) {
                if (!rs.object_has_property(world, id, Kind::P_Weak)) continue;
                Object const* o = world.get(id);
                if (!o) continue;
                bool has_other = false;
                for (ObjectId oid : world.at(o->pos)) {
                    if (oid == id) continue;
                    Object const* ob = world.get(oid);
                    if (ob && !ob->text) { has_other = true; break; }
                }
                if (has_other) doomed.insert(id);
            }
        }
        destroy_and_has(doomed);
    }

    // e. DEFEAT — only check cells with a potential DEFEAT-kind object.
    if (rs.any_grants(Kind::P_Defeat) && rs.any_grants(Kind::P_You)) {
        std::unordered_set<Coord, CoordHash> defeat_cells;
        candidate_cells(Kind::P_Defeat, defeat_cells);
        std::unordered_set<ObjectId> doomed;
        for (Coord c : defeat_cells) {
            auto const& cell = world.at(c);
            bool any_defeat = false;
            for (ObjectId id : cell)
                if (rs.object_has_property(world, id, Kind::P_Defeat)) { any_defeat = true; break; }
            if (any_defeat)
                for (ObjectId id : cell)
                    if (rs.object_has_property(world, id, Kind::P_You)) doomed.insert(id);
        }
        destroy_and_has(doomed);
    }

    // f. OPEN / SHUT — only check cells with a potential OPEN-kind object.
    if (rs.any_grants(Kind::P_Open) && rs.any_grants(Kind::P_Shut)) {
        std::unordered_set<Coord, CoordHash> open_cells;
        candidate_cells(Kind::P_Open, open_cells);
        std::unordered_set<ObjectId> doomed;
        for (Coord c : open_cells) {
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
        destroy_and_has(doomed);
    }
}

// ── CHECK_WIN ──────────────────────────────────────────────────────────────

bool check_win(World const& world, RuleSet const& rs) {
    if (!rs.any_grants(Kind::P_You) || !rs.any_grants(Kind::P_Win)) return false;
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

// ── APPLY_HAS (obsolete, kept as a stub) ───────────────────────────────────
// CP-11 folded HAS into apply_destructions as an inline emit_has_since()
// helper that fires after EACH destruction substage instead of once at end.
// This free function is no longer called; retained only so existing external
// callers (if any — none at time of writing) continue to compile.
[[maybe_unused]] void apply_has(World& world, RuleSet const& rs,
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

// ── APPLY_NUDGE (phase 2.53) ───────────────────────────────────────────────
// Four sub-passes R→U→L→D. Each pass moves all objects with that NUDGE property
// one tile in the named direction. Does not change facing on block (unlike MOVE).

void apply_nudge(World& world, RuleSet const& rs, std::vector<Change>& log) {
    static constexpr Kind kNudgeProps[] = {
        Kind::P_Nudgeright, Kind::P_Nudgeup,
        Kind::P_Nudgeleft,  Kind::P_Nudgedown,
    };
    auto is_nudge = [](Kind p) {
        for (Kind n : kNudgeProps) if (p == n) return true;
        return false;
    };
    bool any = false;
    for (auto const& r : rs.property_rules())
        if (!r.negated && is_nudge(r.property)) { any = true; break; }
    if (!any)
        for (auto const& r : rs.conditional_rules())
            if (is_nudge(r.property)) { any = true; break; }
    if (!any)
        for (auto const& r : rs.global_condition_property_rules())
            if (is_nudge(r.property)) { any = true; break; }
    if (!any) return;

    static constexpr std::pair<Kind, Direction> kPasses[] = {
        {Kind::P_Nudgeright, Direction::Right},
        {Kind::P_Nudgeup,    Direction::Up},
        {Kind::P_Nudgeleft,  Direction::Left},
        {Kind::P_Nudgedown,  Direction::Down},
    };
    for (auto [prop, dir] : kPasses) {
        std::vector<ObjectId> movers;
        for (ObjectId id : world.all_ids()) {
            Object const* o = world.get(id);
            if (!o || o->text) continue;
            if (rs.object_has_property(world, id, Kind::P_Still)) continue;
            if (rs.object_has_property(world, id, prop)) movers.push_back(id);
        }
        Coord sv = step(dir);
        for (ObjectId id : movers) {
            if (!world.get(id)) continue;
            try_move(world, id, sv, rs, log);
        }
    }
}

// ── APPLY_FEAR (phase 2.55) ────────────────────────────────────────────────
// Each FEAR subject moves away from any 4-directionally adjacent feared object.
// Direction priority relative to subject facing: forward→CW→CCW→backward.
// Skips directions that contain a feared object. Same-tile feared = no movement.

void apply_fear(World& world, RuleSet const& rs, std::vector<Change>& log) {
    if (rs.fear_rules().empty()) return;
    std::vector<ObjectId> candidates;
    for (ObjectId id : world.all_ids()) {
        Object const* o = world.get(id);
        if (!o || o->text) continue;
        for (auto const& fr : rs.fear_rules())
            if (fr.subject == o->kind) { candidates.push_back(id); break; }
    }
    auto cw  = [](Direction d) { return static_cast<Direction>((static_cast<int>(d) + 3) % 4); };
    auto ccw = [](Direction d) { return static_cast<Direction>((static_cast<int>(d) + 1) % 4); };
    auto opp = [](Direction d) { return static_cast<Direction>((static_cast<int>(d) + 2) % 4); };
    for (ObjectId id : candidates) {
        Object const* o = world.get(id);
        if (!o) continue;
        std::vector<Direction> feared_dirs;
        for (Direction d : {Direction::Right, Direction::Up, Direction::Left, Direction::Down}) {
            Coord adj = {o->pos.x + step(d).x, o->pos.y + step(d).y};
            bool found = false;
            for (ObjectId other : world.at(adj)) {
                Object const* ob = world.get(other);
                if (!ob || ob->text) continue;
                for (auto const& fr : rs.fear_rules()) {
                    if (fr.subject == o->kind && fr.target == ob->kind) {
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
            if (found) feared_dirs.push_back(d);
        }
        if (feared_dirs.empty()) continue;
        Direction fwd = o->facing;
        for (Direction esc : {fwd, cw(fwd), ccw(fwd), opp(fwd)}) {
            bool skip = false;
            for (Direction fd : feared_dirs) { if (fd == esc) { skip = true; break; } }
            if (skip) continue;
            if (try_move(world, id, step(esc), rs, log)) break;
        }
    }
}

// ── APPLY_PLAY (phase 7.5) ────────────────────────────────────────────────
// For each non-text object with a matching PlayRule, emit one SoundEvent.
// Iteration in ascending id order preserves determinism.

void apply_play(World const& world, RuleSet const& rs,
                std::vector<SoundEvent>& out) {
    if (rs.play_rules().empty()) return;
    for (ObjectId id : world.all_ids()) {
        Object const* o = world.get(id);
        if (!o || o->text) continue;
        for (auto const& pr : rs.play_rules()) {
            if (pr.subject != o->kind) continue;
            out.push_back({o->kind, pr.note, pr.octave, pr.sharp, pr.flat, o->pos});
        }
    }
}

}  // namespace

// ── apply_tick (9-phase pipeline) ─────────────────────────────────────────

// Wraps a block with high_resolution_clock measurements and accumulates into
// report.timings. Two Clock::now() calls per phase add ~30–60 ns of overhead.
#define PHASE_TIME(phase_enum, ...)                                             \
    do {                                                                        \
        auto _t0 = std::chrono::high_resolution_clock::now();                  \
        { __VA_ARGS__ }                                                         \
        auto _dt = (std::chrono::high_resolution_clock::now() - _t0).count();  \
        report.timings.ns[static_cast<int>(phase_enum)] += _dt;                \
        report.timings.total_ns                          += _dt;                \
    } while (0)

TickReport apply_tick(World& world, Input input) {
    using Clock = std::chrono::high_resolution_clock;
    TickReport report;
    std::vector<Change>& log = report.changes;

    // Phase 1: PARSE_INITIAL — timed manually because RuleSet has no default
    // constructor; it must be initialised by parse(), not assigned later.
    auto _parse1_t0 = Clock::now();
    RuleSet rs = RuleSet::parse(world);
    {
        auto _dt = (Clock::now() - _parse1_t0).count();
        report.timings.ns[static_cast<int>(Phase::ParseInitial)] += _dt;
        report.timings.total_ns += _dt;
    }

    // log index of the last completed parse; used by text_dirty to check only
    // changes that occurred after the most recent parse.
    std::size_t last_parse_mark = 0;

    // Returns true iff any change at log[from..end) could affect the active rule set.
    // Checked before each post-parse to skip re-parse when rules can't change.
    // Non-text objects with a WORD property act as text tiles during parsing, so
    // their movement also invalidates the cached rule set.
    auto text_dirty = [&](std::size_t from) -> bool {
        bool word_possible = rs.any_grants(Kind::P_Word);
        for (std::size_t i = from; i < log.size(); ++i) {
            auto const& c = log[i];
            if (c.kind == ChangeKind::FlipText) return true;
            if (c.kind == ChangeKind::Destroy && c.obj_text) return true;
            if (c.kind == ChangeKind::Move || c.kind == ChangeKind::Spawn) {
                Object const* o = world.get(c.id);
                if (!o) continue;
                if (o->text) return true;
                if (word_possible && rs.object_has_property(world, c.id, Kind::P_Word))
                    return true;
            }
        }
        return false;
    };

    // Snapshot SWAP objects before any movement phases.
    std::unordered_set<ObjectId> initial_swap;
    for (ObjectId id : world.all_ids()) {
        Object const* o = world.get(id);
        if (!o || o->text) continue;
        if (rs.object_has_property(world, id, Kind::P_Swap))
            initial_swap.insert(id);
    }

    // Phase 2: APPLY_INPUT
    PHASE_TIME(Phase::Input, {
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
    });

    // Phase 2.5: APPLY_AUTO_MOVE
    PHASE_TIME(Phase::AutoMove, {
        apply_auto_move(world, rs, log);
    });

    // Phase 2.53: APPLY_NUDGE
    PHASE_TIME(Phase::Nudge, {
        apply_nudge(world, rs, log);
    });

    // Phase 2.55: APPLY_FEAR
    PHASE_TIME(Phase::Fear, {
        apply_fear(world, rs, log);
    });

    // Phase 2.6: APPLY_SHIFT
    PHASE_TIME(Phase::Shift, {
        apply_shift(world, rs, log);
    });

    // Phase 2.7: APPLY_SWAP
    PHASE_TIME(Phase::Swap, {
        apply_swap(world, rs, initial_swap, log);
    });

    // Phase 3: PARSE_POST_MOVE — skip when no text object moved/spawned/destroyed
    // since ParseInitial; static rule tiles never change, saving ~700ms/tick.
    PHASE_TIME(Phase::ParsePostMove, {
        if (text_dirty(last_parse_mark)) {
            rs = RuleSet::parse(world);
            last_parse_mark = log.size();
        }
    });

    // Phase 4: TRANSFORM
    PHASE_TIME(Phase::Transform, {
        apply_transforms(world, rs, log);
    });

    // Phase 5: PARSE_POST_TRANSFORM — skip when no text object changed since last parse.
    PHASE_TIME(Phase::ParsePostTransform, {
        if (text_dirty(last_parse_mark)) {
            rs = RuleSet::parse(world);
            last_parse_mark = log.size();
        }
    });

    // Phase 5.5: APPLY_FALL — after TRANSFORM so REVERT resolves before objects slide.
    PHASE_TIME(Phase::Fall, {
        apply_fall(world, rs, log);
    });

    // Phase 5.6: APPLY_DIRECTIONAL (statusblock) — wiki: LEFT/UP/RIGHT/DOWN run in
    // statusblock(), which is after the first fallblock() and before destruct.
    PHASE_TIME(Phase::Directional, {
        apply_directional(world, rs, log);
    });

    // ── Stage 5 — Block (CP-11) ──────────────────────────────────────────
    // Play fires first (before any destructions). Each destruction substage
    // inside apply_destructions triggers its paired HAS inline. Make fires
    // after all destructions. A single post-Block reparse closes the stage,
    // then CheckWin.

    // 5a. PLAY
    PHASE_TIME(Phase::Play, {
        apply_play(world, rs, report.sound_events);
    });

    // 5b..5g. DESTRUCT (SINK → EAT → HOT/MELT → WEAK → DEFEAT → OPEN/SHUT),
    // each paired with inline HAS emission.
    PHASE_TIME(Phase::Destruct, {
        apply_destructions(world, rs, log);
    });

    // 5h. MAKE
    PHASE_TIME(Phase::Make, {
        apply_make(world, rs, log);
    });

    // ParsePostBlock.
    PHASE_TIME(Phase::ParsePostDestruct, {
        if (text_dirty(last_parse_mark)) {
            rs = RuleSet::parse(world);
            last_parse_mark = log.size();
        }
    });

    // 5i. CHECK_WIN — final substage of Block.
    PHASE_TIME(Phase::CheckWin, {
        report.won = check_win(world, rs);
    });

    // Phase 9: COMMIT — changes are already in report.changes
    return report;
}

#undef PHASE_TIME

}  // namespace baba::core
