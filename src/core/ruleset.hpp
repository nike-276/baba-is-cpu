// core/ruleset.hpp — parsed rules for one tick.
//
// Phase 1 scope: IS, AND, NOT-as-text-on-tile parsed but treated as a stub
// (NOT distribution comes when the rule engine grows past the v1 essentials).
// Properties tracked: YOU, PUSH, STOP, WIN. Other property kinds are parsed
// and stored but ignored by the tick engine until later phases land.
#pragma once

#include "kind.hpp"
#include "object.hpp"
#include "world.hpp"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace baba::core {

struct PropertyRule {
    Kind subject;   // a noun kind (or Kind::N_Text for the abstract noun)
    Kind property;  // a P_* kind
    bool negated{false};  // true for "X IS NOT P" (cancels matching positive rule)
};

// NOUN IS NOUN transformation: every non-text object of kind `from` becomes `to`.
struct TransformRule {
    Kind from;
    Kind to;
};

// NOUN IS MAKE NOUN: each tick, spawn one `to` object on every tile
// containing a non-text `from` object (idempotent — skipped if already present).
struct MakeRule {
    Kind from;
    Kind to;
};

// NOUN EAT NOUN: when a non-text `subject` shares a tile with a non-text `target`,
// the target is destroyed; the subject survives.
struct EatRule {
    Kind subject;
    Kind target;
};

// NOUN [NOT] FACING <cond> IS PROPERTY.
// `condition` is either:
//   - a noun kind  → check the tile ahead contains a non-text object of that kind
//   - P_Left/P_Right/P_Up/P_Down → check the subject's own facing direction matches
struct FacingPropertyRule {
    Kind subject;
    Kind condition;  // noun OR directional property (P_Left/Right/Up/Down)
    Kind property;
    bool negated{false};  // true for NOT FACING
};

// NOUN ON NOUN [AND NOUN]* IS PROPERTY: `subject` has `property` only when
// sharing a tile with ALL of the condition_nouns (or NONE if negated).
struct ConditionalPropertyRule {
    Kind subject;
    std::vector<Kind> condition_nouns;  // ALL must be present (or absent if negated)
    Kind property;
    bool negated_condition{false};
};

// NOUN ON NOUN [AND NOUN]* IS NOUN: conditional transform.
struct ConditionalTransformRule {
    Kind subject;
    std::vector<Kind> condition_nouns;
    Kind target;
    bool negated_condition{false};
};

// NOUN ON NOUN [AND NOUN]* MAKE NOUN: conditional spawn.
struct ConditionalMakeRule {
    Kind subject;
    std::vector<Kind> condition_nouns;
    Kind target;
    bool negated_condition{false};
};

class RuleSet {
public:
    RuleSet() = default;

    // Build the rule set from text objects laid out in the world.
    // Always includes the base rule TEXT IS PUSH.
    static RuleSet parse(World const& world);

    // Property predicates (operate on a non-text object's noun kind).
    bool has_property(Kind noun, Kind property) const;

    // Property predicates dispatched on an object id (text objects use
    // base rule TEXT IS PUSH for PUSH, otherwise no properties).
    bool object_has_property(World const& world, ObjectId id, Kind property) const;

    std::vector<PropertyRule>            const& property_rules()    const { return rules_; }
    std::vector<TransformRule>           const& transform_rules()  const { return transforms_; }
    std::vector<MakeRule>                const& make_rules()       const { return makes_; }
    std::vector<EatRule>                 const& eat_rules()               const { return eats_; }
    std::vector<ConditionalPropertyRule>  const& conditional_rules()           const { return cond_rules_; }
    std::vector<ConditionalTransformRule> const& conditional_transform_rules() const { return cond_transforms_; }
    std::vector<ConditionalMakeRule>      const& conditional_make_rules()      const { return cond_makes_; }
    std::vector<FacingPropertyRule>       const& facing_rules()                const { return facing_rules_; }

private:
    std::vector<PropertyRule>            rules_;
    std::vector<TransformRule>           transforms_;
    std::vector<MakeRule>                makes_;
    std::vector<EatRule>                 eats_;
    std::vector<ConditionalPropertyRule>  cond_rules_;
    std::vector<ConditionalTransformRule> cond_transforms_;
    std::vector<ConditionalMakeRule>      cond_makes_;
    std::vector<FacingPropertyRule>       facing_rules_;
    // Cached lookup: (noun, property) → bool.
    std::unordered_set<std::uint32_t> index_;

    static std::uint32_t key_(Kind noun, Kind property) {
        return (static_cast<std::uint32_t>(noun) << 16) |
                static_cast<std::uint32_t>(property);
    }
};

}  // namespace baba::core
