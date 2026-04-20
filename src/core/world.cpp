#include "world.hpp"

#include <algorithm>
#include <utility>

namespace baba::core {

std::vector<ObjectId> const& World::empty_cell_() {
    static std::vector<ObjectId> const kEmpty;
    return kEmpty;
}

ObjectId World::spawn(Coord pos, Kind kind, bool text, Direction facing) {
    ObjectId id = next_id_++;
    objects_.emplace(id, Object{id, pos, kind, kind, text, facing});
    grid_[pos].push_back(id);
    return id;
}

void World::respawn(ObjectId id, Coord pos, Kind kind, Kind original_kind, bool text, Direction facing) {
    objects_.emplace(id, Object{id, pos, kind, original_kind, text, facing});
    grid_[pos].push_back(id);
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
        if (v.empty()) grid_.erase(cell_it);
    }

    grid_[new_pos].push_back(id);
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
    auto cell_it = grid_.find(pos);
    if (cell_it != grid_.end()) {
        auto& v = cell_it->second;
        v.erase(std::remove(v.begin(), v.end(), id), v.end());
        if (v.empty()) grid_.erase(cell_it);
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

std::vector<ObjectId> World::all_ids() const {
    std::vector<ObjectId> ids;
    ids.reserve(objects_.size());
    for (auto const& [id, _] : objects_) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<Coord> World::all_cells() const {
    std::vector<Coord> cells;
    cells.reserve(grid_.size());
    for (auto const& [c, _] : grid_) cells.push_back(c);
    std::sort(cells.begin(), cells.end(), [](Coord a, Coord b) {
        if (a.y != b.y) return a.y < b.y;
        return a.x < b.x;
    });
    return cells;
}

}  // namespace baba::core
