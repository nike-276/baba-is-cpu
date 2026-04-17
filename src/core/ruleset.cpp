#include "ruleset.hpp"

#include <algorithm>

namespace baba::core {

namespace {

// Try to parse a horizontal or vertical text strip starting at `start`,
// stepping by `dir`. We look for the simple form:
//     NOUN [AND NOUN]* IS PROPERTY [AND PROPERTY]*
// (No NOT cancellation in Phase 1; X-IS-Y transformations not implemented.)
// Returns extracted (subjects, properties) on success, or an empty pair.
struct StripMatch {
    std::vector<Kind> subjects;
    std::vector<Kind> properties;
};

// Read a single text kind at coord, or std::nullopt if the cell has no text.
std::optional<Kind> text_kind_at(World const& world, Coord c) {
    auto const& cell = world.at(c);
    for (ObjectId id : cell) {
        Object const* obj = world.get(id);
        if (obj && obj->text) return obj->kind;
    }
    return std::nullopt;
}

void scan_strip(World const& world, Coord start, Coord step_dir,
                std::vector<PropertyRule>& prop_out,
                std::vector<TransformRule>& xform_out) {
    auto first = text_kind_at(world, start);
    if (!first || !is_noun(*first)) return;

    StripMatch m;
    m.subjects.push_back(*first);

    Coord cursor = {start.x + step_dir.x, start.y + step_dir.y};
    // Optional AND NOUN repetition.
    while (true) {
        auto k = text_kind_at(world, cursor);
        if (!k || *k != Kind::O_And) break;
        Coord next = {cursor.x + step_dir.x, cursor.y + step_dir.y};
        auto noun = text_kind_at(world, next);
        if (!noun || !is_noun(*noun)) return;
        m.subjects.push_back(*noun);
        cursor = {next.x + step_dir.x, next.y + step_dir.y};
    }

    // Require IS.
    auto is_tok = text_kind_at(world, cursor);
    if (!is_tok || *is_tok != Kind::O_Is) return;
    cursor = {cursor.x + step_dir.x, cursor.y + step_dir.y};

    // Predicate: either a property (phrase) or a noun (transform).
    auto pred0 = text_kind_at(world, cursor);
    if (!pred0) return;

    if (is_property(*pred0)) {
        // NOUN IS PROPERTY [AND PROPERTY]*
        m.properties.push_back(*pred0);
        cursor = {cursor.x + step_dir.x, cursor.y + step_dir.y};

        while (true) {
            auto k = text_kind_at(world, cursor);
            if (!k || *k != Kind::O_And) break;
            Coord next = {cursor.x + step_dir.x, cursor.y + step_dir.y};
            auto p = text_kind_at(world, next);
            if (!p || !is_property(*p)) break;
            m.properties.push_back(*p);
            cursor = {next.x + step_dir.x, next.y + step_dir.y};
        }

        for (Kind n : m.subjects) {
            for (Kind p : m.properties) {
                prop_out.push_back({n, p});
            }
        }
    } else if (is_noun(*pred0)) {
        // NOUN IS NOUN — single-target transform only (AND-noun-predicates deferred)
        Kind target = *pred0;
        for (Kind n : m.subjects) {
            xform_out.push_back({n, target});
        }
    }
}

}  // namespace

RuleSet RuleSet::parse(World const& world) {
    RuleSet rs;

    // Walk every text object exactly once and try a horizontal and a vertical
    // scan starting there. Duplicates are de-duped by the index_ set.
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

    // Base rule: TEXT IS PUSH (always).
    rs.rules_.push_back({Kind::N_Text, Kind::P_Push});

    // De-dup into the lookup index.
    for (auto const& r : rs.rules_) {
        rs.index_.insert(key_(r.subject, r.property));
    }
    return rs;
}

bool RuleSet::has_property(Kind noun, Kind property) const {
    if (index_.count(key_(noun, property))) return true;
    // The abstract noun TEXT applies to every text kind, but the caller passes
    // a noun kind here, not a text-kind, so no fallthrough is needed.
    return false;
}

bool RuleSet::object_has_property(World const& world, ObjectId id, Kind property) const {
    Object const* o = world.get(id);
    if (!o) return false;
    if (o->text) {
        // Text objects: only TEXT IS <property> rules apply.
        return index_.count(key_(Kind::N_Text, property)) > 0;
    }
    return index_.count(key_(o->kind, property)) > 0;
}

}  // namespace baba::core
