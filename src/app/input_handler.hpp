#pragma once

#include "core/direction.hpp"
#include "editor/editor.hpp"
#include "render/palette_layout.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <raylib.h>
#include <string>
#include <vector>

namespace baba::app {

using render::PALETTE_W;
using render::PALETTE_ENTRY_H;
using render::PALETTE_SEARCH_H;
using render::PALETTE_ENTRY_TOP;

// Translates raw raylib input into editor actions each frame.
// Call poll() once per frame; it mutates the Editor and scroll state in place.
struct InputState {
    float scroll_x{0.0f};
    float scroll_y{0.0f};
    int   tile_px{48};
    int  palette_scroll{0};       // pixel offset into the visible entry list
    bool request_save{false};
    bool request_load{false};
    bool request_new{false};

    std::string palette_search{};
    bool        palette_search_active{false};
    // Filtered palette indices — recomputed by gui_loop each frame before poll_input.
    std::vector<int> palette_filtered{};

    // Track the last tile the mouse placed/deleted on so we only act once per tile
    // while the button is held (drag-painting without per-frame spam).
    core::Coord last_place_tile{INT_MIN, INT_MIN};
    core::Coord last_delete_tile{INT_MIN, INT_MIN};
};

inline void poll_input(editor::Editor& ed, InputState& st) {
    // ── Zoom (mouse wheel over viewport) or palette scroll (over palette) ──
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        if (GetMouseX() < PALETTE_W) {
            st.palette_scroll = std::max(0, st.palette_scroll - static_cast<int>(wheel * PALETTE_ENTRY_H));
        } else {
            int old_px = st.tile_px;
            int new_px = std::clamp(old_px + static_cast<int>(wheel * 4), 8, 128);
            if (new_px != old_px) {
                // Viewport starts at PALETTE_W; compute mouse position within it.
                float mx = static_cast<float>(GetMouseX() - PALETTE_W);
                float my = static_cast<float>(GetMouseY());
                // World tile coordinate under the cursor before zoom.
                float wx = mx / static_cast<float>(old_px) + st.scroll_x;
                float wy = my / static_cast<float>(old_px) + st.scroll_y;
                st.tile_px  = new_px;
                // Adjust scroll so the same world point stays under the cursor (no truncation).
                st.scroll_x = wx - mx / static_cast<float>(new_px);
                st.scroll_y = wy - my / static_cast<float>(new_px);
            }
        }
    }

    // ── Scroll (arrow keys — only when search is not focused) ────────────
    if (!st.palette_search_active) {
        if (IsKeyPressed(KEY_LEFT)  && ed.mode() == editor::EditorMode::Edit) st.scroll_x -= 1.0f;
        if (IsKeyPressed(KEY_RIGHT) && ed.mode() == editor::EditorMode::Edit) st.scroll_x += 1.0f;
        if (IsKeyPressed(KEY_UP)    && ed.mode() == editor::EditorMode::Edit) st.scroll_y -= 1.0f;
        if (IsKeyPressed(KEY_DOWN)  && ed.mode() == editor::EditorMode::Edit) st.scroll_y += 1.0f;
    }

    // ── Middle-drag pan ───────────────────────────────────────────────────
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 delta = GetMouseDelta();
        st.scroll_x -= delta.x / static_cast<float>(st.tile_px);
        st.scroll_y -= delta.y / static_cast<float>(st.tile_px);
    }

    // ── Palette panel clicks (both modes) ────────────────────────────────
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mp = GetMousePosition();
        int mx = static_cast<int>(mp.x);
        int my = static_cast<int>(mp.y);

        if (mx < PALETTE_W) {
            constexpr int search_top = 20;
            if (my >= search_top && my < search_top + PALETTE_SEARCH_H) {
                st.palette_search_active = true;
            } else if (my >= PALETTE_ENTRY_TOP) {
                st.palette_search_active = false;
                int clicked = (my - PALETTE_ENTRY_TOP + st.palette_scroll) / PALETTE_ENTRY_H;
                if (clicked >= 0 && clicked < static_cast<int>(st.palette_filtered.size())) {
                    ed.palette_select(st.palette_filtered[clicked]);
                }
            }
        } else {
            // Clicked outside palette — deactivate search.
            st.palette_search_active = false;
        }
    }

    // ── Search box text input ─────────────────────────────────────────────
    if (st.palette_search_active) {
        int ch;
        while ((ch = GetCharPressed()) != 0) {
            if (ch >= 32 && ch < 127) {
                std::string prev = st.palette_search;
                st.palette_search += static_cast<char>(ch);
                st.palette_scroll = 0;  // reset scroll on query change
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !st.palette_search.empty()) {
            st.palette_search.pop_back();
            st.palette_scroll = 0;
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            st.palette_search_active = false;
        }
        // Don't process any other keyboard controls while typing in the search box.
        return;
    }

    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    if (ed.mode() == editor::EditorMode::Edit) {
        // ── Palette ───────────────────────────────────────────────────────
        if (IsKeyPressed(KEY_E)) ed.palette_next();
        if (IsKeyPressed(KEY_Q)) ed.palette_prev();
        if (IsKeyPressed(KEY_TAB)) ed.toggle_text_variant();
        if (IsKeyPressed(KEY_R)) ed.rotate_facing();

        // ── Edit undo ────────────────────────────────────────────────────
        if (ctrl && IsKeyPressed(KEY_Z)) ed.undo_edit();

        // ── File operations ──────────────────────────────────────────────
        if (ctrl && IsKeyPressed(KEY_S)) st.request_save = true;
        if (ctrl && IsKeyPressed(KEY_O)) st.request_load = true;
        if (ctrl && IsKeyPressed(KEY_N)) st.request_new  = true;

        // ── Enter play mode ──────────────────────────────────────────────
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) ed.enter_play();

        // ── Mouse place / delete ─────────────────────────────────────────
        // Use tile-tracking: only act when the cursor enters a new tile,
        // so holding the button down paints one object per tile (no spam).
        // Skip when mouse is over the palette panel.
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
            GetMouseX() >= PALETTE_W) {
            Vector2 mp = GetMousePosition();
            core::Coord tile{
                static_cast<int>(std::floor(mp.x / st.tile_px + st.scroll_x)),
                static_cast<int>(std::floor(mp.y / st.tile_px + st.scroll_y))
            };
            if (tile.x != st.last_place_tile.x || tile.y != st.last_place_tile.y) {
                auto const& sel = ed.selected();
                ed.place_object(tile, sel.kind, sel.is_text, sel.default_facing);
                st.last_place_tile = tile;
            }
        } else {
            st.last_place_tile = {INT_MIN, INT_MIN};  // reset on button release
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) &&
            GetMouseX() >= PALETTE_W) {
            Vector2 mp = GetMousePosition();
            core::Coord tile{
                static_cast<int>(std::floor(mp.x / st.tile_px + st.scroll_x)),
                static_cast<int>(std::floor(mp.y / st.tile_px + st.scroll_y))
            };
            if (tile.x != st.last_delete_tile.x || tile.y != st.last_delete_tile.y) {
                ed.delete_all_at(tile);
                st.last_delete_tile = tile;
            }
        } else {
            st.last_delete_tile = {INT_MIN, INT_MIN};
        }

    } else {  // Play mode
        // ── Movement ─────────────────────────────────────────────────────
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
            ed.play_step(core::Input::move(core::Direction::Right));
        if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A))
            ed.play_step(core::Input::move(core::Direction::Left));
        if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W))
            ed.play_step(core::Input::move(core::Direction::Up));
        if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S))
            ed.play_step(core::Input::move(core::Direction::Down));
        if (IsKeyPressed(KEY_SPACE))
            ed.play_step(core::Input::wait());

        // ── Play undo ────────────────────────────────────────────────────
        if (IsKeyPressed(KEY_Z) || (ctrl && IsKeyPressed(KEY_Z)))
            ed.play_undo();

        // ── Pan in play mode ─────────────────────────────────────────────
        if (IsKeyPressed(KEY_KP_4)) st.scroll_x -= 1.0f;
        if (IsKeyPressed(KEY_KP_6)) st.scroll_x += 1.0f;
        if (IsKeyPressed(KEY_KP_8)) st.scroll_y -= 1.0f;
        if (IsKeyPressed(KEY_KP_2)) st.scroll_y += 1.0f;

        // ── Return to edit ───────────────────────────────────────────────
        if (IsKeyPressed(KEY_ESCAPE)) ed.enter_edit();
    }
}

}  // namespace baba::app
