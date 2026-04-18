#include "gui_loop.hpp"
#include "input_handler.hpp"

#include "editor/editor.hpp"
#include "render/palette_layout.hpp"
#include "render/renderer.hpp"
#include "sim/simulator.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <string>

namespace baba::app {

int run_gui(std::string const& level_path) {
    // ── Init ─────────────────────────────────────────────────────────────
    const int WIN_W = 1280;
    const int WIN_H = 720;
    InitWindow(WIN_W, WIN_H, "baba-is-true editor");
    SetTargetFPS(60);
    SetExitKey(0);  // disable ESC=close so ESC can return from play→edit

    editor::Editor ed(sim::Simulator(core::World{}));

    if (!level_path.empty()) {
        if (!ed.load_level(level_path)) {
            std::fprintf(stderr, "babaiwt: failed to load '%s'\n", level_path.c_str());
        }
    }

    render::Renderer renderer(48.0f);
    InputState state;
    state.tile_px = renderer.tile_px();

    std::string status_msg;
    int status_frames = 0;

    // ── Main loop ─────────────────────────────────────────────────────────
    while (!WindowShouldClose()) {
        state.request_save = false;
        state.request_load = false;
        state.request_new  = false;
        renderer.set_tile_px(state.tile_px);

        // ── Compute filtered palette indices (case-insensitive substring) ─
        {
            std::string q = state.palette_search;
            for (char& c : q) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            state.palette_filtered.clear();
            auto const& pal = ed.palette();
            for (int i = 0; i < static_cast<int>(pal.size()); ++i) {
                if (q.empty()) {
                    state.palette_filtered.push_back(i);
                } else {
                    std::string name = pal[i].display_name;
                    for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (name.find(q) != std::string::npos)
                        state.palette_filtered.push_back(i);
                }
            }
        }

        poll_input(ed, state);

        // ── File operations ───────────────────────────────────────────────
        if (state.request_save) {
            std::string path = ed.current_path();
            if (path.empty()) path = "untitled.level";
            std::ofstream out(path);
            if (out) {
                out << ed.serialize();
                ed.set_path(path);
                ed.mark_clean();
                status_msg    = "Saved: " + path;
                status_frames = 120;
            } else {
                status_msg    = "Save failed!";
                status_frames = 120;
            }
        }
        if (state.request_new) {
            ed = editor::Editor(sim::Simulator(core::World{}));
            status_msg    = "New level";
            status_frames = 60;
        }

        // ── Draw ──────────────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(BLACK);

        int palette_w = render::PALETTE_W;
        int rules_w   = 220;
        int content_x = palette_w;
        int content_w = WIN_W - palette_w - rules_w;
        (void)content_w;  // content area width for future scissor use

        renderer.draw_world(ed.world(), state.scroll_x, state.scroll_y);
        renderer.draw_grid(WIN_W, WIN_H, state.scroll_x, state.scroll_y);

        // Palette panel (left).
        int palette_panel_h = WIN_H - 24 - 20;  // minus HUD bar and header
        DrawRectangle(0, 0, palette_w - 2, WIN_H - 24, {15, 15, 15, 220});
        {
            auto const& pal = ed.palette();
            int actual_sel  = static_cast<int>(&ed.selected() - pal.data());

            // Build filtered display names; find selected's position in filtered list.
            std::vector<std::string> names;
            int filtered_sel = -1;
            for (int i = 0; i < static_cast<int>(state.palette_filtered.size()); ++i) {
                int idx = state.palette_filtered[i];
                names.push_back(pal[idx].display_name);
                if (idx == actual_sel) filtered_sel = i;
            }

            int max_scroll = renderer.draw_palette(names, filtered_sel, 2, 20,
                                                   state.palette_scroll,
                                                   palette_panel_h,
                                                   state.palette_search,
                                                   state.palette_search_active);
            state.palette_scroll = std::min(state.palette_scroll, max_scroll);
        }

        // Rules panel (right).
        DrawRectangle(WIN_W - rules_w, 0, rules_w, WIN_H - 24, {15, 15, 15, 180});
        renderer.draw_rule_panel(ed.current_rules(), WIN_W - rules_w + 6, 8);

        // HUD bar (bottom).
        std::string mode_str = (ed.mode() == editor::EditorMode::Play) ? "PLAY" : "EDIT";
        std::string fname    = ed.current_path();
        if (ed.dirty()) fname += "*";
        renderer.draw_hud(mode_str, fname,
                          0,  // tick count removed from renderer signature for now
                          false);

        // Status message overlay.
        if (status_frames > 0) {
            --status_frames;
            DrawText(status_msg.c_str(), content_x + 8, WIN_H / 2 - 10, 18, YELLOW);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}

}  // namespace baba::app
