#pragma once

#include "palette.hpp"
#include "sim/simulator.hpp"
#include "sim/undo_buffer.hpp"
#include "core/loader.hpp"
#include "core/schematic.hpp"

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
    explicit Editor(sim::Simulator sim, std::size_t undo_cap = 10'000);

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

    // Benchmark data (accumulated from play_step calls via Simulator).
    sim::BenchReport bench_report() const { return sim_.bench_report(); }
    void             reset_bench()        { sim_.reset_bench(); }

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

    // Clipboard: copy/cut selected rect, paste at target tile.
    // clipboard_world() has objects with positions normalized to (0,0)-based offset.
    void copy_rect(core::Coord a, core::Coord b);
    void cut_rect (core::Coord a, core::Coord b);
    void paste_at (core::Coord target);
    bool has_clipboard() const { return has_clipboard_; }
    core::World const& clipboard_world() const { return clipboard_world_; }

    // Schematics: save selection as .schem; paste a loaded schematic.
    bool save_selection_as_schem(core::Coord a, core::Coord b,
                                 std::vector<core::Coord> const& input_tags,
                                 std::vector<core::Coord> const& output_tags,
                                 std::string const& path);

    bool paste_schematic(core::Schematic const& schem, core::Coord target, int rotation_cw);

private:
    EditorMode      mode_{EditorMode::Edit};
    sim::Simulator  sim_;
    std::size_t     undo_cap_{10'000};
    core::World     snapshot_{};
    bool            snapshot_valid_{false};

    sim::UndoBuffer edit_undo_{500};

    std::vector<PaletteEntry> palette_{default_palette()};
    int palette_idx_{0};

    std::string path_{};
    bool        dirty_{false};

    core::World clipboard_world_{};
    bool        has_clipboard_{false};

public:
    // Schematic placements: tracked for global abstract-view rendering.
    struct SchematicPlacement {
        core::Schematic schem;
        core::Coord     target;
        int             rotation_cw;
    };
    std::vector<SchematicPlacement> const& schem_placements() const { return schem_placements_; }
    void record_schem_placement(core::Schematic const& schem, core::Coord target, int rotation_cw) {
        schem_placements_.push_back({schem, target, rotation_cw});
    }

private:
    std::vector<SchematicPlacement> schem_placements_{};
};

}  // namespace baba::editor
