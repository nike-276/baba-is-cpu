#include "renderer.hpp"
#include "tile_colors.hpp"
#include "palette_layout.hpp"

#include "core/object.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace baba::render {

Renderer::Renderer(int tile_px) : tile_px_{tile_px} {}

Vector2 Renderer::tile_to_screen(int tx, int ty, float scroll_x, float scroll_y) const {
    return {
        (tx - scroll_x) * tile_px_,
        (ty - scroll_y) * tile_px_
    };
}

Vector2 Renderer::screen_to_tile_f(float px, float py, float scroll_x, float scroll_y) const {
    return {
        px / tile_px_ + scroll_x,
        py / tile_px_ + scroll_y
    };
}

void Renderer::draw_tile(core::Object const& obj, int sx, int sy,
                          int layer, int total_layers) const {
    TileStyle style = style_for(obj.kind, obj.text);

    // Vertical offset when multiple objects share a tile.
    int offset = (total_layers > 1) ? layer * (tile_px_ / total_layers) : 0;
    int h = (total_layers > 1) ? tile_px_ / total_layers : tile_px_;

    Rectangle rect{
        static_cast<float>(sx + 1),
        static_cast<float>(sy + offset + 1),
        static_cast<float>(tile_px_ - 2),
        static_cast<float>(h - 2)
    };

    DrawRectangleRec(rect, style.bg);

    // Border for text tiles.
    if (obj.text) {
        DrawRectangleLinesEx(rect, 2, DARKGRAY);
    }

    // Label (truncated to fit).
    int font_size = std::max(8, tile_px_ / 4);
    const char* label = style.label;
    int text_w = MeasureText(label, font_size);
    int tx_pos = sx + (tile_px_ - text_w) / 2;
    int ty_pos = sy + offset + (h - font_size) / 2;
    DrawText(label, tx_pos, ty_pos, font_size, style.text_fg);
}

void Renderer::draw_world(core::World const& world,
                           float scroll_x, float scroll_y) const {
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();

    // Only draw tiles visible in the viewport (plus a 1-tile margin).
    int min_tx = static_cast<int>(scroll_x) - 1;
    int max_tx = static_cast<int>(scroll_x) + screen_w / tile_px_ + 2;
    int min_ty = static_cast<int>(scroll_y) - 1;
    int max_ty = static_cast<int>(scroll_y) + screen_h / tile_px_ + 2;

    for (core::Coord c : world.all_cells()) {
        if (c.x < min_tx || c.x > max_tx || c.y < min_ty || c.y > max_ty) continue;

        auto const& ids = world.at(c);
        if (ids.empty()) continue;

        int sx = static_cast<int>((c.x - scroll_x) * tile_px_);
        int sy = static_cast<int>((c.y - scroll_y) * tile_px_);

        // Sort objects: non-text first (ascending id), then text (ascending id).
        std::vector<core::ObjectId> sorted = ids;
        std::sort(sorted.begin(), sorted.end(), [&](core::ObjectId a, core::ObjectId b) {
            core::Object const* oa = world.get(a);
            core::Object const* ob = world.get(b);
            if (!oa || !ob) return a < b;
            if (oa->text != ob->text) return !oa->text;  // non-text first
            return a < b;
        });

        int total = static_cast<int>(sorted.size());
        for (int i = 0; i < total; ++i) {
            core::Object const* o = world.get(sorted[i]);
            if (o) draw_tile(*o, sx, sy, i, total);
        }
    }
}

void Renderer::draw_grid(int viewport_w, int viewport_h,
                          float scroll_x, float scroll_y) const {
    Color grid_color = {50, 50, 50, 180};

    // Sub-tile fractional offset: aligns grid lines with world-tile boundaries.
    // floor(scroll) - scroll is in (-1, 0], so * tile_px_ gives first line <= 0.
    int start_x = static_cast<int>((std::floor(scroll_x) - scroll_x) * tile_px_);
    for (int x = start_x; x < viewport_w; x += tile_px_) {
        DrawLine(x, 0, x, viewport_h, grid_color);
    }
    int start_y = static_cast<int>((std::floor(scroll_y) - scroll_y) * tile_px_);
    for (int y = start_y; y < viewport_h; y += tile_px_) {
        DrawLine(0, y, viewport_w, y, grid_color);
    }
}

void Renderer::draw_rule_panel(core::RuleSet const& rs, int px, int py) const {
    int font_size = 14;
    int line_h    = font_size + 4;
    int y         = py;

    DrawText("ACTIVE RULES", px, y, font_size, RAYWHITE);
    y += line_h + 4;

    auto const& rules = rs.property_rules();
    if (rules.empty()) {
        DrawText("(none)", px, y, font_size, GRAY);
        return;
    }

    for (auto const& r : rules) {
        // Skip the base TEXT IS PUSH display clutter unless it's a real rule.
        std::string line = std::string(core::kind_name(r.subject)) + " IS "
                         + std::string(core::kind_name(r.property));
        DrawText(line.c_str(), px, y, font_size, RAYWHITE);
        y += line_h;
        if (y > GetScreenHeight() - 20) break;  // don't overflow
    }
}

int Renderer::draw_palette(std::vector<std::string> const& entries,
                            int selected_idx, int px, int py,
                            int scroll_px, int panel_h,
                            std::string const& search_text, bool search_active) const {
    constexpr int SEARCH_H = PALETTE_SEARCH_H;
    int entry_h   = PALETTE_ENTRY_H;
    int font_size = std::max(8, PALETTE_PX / 5);
    int panel_w   = PALETTE_W;

    DrawText("PALETTE", px, py - 18, 12, LIGHTGRAY);

    // ── Search box ────────────────────────────────────────────────────────
    Rectangle sb{static_cast<float>(px), static_cast<float>(py),
                 static_cast<float>(panel_w), static_cast<float>(SEARCH_H)};
    DrawRectangleRec(sb, search_active ? WHITE : Color{40, 40, 40, 255});
    DrawRectangleLinesEx(sb, 1, search_active ? YELLOW : GRAY);

    // Blinking cursor when active.
    bool show_cursor = search_active && (static_cast<int>(GetTime() * 2) % 2 == 0);
    std::string display = search_text + (show_cursor ? "|" : "");
    if (display.empty() && !search_active) display = "search...";
    Color text_col = search_active ? BLACK : (search_text.empty() ? DARKGRAY : LIGHTGRAY);
    DrawText(display.c_str(), px + 4, py + (SEARCH_H - 12) / 2, 12, text_col);

    // ── Entry list ────────────────────────────────────────────────────────
    int entry_start = py + SEARCH_H + 4;
    int visible_h   = panel_h - SEARCH_H - 4;
    int total_h     = static_cast<int>(entries.size()) * entry_h;
    int max_scroll  = std::max(0, total_h - visible_h);

    BeginScissorMode(px, entry_start, panel_w, visible_h);

    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        int ey = entry_start + i * entry_h - scroll_px;
        if (ey + entry_h < entry_start || ey > entry_start + visible_h) continue;

        Rectangle bg{static_cast<float>(px), static_cast<float>(ey),
                     static_cast<float>(panel_w), static_cast<float>(PALETTE_PX + 2)};

        if (i == selected_idx) {
            DrawRectangleRec(bg, WHITE);
            DrawRectangleLinesEx(bg, 2, YELLOW);
        } else {
            DrawRectangleRec(bg, DARKGRAY);
        }

        Color tc = (i == selected_idx) ? BLACK : LIGHTGRAY;
        DrawText(entries[i].c_str(), px + 2, ey + (PALETTE_PX - font_size) / 2, font_size, tc);
    }

    EndScissorMode();
    return max_scroll;
}

void Renderer::draw_hud(std::string const& mode_label, std::string const& filename,
                         int tick, bool won) const {
    int h = GetScreenHeight();
    int w = GetScreenWidth();

    // Bottom bar background.
    DrawRectangle(0, h - 24, w, 24, {20, 20, 20, 220});

    Color mode_col = (mode_label == "PLAY") ? GREEN : SKYBLUE;
    DrawText(mode_label.c_str(), 8, h - 20, 14, mode_col);

    std::string info = filename.empty() ? "(untitled)" : filename;
    info += "  tick:" + std::to_string(tick);
    DrawText(info.c_str(), 80, h - 20, 12, LIGHTGRAY);

    if (won) {
        DrawText("YOU WIN!", w / 2 - 60, h / 2 - 20, 40, YELLOW);
    }

    // Mode hint.
    const char* hint = (mode_label == "PLAY")
        ? "arrows:move  Z:undo  ESC:edit"
        : "LMB:place  RMB:del  Q/E:palette  Enter:play  Ctrl+S:save";
    DrawText(hint, w - MeasureText(hint, 11) - 8, h - 20, 11, DARKGRAY);
}

}  // namespace baba::render
