// core/change.hpp — diff record for one mutation, used by the undo buffer.
//
// Every World mutator that fires during apply_tick emits a Change so the undo
// stack can replay the tick in reverse. Inverse operations:
//   Move    → move(id, from_pos)
//   Face    → face(id, from_facing)
//   Spawn   → destroy(id)
//   Destroy → spawn(obj_pos, obj_kind, obj_text, obj_facing)  [fresh id per spec §6]
//   Retype  → retype(id, from_kind)
#pragma once

#include "coord.hpp"
#include "direction.hpp"
#include "kind.hpp"
#include "object.hpp"

namespace baba::core {

enum class ChangeKind : std::uint8_t {
    Move,
    Face,
    Spawn,
    Destroy,
    Retype,
};

struct Change {
    ChangeKind kind{ChangeKind::Move};
    ObjectId   id{0};

    // Move
    Coord      from_pos{};
    Coord      to_pos{};

    // Face
    Direction  from_facing{Direction::Right};
    Direction  to_facing{Direction::Right};

    // Retype
    Kind       from_kind{Kind::None};
    Kind       to_kind{Kind::None};

    // Destroy snapshot — enough to re-spawn on undo
    Kind       obj_kind{Kind::None};
    Kind       obj_original_kind{Kind::None};
    bool       obj_text{false};
    Direction  obj_facing{Direction::Right};
    Coord      obj_pos{};

    // --- Factory helpers ---

    static Change move(ObjectId id, Coord from, Coord to) {
        Change c;
        c.kind     = ChangeKind::Move;
        c.id       = id;
        c.from_pos = from;
        c.to_pos   = to;
        return c;
    }

    static Change face(ObjectId id, Direction from, Direction to) {
        Change c;
        c.kind         = ChangeKind::Face;
        c.id           = id;
        c.from_facing  = from;
        c.to_facing    = to;
        return c;
    }

    static Change spawn(ObjectId id) {
        Change c;
        c.kind = ChangeKind::Spawn;
        c.id   = id;
        return c;
    }

    static Change destroy(ObjectId id, Coord pos, Kind k, Kind orig_k, bool text, Direction facing) {
        Change c;
        c.kind              = ChangeKind::Destroy;
        c.id                = id;
        c.obj_pos           = pos;
        c.obj_kind          = k;
        c.obj_original_kind = orig_k;
        c.obj_text          = text;
        c.obj_facing        = facing;
        return c;
    }

    static Change retype(ObjectId id, Kind from, Kind to) {
        Change c;
        c.kind      = ChangeKind::Retype;
        c.id        = id;
        c.from_kind = from;
        c.to_kind   = to;
        return c;
    }
};

}  // namespace baba::core
