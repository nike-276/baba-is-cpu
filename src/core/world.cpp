#include "world.hpp"

#include <algorithm>
#include <utility>

namespace baba::core {

std::vector<ObjectId> const& World::empty_cell_() {
    static std::vector<ObjectId> const kEmpty;
    return kEmpty;
}

void World::add_id_(ObjectId id) {
    id_to_idx_[id] = ids_vec_.size();
    ids_vec_.push_back(id);
}

void World::remove_id_(ObjectId id) {
    auto it = id_to_idx_.find(id);
    if (it == id_to_idx_.end()) return;
    std::size_t idx = it->second;
    id_to_idx_.erase(it);
    if (idx != ids_vec_.size() - 1) {
        ObjectId last = ids_vec_.back();
        ids_vec_[idx] = last;
        id_to_idx_[last] = idx;
    }
    ids_vec_.pop_back();
}

void World::add_cell_(Coord c) {
    cell_to_idx_[c] = cells_vec_.size();
    cells_vec_.push_back(c);
}

void World::remove_cell_(Coord c) {
    auto it = cell_to_idx_.find(c);
    if (it == cell_to_idx_.end()) return;
    std::size_t idx = it->second;
    cell_to_idx_.erase(it);
    if (idx != cells_vec_.size() - 1) {
        Coord last = cells_vec_.back();
        cells_vec_[idx] = last;
        cell_to_idx_[last] = idx;
    }
    cells_vec_.pop_back();
}

ObjectId World::spawn(Coord pos, Kind kind, bool text, Direction facing) {
    ObjectId id = next_id_++;
    objects_.emplace(id, Object{id, pos, kind, kind, text, facing});
    add_id_(id);
    auto [it, inserted] = grid_.try_emplace(pos);
    it->second.push_back(id);
    if (inserted) add_cell_(pos);
    return id;
}

void World::respawn(ObjectId id, Coord pos, Kind kind, Kind original_kind, bool text, Direction facing) {
    objects_.emplace(id, Object{id, pos, kind, original_kind, text, facing});
    add_id_(id);
    auto [it, inserted] = grid_.try_emplace(pos);
    it->second.push_back(id);
    if (inserted) add_cell_(pos);
}

bool World::move(ObjectId id, Coord new_pos) {
    auto it = objects_.find(id);
    if (it == objects_.end()) return false;
    Coord old_pos = it->second.pos;
    if (old_pos == new_pos) return true;

    auto cell_it = grid_.find(old_pos);
    if (cell_it != grid_.end()) {
        auto& v = cell_it->second;
        v.erase(std::remove(v.begin(), v.end(), id), v.end());
        if (v.empty()) {
            grid_.erase(cell_it);
            remove_cell_(old_pos);
        }
    }

    auto [new_cell_it, inserted] = grid_.try_emplace(new_pos);
    new_cell_it->second.push_back(id);
    if (inserted) add_cell_(new_pos);

    it->second.pos = new_pos;
    return true;
}

bool World::face(ObjectId id, Direction d) {
    auto it = objects_.find(id);
    if (it == objects_.end()) return false;
    it->second.facing = d;
    return true;
}

bool World::retype(ObjectId id, Kind new_kind) {
    auto it = objects_.find(id);
    if (it == objects_.end()) return false;
    it->second.kind = new_kind;
    return true;
}

bool World::flip_text(ObjectId id) {
    auto it = objects_.find(id);
    if (it == objects_.end()) return false;
    it->second.text = !it->second.text;
    return true;
}

bool World::set_original_kind(ObjectId id, Kind orig_kind) {
    auto it = objects_.find(id);
    if (it == objects_.end()) return false;
    it->second.original_kind = orig_kind;
    return true;
}

bool World::destroy(ObjectId id) {
    auto it = objects_.find(id);
    if (it == objects_.end()) return false;
    Coord pos = it->second.pos;
    objects_.erase(it);
    remove_id_(id);
    auto cell_it = grid_.find(pos);
    if (cell_it != grid_.end()) {
        auto& v = cell_it->second;
        v.erase(std::remove(v.begin(), v.end(), id), v.end());
        if (v.empty()) {
            grid_.erase(cell_it);
            remove_cell_(pos);
        }
    }
    return true;
}

Object const* World::get(ObjectId id) const {
    auto it = objects_.find(id);
    return (it == objects_.end()) ? nullptr : &it->second;
}

std::vector<ObjectId> const& World::at(Coord pos) const {
    auto it = grid_.find(pos);
    return (it == grid_.end()) ? empty_cell_() : it->second;
}

bool World::occupied(Coord pos) const {
    return grid_.find(pos) != grid_.end();
}

std::vector<ObjectId> const& World::all_ids()   const { return ids_vec_;   }
std::vector<Coord>    const& World::all_cells()  const { return cells_vec_; }

}  // namespace baba::core
