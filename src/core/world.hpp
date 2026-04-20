// core/world.hpp — sparse spatial index + monotonic id allocator.
//
// Invariants enforced by World:
//   * Object ids are monotonic; spawn() always returns a fresh id.
//   * grid_ never holds an empty vector — empty cells are erased.
//     (per design decision: "unordered_map shouldn't contain any empty coordinates")
//   * objects_.at(id).pos is always consistent with grid_.at(pos) containing id.
#pragma once

#include "coord.hpp"
#include "direction.hpp"
#include "kind.hpp"
#include "object.hpp"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace baba::core {

class World {
public:
    World() = default;

    // Allocate a new object and insert it into the grid.
    ObjectId spawn(Coord pos, Kind kind, bool text, Direction facing = Direction::Right);

    // Re-insert a previously-destroyed object with its original id.
    // id must be < next_id_ (i.e., it was previously allocated by spawn).
    // next_id_ is NOT advanced. Used exclusively by undo-of-Destroy.
    void respawn(ObjectId id, Coord pos, Kind kind, Kind original_kind, bool text, Direction facing);

    // Mutators (return false if id unknown).
    bool move(ObjectId id, Coord new_pos);
    bool face(ObjectId id, Direction d);
    bool destroy(ObjectId id);
    bool retype(ObjectId id, Kind new_kind);     // change kind in place; same id/pos/facing
    bool flip_text(ObjectId id);                 // toggle text bool in place (IS TEXT)

    // Read-only access.
    Object const* get(ObjectId id) const;
    std::vector<ObjectId> const& at(Coord pos) const;
    bool occupied(Coord pos) const;

    // Iteration. Both return ascending-id order for determinism.
    std::vector<ObjectId> all_ids() const;
    std::vector<Coord>    all_cells() const;  // ascending (y, x); only non-empty

    std::size_t object_count() const { return objects_.size(); }
    std::size_t cell_count()   const { return grid_.size();    }

    ObjectId next_id_preview() const { return next_id_; }

private:
    std::unordered_map<ObjectId, Object>                          objects_;
    std::unordered_map<Coord, std::vector<ObjectId>, CoordHash>   grid_;
    ObjectId                                                       next_id_{0};

    static std::vector<ObjectId> const& empty_cell_();
};

}  // namespace baba::core
