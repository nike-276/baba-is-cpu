#pragma once

#include "undo_buffer.hpp"
#include "core/ruleset.hpp"
#include "core/tick.hpp"
#include "core/world.hpp"

namespace baba::sim {

// Runtime wrapper around the core engine: owns a World, UndoBuffer, and tick counter.
// step_forward applies one tick and pushes the Change list to the undo buffer.
// step_back pops the most recent tick's changes and applies them in reverse.
class Simulator {
public:
    explicit Simulator(core::World world, std::size_t undo_cap = 10'000);

    core::TickReport step_forward(core::Input input);

    // Undo one tick. Returns false if nothing to undo.
    bool step_back();

    core::World const& world() const { return world_; }
    core::World&       world()       { return world_; }

    core::RuleSet current_rules() const { return core::RuleSet::parse(world_); }

    int  tick_count() const { return tick_; }
    bool last_won()   const { return last_report_.won; }

private:
    core::World      world_;
    UndoBuffer       undo_;
    core::TickReport last_report_{};
    int              tick_{0};
};

}  // namespace baba::sim
