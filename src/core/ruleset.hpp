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
};

// NOUN IS NOUN transformation: every non-text object of kind `from` becomes `to`.
struct TransformRule {
    Kind from;  // source noun kind
    Kind to;    // target noun kind
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

    std::vector<PropertyRule> const& property_rules()   const { return rules_; }
    std::vector<TransformRule> const& transform_rules() const { return transforms_; }

private:
    std::vector<PropertyRule>  rules_;
    std::vector<TransformRule> transforms_;
    // Cached lookup: (noun, property) → bool.
    std::unordered_set<std::uint32_t> index_;

    static std::uint32_t key_(Kind noun, Kind property) {
        return (static_cast<std::uint32_t>(noun) << 16) |
                static_cast<std::uint32_t>(property);
    }
};

}  // namespace baba::core
