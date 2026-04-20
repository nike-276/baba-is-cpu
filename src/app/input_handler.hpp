#pragma once

#include "core/direction.hpp"
#include "core/schematic.hpp"
#include "editor/editor.hpp"
#include "render/palette_layout.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <raylib.h>
#include <string>
#include <vector>

namespace baba::app {

using render::PALETTE_W;
using render::PALETTE_ENTRY_H;
using render::PALETTE_SEARCH_H;
using render::PALETTE_ENTRY_TOP;

enum class PasteState { None, Clipboard, Schematic };

// Translates raw raylib input into editor actions each frame.
// Call poll_input() once per frame; it mutates the Editor and scroll state in place.
struct InputState {
    float scroll_x{0.0f};
    float scroll_y{0.0f};
    float tile_px{48.0f};
    int   palette_scroll{0};
    bool  request_save{false};
    bool  request_save_as{false};
    bool  request_save_schem{false};  // Ctrl+Shift+E with selection → save as .schem
    bool  request_load{false};
    bool  request_new{false};
    bool  request_import_schem{false};

    std::string palette_search{};
    bool        palette_search_active{false};
    std::vector<int> palette_filtered{};

    core::Coord last_place_tile{INT_MIN, INT_MIN};
    core::Coord last_delete_tile{INT_MIN, INT_MIN};

    // Box selection.
    bool        shift_selecting{false};
    bool        has_selection{false};
    core::Coord select_start{};
    core::Coord select_end{};

    // Paste state (clipboard or schematic).
    PasteState paste_state{PasteState::None};

    // Schematic import pending (set by gui_loop after loading the file).
    std::optional<core::Schematic> pending_schem{};
    int  schem_rotation{0};       // 0–3 (steps CW) — for the in-flight paste preview

    // Global toggle: render all placed schematics in abstract mode.
    bool schem_abstract_view{false};

    // Auto-tick requests (handled by gui_loop).
    bool request_auto_tick_toggle{false};
    bool request_auto_tick_faster{false};
    bool request_auto_tick_slower{false};
    bool request_auto_tick_max{false};

    // Benchmark overlay requests (play mode only).
    bool request_bench_toggle{false};  // F3 — show/hide overlay
    bool request_bench_reset{false};   // R  — reset accumulated stats

    // Sound events accumulated from play_step calls this frame.
    std::vector<core::SoundEvent> pending_sounds;
};

inline void poll_input(editor::Editor& ed, InputState& st) {
    // ── Zoom (mouse wheel over viewport) or palette scroll (over palette) ──
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        if (GetMouseX() < PALETTE_W) {
            st.palette_scroll = std::max(0, st.palette_scroll - static_cast<int>(wheel * PALETTE_ENTRY_H));
        } else {
            float old_px = st.tile_px;
            float new_px = std::clamp(old_px * std::pow(1.1f, wheel), 8.0f, 128.0f);
            if (new_px != old_px) {
                float mx = static_cast<float>(GetMouseX() - PALETTE_W);
                float my = static_cast<float>(GetMouseY());
                float wx = mx / old_px + st.scroll_x;
                float wy = my / old_px + st.scroll_y;
                st.tile_px  = new_px;
                st.scroll_x = wx - mx / new_px;
                st.scroll_y = wy - my / new_px;
            }
        }
    }

    // ── Scroll (arrow keys — only when search not focused and in edit mode) ──
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
            st.palette_search_active = false;
        }
    }

    // ── Search box text input ─────────────────────────────────────────────
    if (st.palette_search_active) {
        int ch;
        while ((ch = GetCharPressed()) != 0) {
            // Digits 1–9: select the Nth filtered result and close search.
            if (ch >= '1' && ch <= '9' && !st.palette_filtered.empty()) {
                int n = ch - '1';  // 0-based
                if (n < static_cast<int>(st.palette_filtered.size())) {
                    ed.palette_select(st.palette_filtered[n]);
                    st.palette_search_active = false;
                }
                continue;
            }
            if (ch >= 32 && ch < 127)
                st.palette_search += static_cast<char>(ch);
            st.palette_scroll = 0;
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !st.palette_search.empty()) {
            st.palette_search.pop_back();
            st.palette_scroll = 0;
        }
        if (IsKeyPressed(KEY_ESCAPE)) st.palette_search_active = false;
        return;
    }

    bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT)   || IsKeyDown(KEY_RIGHT_SHIFT);

    if (ed.mode() == editor::EditorMode::Edit) {

        // ── Ctrl+letter palette hotkeys ───────────────────────────────────────
        if (ctrl) {
            // Ctrl+R: rotate palette facing, or rotate in-flight schematic.
            if (IsKeyPressed(KEY_R)) {
                if (st.paste_state == PasteState::Schematic && st.pending_schem)
                    st.schem_rotation = (st.schem_rotation + 1) % 4;
                else
                    ed.rotate_facing();
            }
            if (IsKeyPressed(KEY_E))   ed.palette_next();
            if (IsKeyPressed(KEY_Q))   ed.palette_prev();
            if (IsKeyPressed(KEY_TAB)) ed.toggle_text_variant();
        }

        // ── Type-to-search: any printable char without ctrl ───────────────────
        if (!ctrl) {
            int ch = GetCharPressed();
            if (ch >= 32 && ch < 127) {
                st.palette_search.clear();
                st.palette_search += static_cast<char>(ch);
                st.palette_search_active = true;
                st.palette_scroll = 0;
                return;
            }
        }

        // ── Edit undo ────────────────────────────────────────────────────
        if (ctrl && IsKeyPressed(KEY_Z)) ed.undo_edit();

        // ── File operations ──────────────────────────────────────────────
        if (ctrl && IsKeyPressed(KEY_S)) {
            if (shift || ed.current_path().empty()) st.request_save_as = true;
            else                                    st.request_save    = true;
        }
        if (ctrl && IsKeyPressed(KEY_O)) st.request_load = true;
        if (ctrl && IsKeyPressed(KEY_N)) st.request_new  = true;
        if (ctrl && IsKeyPressed(KEY_I)) st.request_import_schem = true;

        // ── Clipboard ────────────────────────────────────────────────────
        if (ctrl && IsKeyPressed(KEY_C) && st.has_selection) {
            ed.copy_rect(st.select_start, st.select_end);
        }
        if (ctrl && IsKeyPressed(KEY_X) && st.has_selection) {
            ed.cut_rect(st.select_start, st.select_end);
            st.has_selection = false;
        }
        if (ctrl && IsKeyPressed(KEY_V) && ed.has_clipboard() && st.paste_state == PasteState::None) {
            st.paste_state   = PasteState::Clipboard;
            st.has_selection = false;
        }
        // Ctrl+B: toggle abstract schematic view (works in normal mode and during schem paste).
        if (ctrl && IsKeyPressed(KEY_B))
            st.schem_abstract_view = !st.schem_abstract_view;
        // Save selection as schematic.
        if (ctrl && shift && IsKeyPressed(KEY_E) && st.has_selection)
            st.request_save_schem = true;

        // ── Paste mode: cancel ────────────────────────────────────────────
        if (st.paste_state != PasteState::None) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                st.paste_state = PasteState::None;
                return;
            }
        }

        // ── Enter play mode ──────────────────────────────────────────────
        if (IsKeyPressed(KEY_ENTER) || (IsKeyPressed(KEY_SPACE) && st.paste_state == PasteState::None))
            ed.enter_play();

        // ── Mouse: shift+drag = select; plain drag = place ────────────────
        bool mouse_in_viewport = GetMouseX() >= PALETTE_W;

        if (mouse_in_viewport) {
            Vector2 mp = GetMousePosition();
            core::Coord tile{
                static_cast<int>(std::floor(mp.x / st.tile_px + st.scroll_x)),
                static_cast<int>(std::floor(mp.y / st.tile_px + st.scroll_y))
            };

            // Paste on left-click when paste mode is active.
            if (st.paste_state != PasteState::None && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (st.paste_state == PasteState::Clipboard) {
                    ed.paste_at(tile);
                } else if (st.paste_state == PasteState::Schematic && st.pending_schem) {
                    ed.paste_schematic(*st.pending_schem, tile, st.schem_rotation);
                }
                st.paste_state = PasteState::None;
                return;
            }

            if (st.paste_state == PasteState::None) {
                if (shift && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    st.shift_selecting = true;
                    st.select_start    = tile;
                    st.select_end      = tile;
                } else if (shift && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && st.shift_selecting) {
                    st.select_end = tile;
                } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && st.shift_selecting) {
                    st.select_end      = tile;
                    st.has_selection   = true;
                    st.shift_selecting = false;
                } else if (!shift) {
                    // Normal placement (cancel selection on plain LMB).
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) st.has_selection = false;

                    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                        if (tile.x != st.last_place_tile.x || tile.y != st.last_place_tile.y) {
                            auto const& sel = ed.selected();
                            ed.place_object(tile, sel.kind, sel.is_text, sel.default_facing);
                            st.last_place_tile = tile;
                        }
                    } else {
                        st.last_place_tile = {INT_MIN, INT_MIN};
                    }

                    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                        if (tile.x != st.last_delete_tile.x || tile.y != st.last_delete_tile.y) {
                            ed.delete_all_at(tile);
                            st.last_delete_tile = tile;
                        }
                    } else {
                        st.last_delete_tile = {INT_MIN, INT_MIN};
                    }
                }
            }
        } else {
            st.last_place_tile  = {INT_MIN, INT_MIN};
            st.last_delete_tile = {INT_MIN, INT_MIN};
        }

    } else {  // Play mode

        // ── Movement ─────────────────────────────────────────────────────
        auto accumulate_sounds = [&](core::TickReport rep) {
            for (auto& e : rep.sound_events) st.pending_sounds.push_back(std::move(e));
        };
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
            accumulate_sounds(ed.play_step(core::Input::move(core::Direction::Right)));
        if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A))
            accumulate_sounds(ed.play_step(core::Input::move(core::Direction::Left)));
        if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W))
            accumulate_sounds(ed.play_step(core::Input::move(core::Direction::Up)));
        if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S))
            accumulate_sounds(ed.play_step(core::Input::move(core::Direction::Down)));
        if (IsKeyPressed(KEY_SPACE))
            accumulate_sounds(ed.play_step(core::Input::wait()));

        // ── Auto-tick controls ───────────────────────────────────────────
        if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_F5))
            st.request_auto_tick_toggle = true;
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
            st.request_auto_tick_faster = true;
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
            st.request_auto_tick_slower = true;
        if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_KP_0))
            st.request_auto_tick_max = true;

        // ── Benchmark overlay ────────────────────────────────────────────
        if (IsKeyPressed(KEY_F3))  st.request_bench_toggle = true;
        if (IsKeyPressed(KEY_R))   st.request_bench_reset  = true;

        // ── Play undo ────────────────────────────────────────────────────
        bool ctrl2 = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (IsKeyPressed(KEY_Z) || (ctrl2 && IsKeyPressed(KEY_Z)))
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
