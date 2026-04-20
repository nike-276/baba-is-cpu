#include "editor.hpp"

#include "core/loader.hpp"

#include <algorithm>
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
        if (o) changes.push_back(Change::destroy(id, o->pos, o->kind, o->original_kind, o->text, o->facing));
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
                sim_.world().respawn(c.id, c.obj_pos, c.obj_kind, c.obj_original_kind, c.obj_text, c.obj_facing);
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

void Editor::palette_select(int idx) {
    int n = static_cast<int>(palette_.size());
    palette_idx_ = std::clamp(idx, 0, n - 1);
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

void Editor::copy_rect(Coord a, Coord b) {
    Coord lo{std::min(a.x, b.x), std::min(a.y, b.y)};
    Coord hi{std::max(a.x, b.x), std::max(a.y, b.y)};

    clipboard_world_ = World{};
    has_clipboard_ = false;

    for (int y = lo.y; y <= hi.y; ++y) {
        for (int x = lo.x; x <= hi.x; ++x) {
            Coord pos{x, y};
            for (ObjectId id : sim_.world().at(pos)) {
                Object const* o = sim_.world().get(id);
                if (!o) continue;
                // Store offset from lo corner.
                clipboard_world_.spawn({x - lo.x, y - lo.y}, o->kind, o->text, o->facing);
                has_clipboard_ = true;
            }
        }
    }
}

void Editor::cut_rect(Coord a, Coord b) {
    copy_rect(a, b);
    if (!has_clipboard_) return;

    Coord lo{std::min(a.x, b.x), std::min(a.y, b.y)};
    Coord hi{std::max(a.x, b.x), std::max(a.y, b.y)};

    std::vector<Change> changes;
    for (int y = lo.y; y <= hi.y; ++y) {
        for (int x = lo.x; x <= hi.x; ++x) {
            Coord pos{x, y};
            auto const& ids = sim_.world().at(pos);
            if (ids.empty()) continue;
            std::vector<ObjectId> to_delete{ids.begin(), ids.end()};
            for (ObjectId id : to_delete) {
                Object const* o = sim_.world().get(id);
                if (o) changes.push_back(Change::destroy(id, o->pos, o->kind, o->original_kind, o->text, o->facing));
            }
            for (ObjectId id : to_delete) sim_.world().destroy(id);
        }
    }
    if (!changes.empty()) {
        edit_undo_.push(std::move(changes));
        dirty_ = true;
    }
}

void Editor::paste_at(Coord target) {
    if (!has_clipboard_) return;

    std::vector<Change> changes;
    for (ObjectId id : clipboard_world_.all_ids()) {
        Object const* o = clipboard_world_.get(id);
        if (!o) continue;
        Coord dest{target.x + o->pos.x, target.y + o->pos.y};
        if (o->text) {
            bool has_text = false;
            for (ObjectId eid : sim_.world().at(dest)) {
                Object const* ex = sim_.world().get(eid);
                if (ex && ex->text) { has_text = true; break; }
            }
            if (has_text) continue;
        }
        ObjectId new_id = sim_.world().spawn(dest, o->kind, o->text, o->facing);
        changes.push_back(Change::spawn(new_id));
    }
    if (!changes.empty()) {
        edit_undo_.push(std::move(changes));
        dirty_ = true;
    }
}

bool Editor::save_selection_as_schem(Coord a, Coord b,
                                      std::vector<Coord> const& input_tags,
                                      std::vector<Coord> const& output_tags,
                                      std::string const& path) {
    Coord lo{std::min(a.x, b.x), std::min(a.y, b.y)};
    Coord hi{std::max(a.x, b.x), std::max(a.y, b.y)};

    Schematic schem;
    schem.name   = path;
    schem.origin = lo;

    for (int y = lo.y; y <= hi.y; ++y) {
        for (int x = lo.x; x <= hi.x; ++x) {
            Coord pos{x, y};
            for (ObjectId id : sim_.world().at(pos)) {
                Object const* o = sim_.world().get(id);
                if (!o) continue;
                schem.world.spawn(o->pos, o->kind, o->text, o->facing);
            }
        }
    }

    for (Coord c : input_tags)  schem.tags.push_back({c, SchemTag::Type::Input});
    for (Coord c : output_tags) schem.tags.push_back({c, SchemTag::Type::Output});

    std::ofstream out(path);
    if (!out) return false;
    out << serialize_schematic(schem);
    return out.good();
}

bool Editor::paste_schematic(Schematic const& schem, Coord target, int rotation_cw) {
    // Record this placement for global abstract-view rendering.
    schem_placements_.push_back({schem, target, rotation_cw});

    Schematic rotated = rotate_schematic(schem, rotation_cw);
    int32_t ox = rotated.origin.x;
    int32_t oy = rotated.origin.y;

    std::vector<Change> changes;
    for (ObjectId id : rotated.world.all_ids()) {
        Object const* o = rotated.world.get(id);
        if (!o) continue;
        Coord dest{target.x + (o->pos.x - ox), target.y + (o->pos.y - oy)};
        if (o->text) {
            bool has_text = false;
            for (ObjectId eid : sim_.world().at(dest)) {
                Object const* ex = sim_.world().get(eid);
                if (ex && ex->text) { has_text = true; break; }
            }
            if (has_text) continue;
        }
        ObjectId new_id = sim_.world().spawn(dest, o->kind, o->text, o->facing);
        changes.push_back(Change::spawn(new_id));
    }
    if (!changes.empty()) {
        edit_undo_.push(std::move(changes));
        dirty_ = true;
    }

    // Recursively paste nested schematics.
    for (auto const& ref : rotated.nested) {
        auto result = load_schematic_file(ref.path);
        if (std::holds_alternative<Schematic>(result)) {
            Coord nested_target{target.x + (ref.pos.x - ox), target.y + (ref.pos.y - oy)};
            paste_schematic(std::get<Schematic>(result), nested_target, rotation_cw);
        }
    }

    return true;
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
