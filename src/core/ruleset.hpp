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

#include <array>
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

// NOUN FEAR NOUN [AND NOUN]*: each tick subject moves away from any adjacent target
// using directional priority relative to subject facing (forward->CW->CCW->backward).
struct FearRule {
    Kind subject;
    Kind target;
};

// NOUN PLAY NOTE [OCTAVE] [ACCIDENTAL]: each tick, emit a SoundEvent for each matching object.
// note = O_LetterA…O_LetterG; modifiers may appear in any order after the note.
struct PlayRule {
    Kind subject;
    Kind note{Kind::O_LetterA};  // O_LetterA…O_LetterG
    int  octave{5};
    bool sharp{false};
    bool flat{false};
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

// NOUN [NOT] FACING <cond> IS NOUN — conditional transform based on facing.
struct FacingTransformRule {
    Kind subject;
    Kind condition;  // noun OR directional property
    Kind target;
    bool negated{false};
};

// NOUN HAS NOUN [AND NOUN]*: when a non-text `subject` is destroyed via any DESTRUCT
// sub-step, one `target` object spawns at the destroyed subject's tile with its facing.
struct HasRule {
    Kind subject;
    Kind target;
};

// One clause in a compound ON/FACEDBY condition.
// nouns: ALL must satisfy the clause type (AND-joined within clause).
// negated: if true, requires that NOT all nouns satisfy the condition.
enum class CondType { On, FacedBy };
struct CondClause {
    CondType ctype{CondType::On};   // On = co-location; FacedBy = adjacent object facing subject
    std::vector<Kind> nouns;        // ALL must be present (or absent if negated)
    bool negated{false};            // true → require NOT all present
};

// NOUN ON NOUN [AND NOUN]* [AND [NOT] ON NOUN [AND NOUN]*]* IS PROPERTY:
// `subject` has `property` only when ALL clauses pass.
struct ConditionalPropertyRule {
    Kind subject;
    std::vector<CondClause> clauses;  // ALL clauses must pass
    Kind property;
};

// NOUN ON ... IS NOUN: conditional transform.
struct ConditionalTransformRule {
    Kind subject;
    std::vector<CondClause> clauses;
    Kind target;
};

// NOUN ON ... MAKE NOUN: conditional spawn.
struct ConditionalMakeRule {
    Kind subject;
    std::vector<CondClause> clauses;
    Kind target;
};

// NOUN ON ... EAT NOUN: conditional destruction.
struct ConditionalEatRule {
    Kind subject;
    std::vector<CondClause> clauses;
    Kind target;
};

// NOUN ON ... PLAY NOTE [OCTAVE] [ACCIDENTAL]: emit note only when ON clause holds.
struct ConditionalPlayRule {
    Kind subject;
    std::vector<CondClause> clauses;
    Kind note{Kind::O_LetterA};
    int  octave{5};
    bool sharp{false};
    bool flat{false};
};

// [NOT] POWEREDx [AND [NOT] POWEREDy]* NOUN IS PROPERTY.
// Each Condition checks one POWER channel; ALL conditions must hold (AND semantics).
// negated=true → condition met when NO object has power_kind.
struct GlobalConditionPropertyRule {
    struct Condition {
        Kind power_kind{Kind::P_Power};
        bool negated{false};
        bool operator==(Condition const& o) const {
            return power_kind == o.power_kind && negated == o.negated;
        }
    };
    Kind subject;
    Kind property;
    std::vector<Condition> conditions;
};

// [NOT] POWEREDx [AND [NOT] POWEREDy]* NOUN [ON/NOT ON NOUN]* EAT NOUN [AND NOUN]*.
// Power conditions AND optional spatial ON clauses both must hold.
struct GlobalConditionEatRule {
    using Condition = GlobalConditionPropertyRule::Condition;
    Kind subject;
    Kind target;
    std::vector<Condition>  conditions;  // POWERED prefix conditions
    std::vector<CondClause> on_clauses;  // optional ON spatial conditions (empty = none)
};

// [NOT] POWEREDx [AND [NOT] POWEREDy]* NOUN [ON/NOT ON NOUN]* MAKE NOUN [AND NOUN]*.
// Mirrors GlobalConditionEatRule: power conditions AND optional spatial ON clauses.
struct GlobalConditionMakeRule {
    using Condition = GlobalConditionPropertyRule::Condition;
    Kind subject;
    Kind target;
    std::vector<Condition>  conditions;
    std::vector<CondClause> on_clauses;
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
    std::vector<ConditionalPropertyRule>      const& conditional_rules()                const { return cond_rules_; }
    std::vector<ConditionalTransformRule>     const& conditional_transform_rules()      const { return cond_transforms_; }
    std::vector<ConditionalMakeRule>          const& conditional_make_rules()           const { return cond_makes_; }
    std::vector<ConditionalEatRule>           const& conditional_eat_rules()            const { return cond_eats_; }
    std::vector<FacingPropertyRule>           const& facing_rules()                    const { return facing_rules_; }
    std::vector<FacingTransformRule>          const& facing_transform_rules()           const { return facing_transforms_; }
    std::vector<GlobalConditionPropertyRule>  const& global_condition_property_rules()  const { return global_cond_rules_; }
    std::vector<GlobalConditionEatRule>       const& global_condition_eat_rules()       const { return global_cond_eat_rules_; }
    std::vector<GlobalConditionMakeRule>      const& global_condition_make_rules()      const { return global_cond_make_rules_; }
    std::vector<HasRule>                      const& has_rules()                        const { return has_rules_; }
    std::vector<FearRule>                     const& fear_rules()                       const { return fear_rules_; }
    std::vector<PlayRule>                     const& play_rules()                       const { return play_rules_; }
    std::vector<ConditionalPlayRule>          const& conditional_play_rules()           const { return cond_play_rules_; }

    // O(1): true if ANY non-negated rule (including conditional) could grant
    // `property` at runtime. Conservative for conditional rules. Used for
    // tick-phase early-exit guards to skip expensive all_cells() passes.
    bool any_grants(Kind property) const { return granted_props_.count(property) != 0; }

    // Categories used by subjects_affected_for_property / _verb.
    enum class Verb : std::uint8_t { Transform, Make, Eat, Has, Fear, Play };

    // Return every subject Kind that MIGHT be granted `property` at runtime
    // (unconditional, conditional, facing, or global-conditional rule).
    // Sorted ascending. Empty sentinel on miss. Iteration of this list into
    // World::objects_of_kind() is the canonical pattern for per-property
    // substages.
    std::vector<Kind> const& subjects_for_property(Kind property) const;

    // Subject kinds appearing in any rule of the given verb category
    // (Transform/Make/Eat/Has/Fear/Play). Sorted ascending.
    std::vector<Kind> const& subjects_for_verb(Verb v) const;

    // True if any live non-text object currently has power_prop active
    // (unconditional or via an ON condition). Used by tick phases to evaluate
    // GlobalConditionEatRule and similar. Never recurses into global_cond_rules_.
    bool any_has_power_kind(World const& world, Kind power_prop) const;

    // Evaluate all CondClauses for the object `id` in `world`. Returns true when
    // every clause passes. Dispatches on CondType::On (co-location) and
    // CondType::FacedBy (adjacent object facing toward subject). Shared across
    // all conditional rule types in ruleset.cpp and tick.cpp.
    static bool eval_cond_clauses(World const& world, ObjectId id,
                                  std::vector<CondClause> const& clauses);

private:
    std::vector<PropertyRule>             rules_;
    std::vector<TransformRule>            transforms_;
    std::vector<MakeRule>                 makes_;
    std::vector<EatRule>                  eats_;
    std::vector<ConditionalPropertyRule>  cond_rules_;
    std::vector<ConditionalTransformRule> cond_transforms_;
    std::vector<ConditionalMakeRule>      cond_makes_;
    std::vector<ConditionalEatRule>       cond_eats_;
    std::vector<FacingPropertyRule>           facing_rules_;
    std::vector<FacingTransformRule>          facing_transforms_;
    std::vector<GlobalConditionPropertyRule>  global_cond_rules_;
    std::vector<GlobalConditionEatRule>       global_cond_eat_rules_;
    std::vector<GlobalConditionMakeRule>      global_cond_make_rules_;
    std::vector<HasRule>                      has_rules_;
    std::vector<FearRule>                     fear_rules_;
    std::vector<PlayRule>                     play_rules_;
    std::vector<ConditionalPlayRule>          cond_play_rules_;
    // Cached lookup: (noun, property) → bool.
    std::unordered_set<std::uint32_t> index_;
    // Set of every property Kind potentially grantable by any rule (for any_grants()).
    std::unordered_set<Kind> granted_props_;

    // Pre-computed subject-kind unions. Keys:
    //   subjects_per_property_[p]  = sorted subjects that might grant property p
    //   subjects_per_verb_[v]      = sorted subjects appearing in verb category v
    // Populated once at end of parse().
    std::unordered_map<Kind, std::vector<Kind>>  subjects_per_property_;
    std::array<std::vector<Kind>, 6>             subjects_per_verb_;

    // object_has_property hot-path indices. Keyed by pack(subject, property).
    // Each maps to sorted rule indices into the owning vector. An early-reject
    // set (cond_any_) lets the hot path bail in one probe.
    std::unordered_set<std::uint32_t>                          cond_any_;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> cond_idx_;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> facing_idx_;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> global_idx_;

    static std::uint32_t key_(Kind noun, Kind property) {
        return (static_cast<std::uint32_t>(noun) << 16) |
                static_cast<std::uint32_t>(property);
    }
};

}  // namespace baba::core
