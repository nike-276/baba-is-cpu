#include "ruleset.hpp"

#include <algorithm>
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
                std::vector<TransformRule>& xform_out) {
    auto at = [&](Coord c) { return text_kind_at(world, c); };
    auto adv = [&](Coord c) -> Coord { return {c.x + step_dir.x, c.y + step_dir.y}; };

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

    // Require IS.
    auto is_tok = at(cursor);
    if (!is_tok || *is_tok != Kind::O_Is) return;
    cursor = adv(cursor);

    // Predicate: optional leading NOT, then PROPERTY or NOUN.
    auto pred0 = at(cursor);
    if (!pred0) return;

    // Check for NOUN IS NOUN (transform).
    if (is_noun(*pred0)) {
        for (Kind n : subjects) xform_out.push_back({n, *pred0});
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
        scan_strip(world, c, {1, 0}, rs.rules_, rs.transforms_);
        scan_strip(world, c, {0, 1}, rs.rules_, rs.transforms_);
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
    if (o->text) {
        return index_.count(key_(Kind::N_Text, property)) > 0;
    }
    return index_.count(key_(o->kind, property)) > 0;
}

}  // namespace baba::core
