#pragma once

#include "core/ruleset.hpp"
#include "core/world.hpp"

#include <raylib.h>
#include <string>
#include <vector>

namespace baba::render {

// Renders a World using colored rectangles + text labels (no sprites).
// Pure consumer of const World& — never mutates simulation state.
class Renderer {
public:
    explicit Renderer(int tile_px = 48);

    // Draw all objects in the world. scroll_x/scroll_y are in tile units.
    void draw_world(core::World const& world, int scroll_x, int scroll_y) const;

    // Draw a light grid overlay.
    void draw_grid(int viewport_w, int viewport_h, int scroll_x, int scroll_y) const;

    // Draw active rules as text in a panel at (px, py).
    void draw_rule_panel(core::RuleSet const& rs, int px, int py) const;

    // Draw the palette selector on the left side.
    // entries: list of display names; selected_idx: highlighted entry.
    void draw_palette(std::vector<std::string> const& entries,
                      int selected_idx, int px, int py) const;

    // Draw the bottom HUD bar.
    void draw_hud(std::string const& mode_label, std::string const& filename,
                  int tick, bool won) const;

    // Tile-to-screen and screen-to-tile coordinate conversions.
    Vector2 tile_to_screen(int tx, int ty, int scroll_x, int scroll_y) const;
    Vector2 screen_to_tile_f(int px, int py, int scroll_x, int scroll_y) const;

    int tile_px() const { return tile_px_; }
    void set_tile_px(int px) { tile_px_ = std::max(8, std::min(px, 128)); }

private:
    int tile_px_;

    void draw_tile(core::Object const& obj, int screen_x, int screen_y,
                   int layer, int total_layers) const;
};

}  // namespace baba::render
