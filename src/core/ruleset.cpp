#include "ruleset.hpp"
#include "direction.hpp"

#include <algorithm>
#include <set>
#include <unordered_set>

namespace baba::core {

namespace {

// Read a single text kind at coord, or std::nullopt if the cell has no text.
std::optional<Kind> text_kind_at(World const& world, Coord c) {
    auto const& cell = world.at(c);
    for (ObjectId id : cell) {
        Object const* obj = world.get(id);
        if (obj && obj->text) return obj->kind;
    }
    return std::nullopt;
}

// Try to parse a rule strip starting at `start`, stepping by `step_dir`.
// Handles:
//   NOUN [AND NOUN]* IS [NOT] PROPERTY [AND [NOT] PROPERTY]*
//   NOUN [AND NOUN]* IS NOUN   (single-target transform; AND-noun predicates deferred)
//   NOT NOUN IS PROPERTY       (NOT-as-subject: applies P to everything except NOUN)
//
// Each parsed PropertyRule carries a `negated` flag for "X IS NOT P" forms.
// NOT-as-subject ("NOT X IS P") produces PropertyRules with negated=false but
// a special N_Not_Subject encoding — handled post-parse (deferred to full NOT pass).
// v1: we implement predicate-NOT cancellation only.
void scan_strip(World const& world, Coord start, Coord step_dir,
                std::vector<PropertyRule>& prop_out,
                std::vector<TransformRule>& xform_out,
                std::vector<MakeRule>& make_out,
                std::vector<EatRule>& eat_out,
                std::vector<ConditionalPropertyRule>& cond_out,
                std::vector<ConditionalTransformRule>& cond_xform_out,
                std::vector<ConditionalMakeRule>& cond_make_out,
                std::vector<FacingPropertyRule>& facing_out) {
    auto at = [&](Coord c) { return text_kind_at(world, c); };
    auto adv = [&](Coord c) -> Coord { return {c.x + step_dir.x, c.y + step_dir.y}; };
    auto bak = [&](Coord c) -> Coord { return {c.x - step_dir.x, c.y - step_dir.y}; };

    // Guard: don't start a rule from inside an AND-chained noun list.
    // This covers two cases:
    //   subject chains: "BABA AND KEKE IS P" — don't start from KEKE
    //   condition chains: "X ON A AND B IS P" — don't start from B
    // Walk backwards through (noun AND)* chains; if O_On is reached we're in
    // a condition list; if we walked any AND-noun pair without finding O_On
    // we're in a subject list. Either way, skip — the rule was already parsed
    // starting from the first noun.
    {
        Coord cur = bak(start);
        bool walked_and_chain = false;
        while (true) {
            auto k = at(cur);
            if (!k) break;
            if (*k == Kind::O_On || *k == Kind::O_Facing) return;
            if (*k != Kind::O_And) break;
            Coord noun_pos = bak(cur);
            auto nk = at(noun_pos);
            if (!nk || !is_noun(*nk)) break;
            walked_and_chain = true;
            cur = bak(noun_pos);
        }
        if (walked_and_chain) return;
    }

    Coord cursor = start;
    auto first = at(cursor);
    if (!first || !is_noun(*first)) return;

    std::vector<Kind> subjects;
    subjects.push_back(*first);
    cursor = adv(cursor);

    // Optional AND NOUN repetition (subjects).
    while (true) {
        auto k = at(cursor);
        if (!k || *k != Kind::O_And) break;
        Coord next = adv(cursor);
        auto noun = at(next);
        if (!noun || !is_noun(*noun)) return;
        subjects.push_back(*noun);
        cursor = adv(next);
    }

    // Operator: IS, ON, or MAKE.
    auto op_tok = at(cursor);
    if (!op_tok) return;

    // ── NOUN MAKE NOUN [AND NOUN]* ──────────────────────────────────────────
    if (*op_tok == Kind::O_Make) {
        cursor = adv(cursor);
        auto target = at(cursor);
        if (target && is_noun(*target)) {
            std::vector<Kind> targets;
            targets.push_back(*target);
            cursor = adv(cursor);
            while (true) {
                auto k = at(cursor);
                if (!k || *k != Kind::O_And) break;
                Coord next = adv(cursor);
                auto nt = at(next);
                if (!nt || !is_noun(*nt)) break;
                targets.push_back(*nt);
                cursor = adv(next);
            }
            for (Kind n : subjects)
                for (Kind t : targets)
                    make_out.push_back({n, t});
        }
        return;
    }

    // ── NOUN EAT NOUN [AND NOUN]* ───────────────────────────────────────────
    if (*op_tok == Kind::O_Eat) {
        cursor = adv(cursor);
        auto target = at(cursor);
        if (target && is_noun(*target)) {
            std::vector<Kind> targets;
            targets.push_back(*target);
            cursor = adv(cursor);
            while (true) {
                auto k = at(cursor);
                if (!k || *k != Kind::O_And) break;
                Coord next = adv(cursor);
                auto nt = at(next);
                if (!nt || !is_noun(*nt)) break;
                targets.push_back(*nt);
                cursor = adv(next);
            }
            for (Kind n : subjects)
                for (Kind t : targets)
                    eat_out.push_back({n, t});
        }
        return;
    }

    // ── NOUN FACING <cond> IS [NOT] PROPERTY ──────────────────────────────
    // <cond> is either a noun (adjacent tile) or P_Left/Right/Up/Down (own facing).
    if (*op_tok == Kind::O_Facing) {
        cursor = adv(cursor);
        auto cond_k = at(cursor);
        if (!cond_k) return;
        bool cond_is_dir = (*cond_k == Kind::P_Left || *cond_k == Kind::P_Right ||
                            *cond_k == Kind::P_Up   || *cond_k == Kind::P_Down);
        if (!is_noun(*cond_k) && !cond_is_dir) return;
        Kind cond = *cond_k;
        cursor = adv(cursor);
        if (auto verb = at(cursor); !verb || *verb != Kind::O_Is) return;
        cursor = adv(cursor);
        auto pk = at(cursor);
        if (!pk) return;
        bool neg = false;
        if (*pk == Kind::O_Not) { neg = true; cursor = adv(cursor); pk = at(cursor); }
        if (!pk || !is_property(*pk)) return;
        for (Kind n : subjects)
            facing_out.push_back({n, cond, *pk, neg});
        return;
    }

    // ── NOUN ON NOUN [AND NOUN]* IS/MAKE PREDICATE ─────────────────────────
    if (*op_tok == Kind::O_On) {
        cursor = adv(cursor);
        auto cond_noun = at(cursor);
        if (!cond_noun || !is_noun(*cond_noun)) return;
        std::vector<Kind> conditions;
        conditions.push_back(*cond_noun);
        cursor = adv(cursor);
        // AND loop for additional condition nouns.
        while (true) {
            auto k = at(cursor);
            if (!k || *k != Kind::O_And) break;
            Coord next = adv(cursor);
            auto nt = at(next);
            if (!nt || !is_noun(*nt)) break;
            conditions.push_back(*nt);
            cursor = adv(next);
        }

        auto verb2 = at(cursor);
        if (!verb2) return;

        // NOUN ON … MAKE NOUN [AND NOUN]*
        if (*verb2 == Kind::O_Make) {
            cursor = adv(cursor);
            auto t0 = at(cursor);
            if (!t0 || !is_noun(*t0)) return;
            std::vector<Kind> targets;
            targets.push_back(*t0);
            cursor = adv(cursor);
            while (true) {
                auto k = at(cursor);
                if (!k || *k != Kind::O_And) break;
                Coord next = adv(cursor);
                auto nt = at(next);
                if (!nt || !is_noun(*nt)) break;
                targets.push_back(*nt);
                cursor = adv(next);
            }
            for (Kind n : subjects)
                for (Kind t : targets)
                    cond_make_out.push_back({n, conditions, t, false});
            return;
        }

        if (*verb2 != Kind::O_Is) return;
        cursor = adv(cursor);

        // Conditional transform: NOUN ON … IS NOUN [AND NOUN]*
        {
            auto pred = at(cursor);
            if (pred && is_noun(*pred)) {
                std::vector<Kind> targets;
                targets.push_back(*pred);
                cursor = adv(cursor);
                while (true) {
                    auto k = at(cursor);
                    if (!k || *k != Kind::O_And) break;
                    Coord next = adv(cursor);
                    auto nt = at(next);
                    if (!nt || !is_noun(*nt)) break;
                    targets.push_back(*nt);
                    cursor = adv(next);
                }
                for (Kind n : subjects)
                    for (Kind t : targets)
                        if (n != t)
                            cond_xform_out.push_back({n, conditions, t, false});
                return;
            }
        }

        // Conditional property: NOUN ON … IS [NOT] PROPERTY [AND …]
        struct PropEntry { Kind prop; bool neg; };
        std::vector<PropEntry> props;
        auto parse_one_prop_local = [&]() -> bool {
            auto k = at(cursor);
            if (!k) return false;
            bool neg = false;
            if (*k == Kind::O_Not) {
                neg = true;
                cursor = adv(cursor);
                k = at(cursor);
                if (!k || !is_property(*k)) return false;
            }
            if (!is_property(*k)) return false;
            props.push_back({*k, neg});
            cursor = adv(cursor);
            return true;
        };
        if (!parse_one_prop_local()) return;
        while (true) {
            auto k = at(cursor);
            if (!k || *k != Kind::O_And) break;
            cursor = adv(cursor);
            if (!parse_one_prop_local()) { cursor = bak(cursor); break; }
        }
        for (Kind n : subjects)
            for (auto const& pe : props)
                if (!pe.neg) cond_out.push_back({n, conditions, pe.prop, false});
        return;
    }

    // ── NOUN NOT ON NOUN [AND NOUN]* IS/MAKE PREDICATE ────────────────────
    if (*op_tok == Kind::O_Not) {
        cursor = adv(cursor);
        auto on_check = at(cursor);

        // ── NOUN NOT FACING <cond> IS [NOT] PROPERTY ──────────────────────
        if (on_check && *on_check == Kind::O_Facing) {
            cursor = adv(cursor);
            auto cond_k = at(cursor);
            if (!cond_k) return;
            bool cond_is_dir = (*cond_k == Kind::P_Left || *cond_k == Kind::P_Right ||
                                *cond_k == Kind::P_Up   || *cond_k == Kind::P_Down);
            if (!is_noun(*cond_k) && !cond_is_dir) return;
            Kind cond = *cond_k;
            cursor = adv(cursor);
            if (auto verb = at(cursor); !verb || *verb != Kind::O_Is) return;
            cursor = adv(cursor);
            auto pk = at(cursor);
            if (!pk) return;
            bool neg = false;
            if (*pk == Kind::O_Not) { neg = true; cursor = adv(cursor); pk = at(cursor); }
            if (!pk || !is_property(*pk)) return;
            for (Kind n : subjects)
                facing_out.push_back({n, cond, *pk, !neg}); // negated=true (NOT FACING)
            return;
        }

        if (!on_check || *on_check != Kind::O_On) return;
        cursor = adv(cursor);
        auto cond_n = at(cursor);
        if (!cond_n || !is_noun(*cond_n)) return;
        std::vector<Kind> conditions;
        conditions.push_back(*cond_n);
        cursor = adv(cursor);
        // AND loop for additional condition nouns.
        while (true) {
            auto k = at(cursor);
            if (!k || *k != Kind::O_And) break;
            Coord next = adv(cursor);
            auto nt = at(next);
            if (!nt || !is_noun(*nt)) break;
            conditions.push_back(*nt);
            cursor = adv(next);
        }

        auto verb2 = at(cursor);
        if (!verb2) return;

        // NOUN NOT ON … MAKE NOUN [AND NOUN]*
        if (*verb2 == Kind::O_Make) {
            cursor = adv(cursor);
            auto t0 = at(cursor);
            if (!t0 || !is_noun(*t0)) return;
            std::vector<Kind> targets;
            targets.push_back(*t0);
            cursor = adv(cursor);
            while (true) {
                auto k = at(cursor);
                if (!k || *k != Kind::O_And) break;
                Coord next = adv(cursor);
                auto nt = at(next);
                if (!nt || !is_noun(*nt)) break;
                targets.push_back(*nt);
                cursor = adv(next);
            }
            for (Kind n : subjects)
                for (Kind t : targets)
                    cond_make_out.push_back({n, conditions, t, /*negated=*/true});
            return;
        }

        if (*verb2 != Kind::O_Is) return;
        cursor = adv(cursor);

        // Negated conditional transform: NOUN NOT ON … IS NOUN [AND NOUN]*
        {
            auto pred = at(cursor);
            if (pred && is_noun(*pred)) {
                std::vector<Kind> targets;
                targets.push_back(*pred);
                cursor = adv(cursor);
                while (true) {
                    auto k = at(cursor);
                    if (!k || *k != Kind::O_And) break;
                    Coord nxt = adv(cursor);
                    auto nt = at(nxt);
                    if (!nt || !is_noun(*nt)) break;
                    targets.push_back(*nt);
                    cursor = adv(nxt);
                }
                for (Kind n : subjects)
                    for (Kind t : targets)
                        if (n != t)
                            cond_xform_out.push_back({n, conditions, t, /*negated=*/true});
                return;
            }
        }

        // Negated conditional property: NOUN NOT ON … IS [NOT] PROPERTY [AND …]
        struct PropEntryN { Kind prop; bool neg; };
        std::vector<PropEntryN> props;
        auto parse_prop_neg = [&]() -> bool {
            auto k = at(cursor);
            if (!k) return false;
            bool neg = false;
            if (*k == Kind::O_Not) {
                neg = true;
                cursor = adv(cursor);
                k = at(cursor);
                if (!k || !is_property(*k)) return false;
            }
            if (!is_property(*k)) return false;
            props.push_back({*k, neg});
            cursor = adv(cursor);
            return true;
        };
        if (!parse_prop_neg()) return;
        while (true) {
            auto k = at(cursor);
            if (!k || *k != Kind::O_And) break;
            cursor = adv(cursor);
            if (!parse_prop_neg()) { cursor = bak(cursor); break; }
        }
        for (Kind n : subjects)
            for (auto const& pe : props)
                if (!pe.neg) cond_out.push_back({n, conditions, pe.prop, /*negated=*/true});
        return;
    }

    // Must be IS.
    if (*op_tok != Kind::O_Is) return;
    cursor = adv(cursor);

    // Predicate: optional leading NOT, then PROPERTY or NOUN.
    auto pred0 = at(cursor);
    if (!pred0) return;

    // Check for NOUN IS NOUN [AND NOUN]* (transform, possibly multi-target).
    if (is_noun(*pred0)) {
        std::vector<Kind> targets;
        targets.push_back(*pred0);
        cursor = adv(cursor);
        while (true) {
            auto k = at(cursor);
            if (!k || *k != Kind::O_And) break;
            Coord next = adv(cursor);
            auto nt = at(next);
            if (!nt || !is_noun(*nt)) break;
            targets.push_back(*nt);
            cursor = adv(next);
        }
        for (Kind n : subjects)
            for (Kind t : targets)
                xform_out.push_back({n, t});
        return;
    }

    // Parse property phrase: [NOT] PROPERTY [AND [NOT] PROPERTY]*
    if (!is_property(*pred0) && *pred0 != Kind::O_Not) return;

    struct PropEntry { Kind prop; bool neg; };
    std::vector<PropEntry> props;

    auto parse_one_prop = [&]() -> bool {
        auto k = at(cursor);
        if (!k) return false;
        bool neg = false;
        if (*k == Kind::O_Not) {
            neg = true;
            cursor = adv(cursor);
            k = at(cursor);
            if (!k || !is_property(*k)) return false;
        }
        if (!is_property(*k)) return false;
        props.push_back({*k, neg});
        cursor = adv(cursor);
        return true;
    };

    if (!parse_one_prop()) return;

    // Optional AND [NOT] PROPERTY repetition.
    while (true) {
        auto k = at(cursor);
        if (!k || *k != Kind::O_And) break;
        cursor = adv(cursor);
        if (!parse_one_prop()) { cursor = {cursor.x - step_dir.x, cursor.y - step_dir.y}; break; }
    }

    for (Kind n : subjects) {
        for (auto const& pe : props) {
            prop_out.push_back({n, pe.prop, pe.neg});
        }
    }
}

}  // namespace

RuleSet RuleSet::parse(World const& world) {
    RuleSet rs;

    auto cells = world.all_cells();
    for (Coord c : cells) {
        auto const& ids = world.at(c);
        bool has_text = false;
        for (ObjectId id : ids) {
            Object const* o = world.get(id);
            if (o && o->text) { has_text = true; break; }
        }
        if (!has_text) continue;
        scan_strip(world, c, {1, 0}, rs.rules_, rs.transforms_, rs.makes_, rs.eats_, rs.cond_rules_, rs.cond_transforms_, rs.cond_makes_, rs.facing_rules_);
        scan_strip(world, c, {0, 1}, rs.rules_, rs.transforms_, rs.makes_, rs.eats_, rs.cond_rules_, rs.cond_transforms_, rs.cond_makes_, rs.facing_rules_);
    }

    // Deduplicate verb-operator rules before further processing.
    // Multiple scan starting positions can produce the same rule (e.g. scanning
    // from ROCK in "BABA AND ROCK IS FLAG" duplicates the ROCK IS FLAG entry).
    // Property-rule duplicates are harmless (index deduplicates lookups), but
    // transform duplicates would cause xmap to contain the same target twice,
    // leading to objects being spawned multiple times.
    {
        std::set<std::pair<uint16_t,uint16_t>> seen;
        std::vector<TransformRule> deduped;
        for (auto const& tr : rs.transforms_) {
            auto key2 = std::make_pair(static_cast<uint16_t>(tr.from),
                                       static_cast<uint16_t>(tr.to));
            if (seen.insert(key2).second) deduped.push_back(tr);
        }
        rs.transforms_ = std::move(deduped);
    }
    {
        std::set<std::pair<uint16_t,uint16_t>> seen;
        std::vector<EatRule> deduped;
        for (auto const& er : rs.eats_) {
            auto key2 = std::make_pair(static_cast<uint16_t>(er.subject),
                                       static_cast<uint16_t>(er.target));
            if (seen.insert(key2).second) deduped.push_back(er);
        }
        rs.eats_ = std::move(deduped);
    }

    // Base rule: TEXT IS PUSH (always, treated as positive non-cancellable).
    rs.rules_.push_back({Kind::N_Text, Kind::P_Push, false});

    // NOT cancellation (spec §3.3 rule 1):
    // If both "X IS P" and "X IS NOT P" exist, neither applies.
    // Build a set of (subject, property) pairs that are contradicted.
    using Key = std::uint32_t;
    auto key = [](Kind n, Kind p) -> Key {
        return (static_cast<Key>(n) << 16) | static_cast<Key>(p);
    };

    std::unordered_set<Key> positive_keys, negative_keys;
    for (auto const& r : rs.rules_) {
        if (r.negated) negative_keys.insert(key(r.subject, r.property));
        else           positive_keys.insert(key(r.subject, r.property));
    }
    // Contradicted = intersection of positive and negative.
    std::unordered_set<Key> cancelled;
    for (Key k2 : positive_keys)
        if (negative_keys.count(k2)) cancelled.insert(k2);

    // Build final positive rules (exclude cancelled and negated).
    std::vector<PropertyRule> final_rules;
    for (auto const& r : rs.rules_) {
        if (r.negated) continue;  // never index negated rules directly
        if (cancelled.count(key(r.subject, r.property))) continue;  // cancelled by NOT
        final_rules.push_back(r);
    }
    rs.rules_ = std::move(final_rules);

    // Build lookup index from surviving positive rules.
    for (auto const& r : rs.rules_) {
        rs.index_.insert(key_(r.subject, r.property));
    }
    return rs;
}

bool RuleSet::has_property(Kind noun, Kind property) const {
    if (index_.count(key_(noun, property))) return true;
    return false;
}

bool RuleSet::object_has_property(World const& world, ObjectId id, Kind property) const {
    Object const* o = world.get(id);
    if (!o) return false;
    Kind noun = o->text ? Kind::N_Text : o->kind;
    if (index_.count(key_(noun, property))) return true;

    // Check conditional rules (NOUN ON/NOT ON NOUN [AND NOUN]* IS PROPERTY).
    if (!o->text) {
        for (auto const& cr : cond_rules_) {
            if (cr.subject != o->kind || cr.property != property) continue;
            // ON A AND B: grant if ALL present. NOT ON A AND B: grant if NOT ALL present.
            bool all_found = true;
            for (Kind cn : cr.condition_nouns) {
                bool found = false;
                for (ObjectId other : world.at(o->pos)) {
                    if (other == id) continue;
                    Object const* ob = world.get(other);
                    if (ob && !ob->text && ob->kind == cn) { found = true; break; }
                }
                if (!found) { all_found = false; break; }
            }
            bool condition_met = cr.negated_condition ? !all_found : all_found;
            if (condition_met) return true;
        }

        // Check FACING rules (NOUN [NOT] FACING <cond> IS PROPERTY).
        for (auto const& fr : facing_rules_) {
            if (fr.subject != o->kind || fr.property != property) continue;
            bool matched;
            bool cond_is_dir = (fr.condition == Kind::P_Left || fr.condition == Kind::P_Right ||
                                fr.condition == Kind::P_Up   || fr.condition == Kind::P_Down);
            if (cond_is_dir) {
                // Direction check: does the object's facing match the required direction?
                Direction req = fr.condition == Kind::P_Left  ? Direction::Left  :
                                fr.condition == Kind::P_Right ? Direction::Right :
                                fr.condition == Kind::P_Up    ? Direction::Up    :
                                                                Direction::Down;
                matched = (o->facing == req);
            } else {
                // Noun check: does the tile ahead contain the required noun?
                Coord face_pos = {o->pos.x + step(o->facing).x, o->pos.y + step(o->facing).y};
                matched = false;
                for (ObjectId other : world.at(face_pos)) {
                    Object const* ob = world.get(other);
                    if (ob && !ob->text && ob->kind == fr.condition) { matched = true; break; }
                }
            }
            if (fr.negated ? !matched : matched) return true;
        }
    }
    return false;
}

}  // namespace baba::core
