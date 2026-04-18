#include "editor.hpp"

#include "core/loader.hpp"

#include <fstream>
#include <sstream>

namespace baba::editor {

using namespace baba::core;

Editor::Editor(sim::Simulator sim) : sim_{std::move(sim)} {}

void Editor::enter_play() {
    if (mode_ == EditorMode::Play) return;
    snapshot_       = sim_.world();  // stash clone
    snapshot_valid_ = true;
    mode_           = EditorMode::Play;
}

void Editor::enter_edit() {
    if (mode_ == EditorMode::Edit) return;
    if (snapshot_valid_) {
        sim_.world() = snapshot_;
        // Reset tick counter by constructing a fresh Simulator from the snapshot.
        sim_ = sim::Simulator(snapshot_);
    }
    mode_ = EditorMode::Edit;
}

bool Editor::place_object(Coord pos, Kind kind, bool is_text, Direction facing) {
    // Coexistence check: at most one text per tile.
    if (is_text) {
        for (ObjectId id : sim_.world().at(pos)) {
            Object const* o = sim_.world().get(id);
            if (o && o->text) return false;  // already a text tile here
        }
    }

    ObjectId new_id = sim_.world().spawn(pos, kind, is_text, facing);
    // Record spawn as an edit undo entry.
    std::vector<Change> changes;
    changes.push_back(Change::spawn(new_id));
    edit_undo_.push(std::move(changes));

    dirty_ = true;
    return true;
}

bool Editor::delete_all_at(Coord pos) {
    auto const& ids = sim_.world().at(pos);
    if (ids.empty()) return false;

    // Snapshot all objects at this tile before destroying them.
    std::vector<Change> changes;
    // Copy ids since at() returns a reference that changes as we destroy.
    std::vector<ObjectId> to_delete{ids.begin(), ids.end()};
    for (ObjectId id : to_delete) {
        Object const* o = sim_.world().get(id);
        if (o) changes.push_back(Change::destroy(id, o->pos, o->kind, o->text, o->facing));
    }
    for (ObjectId id : to_delete) sim_.world().destroy(id);

    edit_undo_.push(std::move(changes));
    dirty_ = true;
    return true;
}

bool Editor::undo_edit() {
    auto maybe = edit_undo_.pop();
    if (!maybe) return false;

    auto const& changes = *maybe;
    for (auto it = changes.rbegin(); it != changes.rend(); ++it) {
        auto const& c = *it;
        switch (c.kind) {
            case ChangeKind::Spawn:
                sim_.world().destroy(c.id);
                break;
            case ChangeKind::Destroy:
                sim_.world().spawn(c.obj_pos, c.obj_kind, c.obj_text, c.obj_facing);
                break;
            default: break;  // edit ops only produce Spawn/Destroy
        }
    }
    dirty_ = true;
    return true;
}

core::TickReport Editor::play_step(core::Input input) {
    return sim_.step_forward(input);
}

bool Editor::play_undo() {
    return sim_.step_back();
}

void Editor::palette_next() {
    palette_idx_ = (palette_idx_ + 1) % static_cast<int>(palette_.size());
}

void Editor::palette_prev() {
    palette_idx_ = (palette_idx_ - 1 + static_cast<int>(palette_.size()))
                   % static_cast<int>(palette_.size());
}

void Editor::toggle_text_variant() {
    auto& e = palette_[palette_idx_];
    if (!is_noun(e.kind)) return;  // operators/properties have no object twin
    e.is_text = !e.is_text;
}

void Editor::rotate_facing() {
    auto& e = palette_[palette_idx_];
    switch (e.default_facing) {
        case Direction::Right: e.default_facing = Direction::Up;    break;
        case Direction::Up:    e.default_facing = Direction::Left;  break;
        case Direction::Left:  e.default_facing = Direction::Down;  break;
        case Direction::Down:  e.default_facing = Direction::Right; break;
    }
}

std::string Editor::serialize() const {
    LoadedLevel lv;
    lv.world = sim_.world();
    lv.name  = path_.empty() ? "untitled" : path_;
    return serialize_level(lv);
}

bool Editor::load_level(std::string const& path) {
    auto result = load_level_file(path);
    if (std::holds_alternative<ParseError>(result)) return false;
    auto& loaded = std::get<LoadedLevel>(result);
    sim_   = sim::Simulator(std::move(loaded.world));
    path_  = path;
    dirty_ = false;
    snapshot_valid_ = false;
    mode_  = EditorMode::Edit;
    return true;
}

}  // namespace baba::editor
