#include "simulator.hpp"

#include <algorithm>

namespace baba::sim {

using namespace baba::core;

Simulator::Simulator(World world, std::size_t undo_cap)
    : world_{std::move(world)}, undo_{undo_cap} {}

TickReport Simulator::step_forward(Input input) {
    last_report_ = apply_tick(world_, input);
    undo_.push(last_report_.changes);
    ++tick_;
    return last_report_;
}

bool Simulator::step_back() {
    auto maybe = undo_.pop();
    if (!maybe) return false;

    auto const& changes = *maybe;
    // Replay in reverse order, applying the inverse of each Change.
    for (auto it = changes.rbegin(); it != changes.rend(); ++it) {
        auto const& c = *it;
        switch (c.kind) {
            case ChangeKind::Move:
                world_.move(c.id, c.from_pos);
                break;
            case ChangeKind::Face:
                world_.face(c.id, c.from_facing);
                break;
            case ChangeKind::Spawn:
                // Inverse of spawn is destroy.
                world_.destroy(c.id);
                break;
            case ChangeKind::Destroy:
                // Inverse of destroy is re-spawn (fresh id per spec §6).
                world_.spawn(c.obj_pos, c.obj_kind, c.obj_text, c.obj_facing);
                break;
            case ChangeKind::Retype:
                world_.retype(c.id, c.from_kind);
                break;
        }
    }
    --tick_;
    return true;
}

}  // namespace baba::sim
