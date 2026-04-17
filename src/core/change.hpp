// core/change.hpp — diff record for one mutation, used by the undo buffer.
//
// Phase 1 keeps Move/Face only (no transform/destroy/spawn yet — those land
// when the rule engine grows TRANSFORM and DESTRUCT phases).
#pragma once

#include "coord.hpp"
#include "direction.hpp"
#include "object.hpp"

namespace baba::core {

enum class ChangeKind : std::uint8_t {
    Move,
    Face,
};

struct Change {
    ChangeKind kind;
    ObjectId   id;
    Coord      from_pos;
    Coord      to_pos;
    Direction  from_facing;
    Direction  to_facing;
};

}  // namespace baba::core
