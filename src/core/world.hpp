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
    bool retype(ObjectId id, Kind new_kind);             // change kind in place; same id/pos/facing
    bool flip_text(ObjectId id);                         // toggle text bool in place (IS TEXT)
    bool set_original_kind(ObjectId id, Kind orig_kind); // set original_kind without changing current kind

    // Read-only access.
    Object const* get(ObjectId id) const;
    std::vector<ObjectId> const& at(Coord pos) const;
    bool occupied(Coord pos) const;

    // Iteration. Order is unordered but stable within a run.
    std::vector<ObjectId> const& all_ids()   const;
    std::vector<Coord>    const& all_cells() const;  // non-empty cells only

    // Kind → ids reverse index. Text objects bucket into Kind::N_Text
    // (regardless of their underlying kind). Ids within a bucket are
    // ascending. Maintained by spawn / respawn / destroy / retype / flip_text;
    // move / face / set_original_kind do NOT change a bucket.
    std::vector<ObjectId> const& objects_of_kind(Kind k) const;

    std::size_t object_count() const { return objects_.size(); }
    std::size_t cell_count()   const { return grid_.size();    }

    ObjectId next_id_preview() const { return next_id_; }

private:
    std::unordered_map<ObjectId, Object>                          objects_;
    std::unordered_map<Coord, std::vector<ObjectId>, CoordHash>   grid_;
    ObjectId                                                       next_id_{0};

    std::vector<ObjectId>                            ids_vec_;
    std::unordered_map<ObjectId, std::size_t>        id_to_idx_;
    std::vector<Coord>                               cells_vec_;
    std::unordered_map<Coord, std::size_t, CoordHash> cell_to_idx_;

    // Kind bucket for id lookup — keyed by (text ? N_Text : kind).
    std::unordered_map<Kind, std::vector<ObjectId>>  kind_index_;

    void add_id_(ObjectId id);
    void remove_id_(ObjectId id);
    void add_cell_(Coord c);
    void remove_cell_(Coord c);

    // kind_index_ maintenance.
    Kind bucket_for_(bool text, Kind kind) const;
    void add_to_bucket_(Kind bucket, ObjectId id);
    void remove_from_bucket_(Kind bucket, ObjectId id);

    static std::vector<ObjectId> const& empty_cell_();
};

}  // namespace baba::core
