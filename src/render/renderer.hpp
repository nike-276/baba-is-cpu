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

    // Draw all objects in the world. scroll_x/scroll_y are in tile units (float for sub-tile precision).
    void draw_world(core::World const& world, float scroll_x, float scroll_y) const;

    // Draw a light grid overlay.
    void draw_grid(int viewport_w, int viewport_h, float scroll_x, float scroll_y) const;

    // Draw active rules as text in a panel at (px, py).
    void draw_rule_panel(core::RuleSet const& rs, int px, int py) const;

    // Draw the palette selector on the left side.
    // entries: filtered display names (already filtered by caller).
    // selected_idx: index within `entries` that is currently selected (-1 = none visible).
    // scroll_px: vertical pixel offset into the entry list.
    // search_text / search_active: for rendering the search box at the top.
    // Returns the maximum valid scroll_px so the caller can clamp.
    int draw_palette(std::vector<std::string> const& entries,
                     int selected_idx, int px, int py,
                     int scroll_px, int panel_h,
                     std::string const& search_text, bool search_active) const;

    // Draw the bottom HUD bar.
    void draw_hud(std::string const& mode_label, std::string const& filename,
                  int tick, bool won) const;

    // Tile-to-screen and screen-to-tile coordinate conversions.
    Vector2 tile_to_screen(int tx, int ty, float scroll_x, float scroll_y) const;
    Vector2 screen_to_tile_f(float px, float py, float scroll_x, float scroll_y) const;

    int tile_px() const { return tile_px_; }
    void set_tile_px(int px) { tile_px_ = std::max(8, std::min(px, 128)); }

private:
    int tile_px_;

    void draw_tile(core::Object const& obj, int screen_x, int screen_y,
                   int layer, int total_layers) const;
};

}  // namespace baba::render
