#pragma once

#include "core/direction.hpp"
#include "editor/editor.hpp"

#include <raylib.h>

namespace baba::app {

// Translates raw raylib input into editor actions each frame.
// Call poll() once per frame; it mutates the Editor and scroll state in place.
struct InputState {
    int  scroll_x{0};
    int  scroll_y{0};
    int  tile_px{48};
    bool request_save{false};
    bool request_load{false};
    bool request_new{false};
};

inline void poll_input(editor::Editor& ed, InputState& st) {
    // ── Zoom (mouse wheel) ────────────────────────────────────────────────
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        st.tile_px = std::clamp(st.tile_px + static_cast<int>(wheel * 4), 8, 128);
    }

    // ── Scroll (arrow keys in both modes) ────────────────────────────────
    if (IsKeyPressed(KEY_LEFT)  && ed.mode() == editor::EditorMode::Edit) --st.scroll_x;
    if (IsKeyPressed(KEY_RIGHT) && ed.mode() == editor::EditorMode::Edit) ++st.scroll_x;
    if (IsKeyPressed(KEY_UP)    && ed.mode() == editor::EditorMode::Edit) --st.scroll_y;
    if (IsKeyPressed(KEY_DOWN)  && ed.mode() == editor::EditorMode::Edit) ++st.scroll_y;

    // ── Middle-drag pan ───────────────────────────────────────────────────
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 delta = GetMouseDelta();
        if (std::abs(delta.x) > 1) st.scroll_x -= static_cast<int>(delta.x) / st.tile_px;
        if (std::abs(delta.y) > 1) st.scroll_y -= static_cast<int>(delta.y) / st.tile_px;
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
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 mp = GetMousePosition();
            int tx = static_cast<int>(mp.x) / st.tile_px + st.scroll_x;
            int ty = static_cast<int>(mp.y) / st.tile_px + st.scroll_y;
            auto const& sel = ed.selected();
            ed.place_object({tx, ty}, sel.kind, sel.is_text, sel.default_facing);
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            Vector2 mp = GetMousePosition();
            int tx = static_cast<int>(mp.x) / st.tile_px + st.scroll_x;
            int ty = static_cast<int>(mp.y) / st.tile_px + st.scroll_y;
            ed.delete_all_at({tx, ty});
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
        if (IsKeyPressed(KEY_KP_4)) --st.scroll_x;
        if (IsKeyPressed(KEY_KP_6)) ++st.scroll_x;
        if (IsKeyPressed(KEY_KP_8)) --st.scroll_y;
        if (IsKeyPressed(KEY_KP_2)) ++st.scroll_y;

        // ── Return to edit ───────────────────────────────────────────────
        if (IsKeyPressed(KEY_ESCAPE)) ed.enter_edit();
    }
}

}  // namespace baba::app
