#include "renderer.hpp"
#include "tile_colors.hpp"
#include "palette_layout.hpp"

#include "core/object.hpp"
#include "core/schematic.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_set>

namespace baba::render {

Renderer::Renderer(float tile_px) : tile_px_{tile_px} {}

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
                          int layer, int total_layers, unsigned char alpha) const {
    TileStyle style = style_for(obj.kind, obj.text);

    // Vertical offset when multiple objects share a tile.
    float layer_h = tile_px_ / static_cast<float>(total_layers);
    int offset = (total_layers > 1) ? static_cast<int>(std::roundf(layer * layer_h)) : 0;
    int h      = (total_layers > 1) ? static_cast<int>(std::roundf(layer_h)) : static_cast<int>(tile_px_);

    Rectangle rect{
        static_cast<float>(sx + 1),
        static_cast<float>(sy + offset + 1),
        tile_px_ - 2.0f,
        static_cast<float>(h - 2)
    };

    Color bg  = {style.bg.r,      style.bg.g,      style.bg.b,      alpha};
    Color fg  = {style.text_fg.r, style.text_fg.g, style.text_fg.b, alpha};

    // Try to draw a sprite texture; fall back to colored rect.
    Texture2D const* tex = atlas_ ? atlas_->get(obj.kind, obj.text) : nullptr;
    if (tex && tex->id != 0) {
        Color sprite_bg = {20, 20, 30, alpha};  // dark neutral — sprites have transparent bg
        DrawRectangleRec(rect, sprite_bg);
        Rectangle src{0, 0, static_cast<float>(tex->width), static_cast<float>(tex->height)};
        DrawTexturePro(*tex, src, rect, {0, 0}, 0.0f, {255, 255, 255, alpha});
    } else {
        DrawRectangleRec(rect, bg);
    }

    // Border for text tiles.
    if (obj.text) {
        Color border = {DARKGRAY.r, DARKGRAY.g, DARKGRAY.b, alpha};
        DrawRectangleLinesEx(rect, 2, border);
    }

    // Label only when no sprite available.
    if (!tex || tex->id == 0) {
        int font_size = std::max(8, static_cast<int>(tile_px_) / 4);
        const char* label = style.label;
        int text_w = MeasureText(label, font_size);
        int tx_pos = sx + static_cast<int>((tile_px_ - static_cast<float>(text_w)) / 2.0f);
        int ty_pos = sy + offset + (h - font_size) / 2;
        DrawText(label, tx_pos, ty_pos, font_size, fg);
    }

    // Direction indicator: triangle at the facing edge (not center) so it doesn't block text.
    if (!obj.text && tile_px_ >= 16.0f) {
        float cx = sx + tile_px_ * 0.5f;
        float cy = sy + static_cast<float>(offset) + static_cast<float>(h) * 0.5f;
        float ar = std::min(tile_px_, static_cast<float>(h)) * 0.18f;

        float ax = 0.0f, ay = 0.0f;
        switch (obj.facing) {
            case core::Direction::Right: ax =  1.0f; ay =  0.0f; break;
            case core::Direction::Left:  ax = -1.0f; ay =  0.0f; break;
            case core::Direction::Up:    ax =  0.0f; ay = -1.0f; break;
            case core::Direction::Down:  ax =  0.0f; ay =  1.0f; break;
        }
        float px = -ay, py = ax;  // perpendicular (CCW)

        // Center the triangle near the facing edge (tip points to the edge).
        float half_w = tile_px_ * 0.46f;
        float half_h = static_cast<float>(h) * 0.46f;
        float tc_x = cx + ax * (half_w - ar);
        float tc_y = cy + ay * (half_h - ar);

        Vector2 tip    = {tc_x + ax * ar,                 tc_y + ay * ar};
        Vector2 wing_a = {tc_x - ax * ar * 0.5f + px * ar * 0.65f, tc_y - ay * ar * 0.5f + py * ar * 0.65f};
        Vector2 wing_b = {tc_x - ax * ar * 0.5f - px * ar * 0.65f, tc_y - ay * ar * 0.5f - py * ar * 0.65f};

        Color ac = {255, 255, 255, static_cast<unsigned char>(alpha * 200 / 255)};
        DrawTriangle(tip, wing_b, wing_a, ac);  // CCW on screen (y-down)
    }
}

void Renderer::draw_world(core::World const& world,
                           float scroll_x, float scroll_y) const {
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();

    // Only draw tiles visible in the viewport (plus a 1-tile margin).
    int min_tx = static_cast<int>(scroll_x) - 1;
    int max_tx = static_cast<int>(scroll_x) + static_cast<int>(screen_w / tile_px_) + 2;
    int min_ty = static_cast<int>(scroll_y) - 1;
    int max_ty = static_cast<int>(scroll_y) + static_cast<int>(screen_h / tile_px_) + 2;

    for (core::Coord c : world.all_cells()) {
        if (c.x < min_tx || c.x > max_tx || c.y < min_ty || c.y > max_ty) continue;

        auto const& ids = world.at(c);
        if (ids.empty()) continue;

        // Use roundf so pixel positions are consistent with the grid lines.
        int sx = static_cast<int>(std::roundf((c.x - scroll_x) * tile_px_));
        int sy = static_cast<int>(std::roundf((c.y - scroll_y) * tile_px_));

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

    // Sub-tile fractional offset keeps grid lines pinned to world-tile edges.
    float start_xf = (std::floor(scroll_x) - scroll_x) * tile_px_;
    for (float x = start_xf; x < static_cast<float>(viewport_w); x += tile_px_) {
        int xi = static_cast<int>(std::roundf(x));
        DrawLine(xi, 0, xi, viewport_h, grid_color);
    }
    float start_yf = (std::floor(scroll_y) - scroll_y) * tile_px_;
    for (float y = start_yf; y < static_cast<float>(viewport_h); y += tile_px_) {
        int yi = static_cast<int>(std::roundf(y));
        DrawLine(0, yi, viewport_w, yi, grid_color);
    }
}

void Renderer::draw_world_at(core::World const& world, core::Coord offset,
                              float scroll_x, float scroll_y, unsigned char alpha) const {
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    int min_tx = static_cast<int>(scroll_x) - 1;
    int max_tx = static_cast<int>(scroll_x) + static_cast<int>(screen_w / tile_px_) + 2;
    int min_ty = static_cast<int>(scroll_y) - 1;
    int max_ty = static_cast<int>(scroll_y) + static_cast<int>(screen_h / tile_px_) + 2;

    for (core::Coord c : world.all_cells()) {
        int wx = c.x + offset.x;
        int wy = c.y + offset.y;
        if (wx < min_tx || wx > max_tx || wy < min_ty || wy > max_ty) continue;

        auto const& ids = world.at(c);
        if (ids.empty()) continue;

        int sx = static_cast<int>(std::roundf((wx - scroll_x) * tile_px_));
        int sy = static_cast<int>(std::roundf((wy - scroll_y) * tile_px_));

        std::vector<core::ObjectId> sorted = ids;
        std::sort(sorted.begin(), sorted.end(), [&](core::ObjectId a, core::ObjectId b) {
            core::Object const* oa = world.get(a);
            core::Object const* ob = world.get(b);
            if (!oa || !ob) return a < b;
            if (oa->text != ob->text) return !oa->text;
            return a < b;
        });

        int total = static_cast<int>(sorted.size());
        for (int i = 0; i < total; ++i) {
            core::Object const* o = world.get(sorted[i]);
            if (o) draw_tile(*o, sx, sy, i, total, alpha);
        }
    }
}

void Renderer::draw_selection(core::Coord a, core::Coord b,
                               float scroll_x, float scroll_y) const {
    core::Coord lo{std::min(a.x, b.x), std::min(a.y, b.y)};
    core::Coord hi{std::max(a.x, b.x), std::max(a.y, b.y)};

    float sx = std::roundf((lo.x - scroll_x) * tile_px_);
    float sy = std::roundf((lo.y - scroll_y) * tile_px_);
    float sw = (hi.x - lo.x + 1) * tile_px_;
    float sh = (hi.y - lo.y + 1) * tile_px_;

    // Semi-transparent fill.
    DrawRectangle(static_cast<int>(sx), static_cast<int>(sy),
                  static_cast<int>(sw), static_cast<int>(sh),
                  {100, 180, 255, 40});
    // Solid border.
    DrawRectangleLinesEx({sx, sy, sw, sh}, 2, {100, 180, 255, 200});
}

void Renderer::draw_schematic_normal(core::Schematic const& schem, core::Coord target,
                                      int rotation_cw, float scroll_x, float scroll_y) const {
    core::Schematic rotated = core::rotate_schematic(schem, rotation_cw);
    int32_t ox = rotated.origin.x;
    int32_t oy = rotated.origin.y;
    core::Coord offset{target.x - ox, target.y - oy};
    draw_world_at(rotated.world, offset, scroll_x, scroll_y, 128);
}

void Renderer::draw_schematic_abstract(core::Schematic const& schem, core::Coord target,
                                        int rotation_cw, float scroll_x, float scroll_y) const {
    core::Schematic rotated = core::rotate_schematic(schem, rotation_cw);
    int32_t ox = rotated.origin.x;
    int32_t oy = rotated.origin.y;

    // Collect occupied world tile positions.
    using CoordSet = std::unordered_set<core::Coord, core::CoordHash>;
    CoordSet occupied;
    int32_t min_wx = INT32_MAX, max_wx = INT32_MIN;
    int32_t min_wy = INT32_MAX, max_wy = INT32_MIN;

    for (core::Coord c : rotated.world.all_cells()) {
        int32_t wx = target.x + (c.x - ox);
        int32_t wy = target.y + (c.y - oy);
        occupied.insert({wx, wy});
        min_wx = std::min(min_wx, wx);  max_wx = std::max(max_wx, wx);
        min_wy = std::min(min_wy, wy);  max_wy = std::max(max_wy, wy);
    }
    if (occupied.empty()) return;

    // Build tag lookup.
    CoordSet input_tiles, output_tiles;
    for (auto const& tag : rotated.tags) {
        int32_t wx = target.x + (tag.pos.x - ox);
        int32_t wy = target.y + (tag.pos.y - oy);
        if (tag.type == core::SchemTag::Type::Input)  input_tiles.insert({wx, wy});
        else                                           output_tiles.insert({wx, wy});
    }

    // Also fill all tile grid positions within bounding box (gray background for extent).
    for (int32_t wy = min_wy; wy <= max_wy; ++wy) {
        for (int32_t wx = min_wx; wx <= max_wx; ++wx) {
            if (occupied.find({wx, wy}) == occupied.end()) continue;

            int sx = static_cast<int>(std::roundf((wx - scroll_x) * tile_px_));
            int sy = static_cast<int>(std::roundf((wy - scroll_y) * tile_px_));
            int tp = static_cast<int>(tile_px_);

            // Opaque-ish fill to cover underlying tile rendering.
            Color fill = {80, 80, 80, 230};
            if (input_tiles.count({wx, wy}))       fill = {50, 90, 210, 235};
            else if (output_tiles.count({wx, wy})) fill = {190, 50, 50, 235};
            DrawRectangle(sx + 1, sy + 1, tp - 2, tp - 2, fill);

            // Conforming outline: draw edges where neighbor is unoccupied.
            Color edge = {220, 220, 220, 220};
            int lw = 2;
            if (!occupied.count({wx - 1, wy})) DrawRectangle(sx,        sy,  lw, tp, edge);  // left
            if (!occupied.count({wx + 1, wy})) DrawRectangle(sx + tp - lw, sy, lw, tp, edge); // right
            if (!occupied.count({wx, wy - 1})) DrawRectangle(sx, sy,        tp, lw, edge);    // top
            if (!occupied.count({wx, wy + 1})) DrawRectangle(sx, sy + tp - lw, tp, lw, edge); // bottom
        }
    }

    // Centered name label.
    if (!schem.name.empty()) {
        float bbox_sx = std::roundf((min_wx - scroll_x) * tile_px_);
        float bbox_sy = std::roundf((min_wy - scroll_y) * tile_px_);
        float bbox_w  = (max_wx - min_wx + 1) * tile_px_;
        float bbox_h  = (max_wy - min_wy + 1) * tile_px_;
        int font_size = std::max(10, static_cast<int>(tile_px_) / 3);
        int text_w    = MeasureText(schem.name.c_str(), font_size);
        int tx = static_cast<int>(bbox_sx + (bbox_w - text_w) / 2.0f);
        int ty = static_cast<int>(bbox_sy + (bbox_h - font_size) / 2.0f);
        DrawText(schem.name.c_str(), tx, ty, font_size, WHITE);
    }
}

void Renderer::draw_rule_panel(core::RuleSet const& rs, int px, int py) const {
    int font_size = 13;
    int line_h    = font_size + 3;
    int y         = py;
    int max_y     = GetScreenHeight() - 20;

    DrawText("ACTIVE RULES", px, y, font_size, RAYWHITE);
    y += line_h + 4;

    auto draw_line = [&](std::string const& line) -> bool {
        if (y > max_y) return false;
        DrawText(line.c_str(), px, y, font_size, RAYWHITE);
        y += line_h;
        return true;
    };

    bool any = false;
    for (auto const& r : rs.property_rules()) {
        any = true;
        if (!draw_line(std::string(core::kind_name(r.subject)) + " IS "
                       + std::string(core::kind_name(r.property)))) return;
    }
    for (auto const& tr : rs.transform_rules()) {
        any = true;
        if (!draw_line(std::string(core::kind_name(tr.from)) + " IS "
                       + std::string(core::kind_name(tr.to)))) return;
    }
    auto join_nouns = [](std::vector<core::Kind> const& nouns) -> std::string {
        std::string s;
        for (auto const& n : nouns) {
            if (!s.empty()) s += " AND ";
            s += std::string(core::kind_name(n));
        }
        return s;
    };
    auto join_cond = [&](std::vector<core::Kind> const& on_ns,
                         std::vector<core::Kind> const& not_ns) -> std::string {
        std::string s;
        if (!on_ns.empty()) s += " ON " + join_nouns(on_ns);
        for (auto k : not_ns) s += " NOT ON " + std::string(core::kind_name(k));
        return s;
    };
    for (auto const& cr : rs.conditional_rules()) {
        any = true;
        if (!draw_line(std::string(core::kind_name(cr.subject))
                       + join_cond(cr.condition_nouns, cr.forbidden_nouns) + " IS "
                       + std::string(core::kind_name(cr.property)))) return;
    }
    for (auto const& ct : rs.conditional_transform_rules()) {
        any = true;
        if (!draw_line(std::string(core::kind_name(ct.subject))
                       + join_cond(ct.condition_nouns, ct.forbidden_nouns) + " IS "
                       + std::string(core::kind_name(ct.target)))) return;
    }
    for (auto const& cm : rs.conditional_make_rules()) {
        any = true;
        if (!draw_line(std::string(core::kind_name(cm.subject))
                       + join_cond(cm.condition_nouns, cm.forbidden_nouns) + " MAKE "
                       + std::string(core::kind_name(cm.target)))) return;
    }
    for (auto const& mr : rs.make_rules()) {
        any = true;
        if (!draw_line(std::string(core::kind_name(mr.from)) + " MAKE "
                       + std::string(core::kind_name(mr.to)))) return;
    }
    for (auto const& er : rs.eat_rules()) {
        any = true;
        if (!draw_line(std::string(core::kind_name(er.subject)) + " EAT "
                       + std::string(core::kind_name(er.target)))) return;
    }
    if (!any) DrawText("(none)", px, y, font_size, GRAY);
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
                         int tick, bool won, std::string const& extra_info) const {
    int h = GetScreenHeight();
    int w = GetScreenWidth();

    // Bottom bar background.
    DrawRectangle(0, h - 24, w, 24, {20, 20, 20, 220});

    Color mode_col = (mode_label == "PLAY") ? GREEN : SKYBLUE;
    DrawText(mode_label.c_str(), 8, h - 20, 14, mode_col);

    std::string info = filename.empty() ? "(untitled)" : filename;
    info += "  tick:" + std::to_string(tick);
    if (!extra_info.empty()) info += "  " + extra_info;
    DrawText(info.c_str(), 80, h - 20, 12, LIGHTGRAY);

    if (won) {
        DrawText("YOU WIN!", w / 2 - 60, h / 2 - 20, 40, YELLOW);
    }

    // Mode hint.
    const char* hint = (mode_label == "PLAY")
        ? "P:auto  +/-:speed  0:max  arrows:move  Z:undo  ESC:edit"
        : "LMB:place  Sh+drag:select  RMB:del  Ctrl+R:rotate  Ctrl+B:abstract  Enter:play  Ctrl+S/O/I";
    DrawText(hint, w - MeasureText(hint, 11) - 8, h - 20, 11, DARKGRAY);
}

void Renderer::draw_dialog(std::string const& prompt, std::string const& text,
                            std::string const& error_msg) const {
    int w = GetScreenWidth();
    int h = GetScreenHeight();

    // Dimmed overlay.
    DrawRectangle(0, 0, w, h, {0, 0, 0, 160});

    // Dialog box.
    int bw = 500, bh = 90;
    int bx = (w - bw) / 2, by = (h - bh) / 2;
    DrawRectangle(bx, by, bw, bh, {30, 30, 30, 240});
    DrawRectangleLinesEx({static_cast<float>(bx), static_cast<float>(by),
                          static_cast<float>(bw), static_cast<float>(bh)}, 2, LIGHTGRAY);

    DrawText(prompt.c_str(), bx + 12, by + 10, 14, LIGHTGRAY);

    // Input field.
    bool show_cursor = (static_cast<int>(GetTime() * 2) % 2 == 0);
    std::string display = text + (show_cursor ? "|" : " ");
    DrawRectangle(bx + 10, by + 32, bw - 20, 24, {50, 50, 50, 255});
    DrawRectangleLinesEx({static_cast<float>(bx + 10), static_cast<float>(by + 32),
                          static_cast<float>(bw - 20), 24.0f}, 1, YELLOW);
    DrawText(display.c_str(), bx + 14, by + 36, 13, WHITE);

    if (!error_msg.empty())
        DrawText(error_msg.c_str(), bx + 12, by + 64, 12, RED);
    else
        DrawText("Enter: confirm   ESC: cancel", bx + 12, by + 64, 12, DARKGRAY);
}

}  // namespace baba::render
