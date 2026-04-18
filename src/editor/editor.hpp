#pragma once

#include "palette.hpp"
#include "sim/simulator.hpp"
#include "sim/undo_buffer.hpp"
#include "core/loader.hpp"

#include <istream>
#include <string>
#include <vector>

namespace baba::editor {

enum class EditorMode { Edit, Play };

// Manages play/edit mode switching, object placement/deletion, and dual undo stacks.
// In Edit mode: mutations go through place_object/delete_all_at and are tracked
//               in edit_undo_ (separate from the play undo buffer).
// In Play mode: inputs are delegated to the Simulator.
// Mode toggle: enter_play() stashes a World clone; enter_edit() restores it.
class Editor {
public:
    explicit Editor(sim::Simulator sim);

    void enter_play();
    void enter_edit();
    EditorMode mode() const { return mode_; }

    // Edit-mode mutations. Return false on coexistence violation (text-on-text).
    bool place_object(core::Coord pos, core::Kind kind, bool is_text,
                      core::Direction facing = core::Direction::Right);
    bool delete_all_at(core::Coord pos);
    bool undo_edit();

    // Play-mode delegation.
    core::TickReport play_step(core::Input input);
    bool play_undo();

    // World read access (for renderer + input handler).
    core::World const& world() const { return sim_.world(); }
    core::RuleSet current_rules() const { return sim_.current_rules(); }

    // Palette state.
    std::vector<PaletteEntry> const& palette() const { return palette_; }
    PaletteEntry const& selected() const { return palette_[palette_idx_]; }
    void palette_next();
    void palette_prev();
    void palette_select(int idx);  // direct selection by index (clamped)
    void toggle_text_variant();
    void rotate_facing();

    // File I/O.
    std::string serialize() const;
    bool load_level(std::string const& path);
    std::string const& current_path() const { return path_; }
    bool dirty() const { return dirty_; }
    void set_path(std::string p) { path_ = std::move(p); }
    void mark_clean() { dirty_ = false; }

private:
    EditorMode      mode_{EditorMode::Edit};
    sim::Simulator  sim_;
    core::World     snapshot_{};
    bool            snapshot_valid_{false};

    sim::UndoBuffer edit_undo_{500};

    std::vector<PaletteEntry> palette_{default_palette()};
    int palette_idx_{0};

    std::string path_{};
    bool        dirty_{false};
};

}  // namespace baba::editor
