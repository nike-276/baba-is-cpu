#include "gui_loop.hpp"
#include "input_handler.hpp"

#include "editor/editor.hpp"
#include "render/palette_layout.hpp"
#include "render/renderer.hpp"
#include "render/sprite_atlas.hpp"
#include "sim/simulator.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace baba::app {

// ── Overlay (modal dialog) state ──────────────────────────────────────────
enum class Overlay {
    None,
    OpenFile,           // Ctrl+O
    SaveAs,             // Ctrl+Shift+S or Ctrl+S with no path
    ImportSchematic,    // Ctrl+I — fuzzy picker of schematics/ folder
    SaveSchematic,      // Ctrl+Shift+E with selection — enter schematic name
    TagSchematic,       // after SaveSchematic — click tiles to tag I/O
    SchematicPicker,    // fuzzy list of all .schem files in schematics/
};

// ── Auto-tick state ───────────────────────────────────────────────────────
struct AutoTick {
    bool  active{false};
    bool  max_speed{false};
    float interval_ms{250.0f};
    float accum_ms{0.0f};

    void toggle()  { active = !active; accum_ms = 0.0f; }
    void faster()  { interval_ms = std::max(50.0f,   interval_ms - 50.0f); }
    void slower()  { interval_ms = std::min(5000.0f, interval_ms + 50.0f); }
    void toggle_max() { max_speed = !max_speed; }

    std::string label() const {
        if (!active)      return "";
        if (max_speed)    return "AUTO:MAX";
        return "AUTO:" + std::to_string(static_cast<int>(interval_ms)) + "ms";
    }
};

// ── Modal dialog input helper ─────────────────────────────────────────────
// Process keyboard input for the text dialog; returns true when Enter is pressed.
// Returns false normally; call per-frame while dialog is active.
static bool handle_dialog_input(std::string& text, bool& cancelled) {
    int ch;
    while ((ch = GetCharPressed()) != 0)
        if (ch >= 32 && ch < 127) text += static_cast<char>(ch);
    if (IsKeyPressed(KEY_BACKSPACE) && !text.empty()) text.pop_back();
    if (IsKeyPressed(KEY_ESCAPE)) { cancelled = true; return false; }
    if (IsKeyPressed(KEY_ENTER))  return true;
    return false;
}

// ── Audio helpers (PLAY keyword) ──────────────────────────────────────────
namespace {

float note_freq(core::Kind note, int octave, bool sharp, bool flat) {
    // Chromatic position from C: C=0, D=2, E=4, F=5, G=7, A=9, B=11
    static constexpr int chroma[] = {9, 11, 0, 2, 4, 5, 7};  // A B C D E F G
    int idx = static_cast<int>(note) - static_cast<int>(core::Kind::O_LetterA);
    if (idx < 0 || idx > 6) idx = 0;
    int midi = (octave + 1) * 12 + chroma[idx];
    if (sharp) ++midi;
    if (flat)  --midi;
    return 440.0f * std::pow(2.0f, (midi - 69) / 12.0f);
}

Sound& note_sound(core::Kind note, int octave, bool sharp, bool flat) {
    using Key = std::tuple<int, int, bool, bool>;
    static std::map<Key, Sound> cache;
    Key k{static_cast<int>(note), octave, sharp, flat};
    auto it = cache.find(k);
    if (it != cache.end()) return it->second;

    constexpr int   SR  = 44100;
    constexpr float DUR = 0.3f;
    const int N    = static_cast<int>(SR * DUR);
    const int FADE = static_cast<int>(SR * 0.01f);  // 10 ms fade-out

    float freq = note_freq(note, octave, sharp, flat);
    std::vector<int16_t> buf(N);
    constexpr float TWO_PI = 6.28318530717958647692f;
    for (int i = 0; i < N; ++i) {
        float t   = static_cast<float>(i) / SR;
        float env = (i >= N - FADE) ? static_cast<float>(N - 1 - i) / FADE : 1.0f;
        buf[i] = static_cast<int16_t>(env * 16000.0f * std::sin(TWO_PI * freq * t));
    }

    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(N);
    wave.sampleRate = SR;
    wave.sampleSize = 16;
    wave.channels   = 1;
    wave.data       = buf.data();  // LoadSoundFromWave copies the data

    cache[k] = LoadSoundFromWave(wave);
    return cache[k];
}

void play_sound_events(std::vector<core::SoundEvent> const& events) {
    for (auto const& ev : events)
        PlaySound(note_sound(ev.note, ev.octave, ev.sharp, ev.flat));
}

}  // namespace

// ── Main loop ─────────────────────────────────────────────────────────────
int run_gui(std::string const& level_path, std::size_t undo_cap) {
    const int WIN_W = 1280;
    const int WIN_H = 720;
    InitWindow(WIN_W, WIN_H, "baba-is-true editor");
    InitAudioDevice();
    SetTargetFPS(60);
    SetExitKey(0);

    editor::Editor ed(sim::Simulator(core::World{}, undo_cap), undo_cap);
    if (!level_path.empty()) {
        if (!ed.load_level(level_path))
            std::fprintf(stderr, "babaiwt: failed to load '%s'\n", level_path.c_str());
    }

    render::Renderer renderer(48.0f);
    render::SpriteAtlas sprite_atlas;
    sprite_atlas.load("assets/sprites");
    if (!sprite_atlas.empty()) renderer.set_atlas(&sprite_atlas);
    InputState state;
    state.tile_px = renderer.tile_px();

    std::string status_msg;
    int status_frames = 0;

    Overlay     overlay{Overlay::None};
    std::string overlay_text;
    std::string overlay_error;
    std::string overlay_prompt;

    // For schematic saving + tagging.
    core::Coord  schem_sel_a{}, schem_sel_b{};
    std::vector<core::Coord> schem_input_tags, schem_output_tags;
    int tag_guard_frames{0};  // skip Enter for N frames after entering TagSchematic

    // For schematic picker (Ctrl+I).
    std::vector<std::string> picker_files;     // all .schem paths in schematics/
    std::vector<std::string> picker_filtered;  // subset matching picker_query
    std::string              picker_query;
    int                      picker_sel{0};

    AutoTick auto_tick;
    bool     show_bench{false};

    // ── Main loop ─────────────────────────────────────────────────────────
    while (!WindowShouldClose()) {
        state.request_save             = false;
        state.request_save_as          = false;
        state.request_save_schem       = false;
        state.request_load             = false;
        state.request_new              = false;
        state.request_import_schem     = false;
        state.request_auto_tick_toggle = false;
        state.request_auto_tick_faster = false;
        state.request_auto_tick_slower = false;
        state.request_auto_tick_max    = false;
        state.request_bench_toggle     = false;
        state.request_bench_reset      = false;
        state.pending_sounds.clear();
        renderer.set_tile_px(state.tile_px);

        // ── Filtered palette ───────────────────────────────────────────────
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

        // ── Handle modal overlays (skip normal poll_input when active) ─────
        bool overlay_active = (overlay != Overlay::None);
        if (!overlay_active) {
            poll_input(ed, state);
            play_sound_events(state.pending_sounds);
        } else {
            // Allow scroll/zoom while overlay is up.
            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f && GetMouseX() >= render::PALETTE_W) {
                float old_px = state.tile_px;
                float new_px = std::clamp(old_px * std::pow(1.1f, wheel), 8.0f, 128.0f);
                if (new_px != old_px) {
                    float mx = static_cast<float>(GetMouseX() - render::PALETTE_W);
                    float my = static_cast<float>(GetMouseY());
                    float wx = mx / old_px + state.scroll_x;
                    float wy = my / old_px + state.scroll_y;
                    state.tile_px  = new_px;
                    state.scroll_x = wx - mx / new_px;
                    state.scroll_y = wy - my / new_px;
                }
            }
        }

        // ── Process InputState requests ───────────────────────────────────
        if (state.request_save) {
            std::string path = ed.current_path();
            std::ofstream out(path);
            if (out) {
                out << ed.serialize();
                ed.mark_clean();
                status_msg    = "Saved: " + path;
                status_frames = 120;
            } else {
                status_msg    = "Save failed!";
                status_frames = 120;
            }
        }

        if (state.request_save_as) {
            overlay        = Overlay::SaveAs;
            overlay_prompt = "Save as (.level):";
            overlay_text   = ed.current_path();
            overlay_error  = "";
        }

        if (state.request_save_schem) {
            overlay         = Overlay::SaveSchematic;
            overlay_prompt  = "Schematic name (saved to schematics/):";
            overlay_text    = "";
            overlay_error   = "";
            schem_sel_a     = state.select_start;
            schem_sel_b     = state.select_end;
            schem_input_tags.clear();
            schem_output_tags.clear();
        }

        if (state.request_load) {
            overlay        = Overlay::OpenFile;
            overlay_prompt = "Open level file:";
            overlay_text   = "";
            overlay_error  = "";
        }

        if (state.request_new) {
            ed            = editor::Editor(sim::Simulator(core::World{}, undo_cap), undo_cap);
            status_msg    = "New level";
            status_frames = 60;
        }

        if (state.request_import_schem) {
            // Scan schematics/ folder and open fuzzy picker.
            picker_files.clear();
            picker_query = "";
            picker_sel   = 0;
            std::error_code ec;
            for (auto const& entry : std::filesystem::directory_iterator("schematics", ec)) {
                if (entry.path().extension() == ".schem")
                    picker_files.push_back(entry.path().string());
            }
            std::sort(picker_files.begin(), picker_files.end());
            overlay        = Overlay::SchematicPicker;
            overlay_error  = "";
        }

        // ── Auto-tick controls ────────────────────────────────────────────
        if (state.request_auto_tick_toggle) auto_tick.toggle();
        if (state.request_auto_tick_faster) auto_tick.faster();
        if (state.request_auto_tick_slower) auto_tick.slower();
        if (state.request_auto_tick_max)    auto_tick.toggle_max();

        // Auto-tick execution (only in play mode).
        if (auto_tick.active && ed.mode() == editor::EditorMode::Play) {
            float dt_ms = GetFrameTime() * 1000.0f;
            if (auto_tick.max_speed) {
                play_sound_events(ed.play_step(core::Input::wait()).sound_events);
            } else {
                auto_tick.accum_ms += dt_ms;
                if (auto_tick.accum_ms >= auto_tick.interval_ms) {
                    auto_tick.accum_ms = std::fmod(auto_tick.accum_ms, auto_tick.interval_ms);
                    play_sound_events(ed.play_step(core::Input::wait()).sound_events);
                }
            }
        }
        if (ed.mode() != editor::EditorMode::Play) {
            auto_tick.active = false;
            show_bench       = false;
        }

        // ── Benchmark overlay toggle / reset ──────────────────────────────
        if (state.request_bench_toggle && ed.mode() == editor::EditorMode::Play) {
            show_bench = !show_bench;
            if (show_bench) ed.reset_bench();
        }
        if (state.request_bench_reset && show_bench) ed.reset_bench();

        // ── Handle overlay dialog input ───────────────────────────────────
        if (overlay_active && overlay != Overlay::TagSchematic && overlay != Overlay::SchematicPicker) {
            bool cancelled = false;
            bool confirmed = handle_dialog_input(overlay_text, cancelled);
            if (cancelled) {
                overlay = Overlay::None;
            } else if (confirmed) {
                switch (overlay) {
                    case Overlay::OpenFile: {
                        if (!ed.load_level(overlay_text)) {
                            overlay_error = "Cannot open: " + overlay_text;
                        } else {
                            overlay = Overlay::None;
                            status_msg    = "Opened: " + overlay_text;
                            status_frames = 120;
                        }
                        break;
                    }
                    case Overlay::SaveAs: {
                        std::ofstream out(overlay_text);
                        if (out) {
                            out << ed.serialize();
                            ed.set_path(overlay_text);
                            ed.mark_clean();
                            overlay       = Overlay::None;
                            status_msg    = "Saved: " + overlay_text;
                            status_frames = 120;
                        } else {
                            overlay_error = "Cannot write: " + overlay_text;
                        }
                        break;
                    }
                    case Overlay::ImportSchematic: {
                        auto result = core::load_schematic_file(overlay_text);
                        if (std::holds_alternative<core::Schematic>(result)) {
                            state.pending_schem       = std::get<core::Schematic>(result);
                            state.paste_state         = PasteState::Schematic;
                            state.schem_rotation      = 0;
                            state.schem_abstract_view = false;
                            overlay = Overlay::None;
                        } else {
                            overlay_error = "Cannot load schematic: " + overlay_text;
                        }
                        break;
                    }
                    case Overlay::SaveSchematic: {
                        // Build full path: schematics/<name>.schem
                        std::error_code ec;
                        std::filesystem::create_directories("schematics", ec);
                        std::string full_path = "schematics/" + overlay_text + ".schem";
                        overlay_text      = full_path;  // store full path for TagSchematic
                        overlay           = Overlay::TagSchematic;
                        overlay_error     = "";
                        tag_guard_frames  = 2;  // prevent same-frame Enter from saving immediately
                        status_msg        = "Tag I/O tiles: LMB=input RMB=output MMB=clear  Enter=save  ESC=cancel";
                        status_frames     = 300;
                        break;
                    }
                    default: break;
                }
            }
        }

        // ── Tagging overlay: click tiles in the selection ─────────────────
        if (overlay == Overlay::TagSchematic) {
            if (tag_guard_frames > 0) --tag_guard_frames;
            if (IsKeyPressed(KEY_ESCAPE)) {
                overlay = Overlay::None;
            } else if (tag_guard_frames == 0 && IsKeyPressed(KEY_ENTER)) {
                std::string path = overlay_text;
                if (ed.save_selection_as_schem(schem_sel_a, schem_sel_b,
                                               schem_input_tags, schem_output_tags, path)) {
                    // Record as a placed schematic so abstract view shows it immediately.
                    auto result = core::load_schematic_file(path);
                    if (std::holds_alternative<core::Schematic>(result)) {
                        core::Coord lo{std::min(schem_sel_a.x, schem_sel_b.x),
                                       std::min(schem_sel_a.y, schem_sel_b.y)};
                        ed.record_schem_placement(std::get<core::Schematic>(result), lo, 0);
                        state.schem_abstract_view = true;  // switch to abstract view
                    }
                    overlay       = Overlay::None;
                    status_msg    = "Schematic saved: " + path;
                    status_frames = 120;
                } else {
                    overlay_error = "Cannot write: " + path;
                    overlay = Overlay::SaveSchematic;  // go back to filename dialog
                }
            } else if (GetMouseX() >= render::PALETTE_W) {
                // Map mouse to tile.
                float mx = static_cast<float>(GetMouseX());
                float my = static_cast<float>(GetMouseY());
                core::Coord tile{
                    static_cast<int>(std::floor(mx / state.tile_px + state.scroll_x)),
                    static_cast<int>(std::floor(my / state.tile_px + state.scroll_y))
                };

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    // Toggle input tag.
                    auto it = std::find(schem_input_tags.begin(), schem_input_tags.end(), tile);
                    if (it != schem_input_tags.end()) {
                        schem_input_tags.erase(it);
                    } else {
                        schem_input_tags.push_back(tile);
                        auto it2 = std::find(schem_output_tags.begin(), schem_output_tags.end(), tile);
                        if (it2 != schem_output_tags.end()) schem_output_tags.erase(it2);
                    }
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    auto it = std::find(schem_output_tags.begin(), schem_output_tags.end(), tile);
                    if (it != schem_output_tags.end()) {
                        schem_output_tags.erase(it);
                    } else {
                        schem_output_tags.push_back(tile);
                        auto it2 = std::find(schem_input_tags.begin(), schem_input_tags.end(), tile);
                        if (it2 != schem_input_tags.end()) schem_input_tags.erase(it2);
                    }
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
                    schem_input_tags.erase(std::remove(schem_input_tags.begin(), schem_input_tags.end(), tile), schem_input_tags.end());
                    schem_output_tags.erase(std::remove(schem_output_tags.begin(), schem_output_tags.end(), tile), schem_output_tags.end());
                }
            }
        }

        // ── Schematic picker: rebuild filtered list and handle input ─────────
        if (overlay == Overlay::SchematicPicker) {
            // Rebuild filtered list.
            std::string q_low = picker_query;
            for (char& c : q_low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            picker_filtered.clear();
            for (auto const& p : picker_files) {
                std::string name_low = p;
                for (char& c : name_low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (q_low.empty() || name_low.find(q_low) != std::string::npos)
                    picker_filtered.push_back(p);
            }
            picker_sel = std::clamp(picker_sel, 0, std::max(0, static_cast<int>(picker_filtered.size()) - 1));

            // Keyboard input.
            int ch;
            while ((ch = GetCharPressed()) != 0)
                if (ch >= 32 && ch < 127) { picker_query += static_cast<char>(ch); picker_sel = 0; }
            if (IsKeyPressed(KEY_BACKSPACE) && !picker_query.empty()) { picker_query.pop_back(); picker_sel = 0; }
            if (IsKeyPressed(KEY_DOWN) && picker_sel < static_cast<int>(picker_filtered.size()) - 1) ++picker_sel;
            if (IsKeyPressed(KEY_UP)   && picker_sel > 0)                                            --picker_sel;
            if (IsKeyPressed(KEY_ESCAPE)) { overlay = Overlay::None; }
            if (IsKeyPressed(KEY_ENTER) && !picker_filtered.empty()) {
                std::string path = picker_filtered[picker_sel];
                auto result = core::load_schematic_file(path);
                if (std::holds_alternative<core::Schematic>(result)) {
                    state.pending_schem       = std::get<core::Schematic>(result);
                    state.paste_state         = PasteState::Schematic;
                    state.schem_rotation      = 0;
                    state.schem_abstract_view = false;
                    overlay = Overlay::None;
                    status_msg    = "Loaded: " + path;
                    status_frames = 120;
                } else {
                    overlay_error = "Cannot load: " + path;
                }
            }
        }

        // ── Draw ──────────────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(BLACK);

        int palette_w = render::PALETTE_W;
        int rules_w   = 220;

        renderer.draw_world(ed.world(), state.scroll_x, state.scroll_y);
        renderer.draw_grid(WIN_W, WIN_H, state.scroll_x, state.scroll_y);

        // Selection rectangle.
        if (state.has_selection || state.shift_selecting) {
            renderer.draw_selection(state.select_start, state.select_end,
                                    state.scroll_x, state.scroll_y);
        }

        // I/O tag overlays during tagging mode.
        if (overlay == Overlay::TagSchematic) {
            renderer.draw_selection(schem_sel_a, schem_sel_b, state.scroll_x, state.scroll_y);
            for (auto const& c : schem_input_tags) {
                int sx = static_cast<int>(std::roundf((c.x - state.scroll_x) * state.tile_px));
                int sy = static_cast<int>(std::roundf((c.y - state.scroll_y) * state.tile_px));
                DrawRectangle(sx+1, sy+1, static_cast<int>(state.tile_px)-2,
                              static_cast<int>(state.tile_px)-2, {60, 100, 220, 160});
            }
            for (auto const& c : schem_output_tags) {
                int sx = static_cast<int>(std::roundf((c.x - state.scroll_x) * state.tile_px));
                int sy = static_cast<int>(std::roundf((c.y - state.scroll_y) * state.tile_px));
                DrawRectangle(sx+1, sy+1, static_cast<int>(state.tile_px)-2,
                              static_cast<int>(state.tile_px)-2, {200, 60, 60, 160});
            }
        }

        // Global abstract view: render all placed schematics abstractly.
        if (state.schem_abstract_view) {
            for (auto const& p : ed.schem_placements()) {
                renderer.draw_schematic_abstract(p.schem, p.target, p.rotation_cw,
                                                 state.scroll_x, state.scroll_y);
            }
        }

        // Paste preview.
        if (state.paste_state == PasteState::Clipboard && ed.has_clipboard()) {
            Vector2 mp = GetMousePosition();
            core::Coord hover{
                static_cast<int>(std::floor(mp.x / state.tile_px + state.scroll_x)),
                static_cast<int>(std::floor(mp.y / state.tile_px + state.scroll_y))
            };
            renderer.draw_world_at(ed.clipboard_world(), hover,
                                   state.scroll_x, state.scroll_y, 128);
        }
        if (state.paste_state == PasteState::Schematic && state.pending_schem) {
            Vector2 mp = GetMousePosition();
            core::Coord hover{
                static_cast<int>(std::floor(mp.x / state.tile_px + state.scroll_x)),
                static_cast<int>(std::floor(mp.y / state.tile_px + state.scroll_y))
            };
            if (state.schem_abstract_view) {
                renderer.draw_schematic_abstract(*state.pending_schem, hover,
                                                 state.schem_rotation,
                                                 state.scroll_x, state.scroll_y);
            } else {
                renderer.draw_schematic_normal(*state.pending_schem, hover,
                                               state.schem_rotation,
                                               state.scroll_x, state.scroll_y);
            }
        }

        // ── Palette panel (left) ──────────────────────────────────────────
        int palette_panel_h = WIN_H - 24 - 20;
        DrawRectangle(0, 0, palette_w - 2, WIN_H - 24, {15, 15, 15, 220});
        {
            auto const& pal = ed.palette();
            int actual_sel  = static_cast<int>(&ed.selected() - pal.data());

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

        // ── Rules panel (right) ───────────────────────────────────────────
        DrawRectangle(WIN_W - rules_w, 0, rules_w, WIN_H - 24, {15, 15, 15, 180});
        renderer.draw_rule_panel(ed.current_rules(), WIN_W - rules_w + 6, 8);

        // ── HUD bar (bottom) ──────────────────────────────────────────────
        std::string mode_str = (ed.mode() == editor::EditorMode::Play) ? "PLAY" : "EDIT";
        std::string fname    = ed.current_path();
        if (ed.dirty()) fname += "*";
        renderer.draw_hud(mode_str, fname, 0, false, auto_tick.label());

        // ── Status message ────────────────────────────────────────────────
        if (status_frames > 0) {
            --status_frames;
            DrawText(status_msg.c_str(), palette_w + 8, WIN_H / 2 - 10, 18, YELLOW);
        }

        // ── Overlay dialogs ───────────────────────────────────────────────
        if (overlay == Overlay::OpenFile || overlay == Overlay::SaveAs ||
            overlay == Overlay::SaveSchematic) {
            std::string err_disp = overlay_error;
            renderer.draw_dialog(overlay_prompt, overlay_text, err_disp);
        }

        // Schematic picker overlay.
        if (overlay == Overlay::SchematicPicker) {
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            DrawRectangle(0, 0, sw, sh, {0, 0, 0, 160});

            int bw = 560, bh = 360;
            int bx = (sw - bw) / 2, by = (sh - bh) / 2;
            DrawRectangle(bx, by, bw, bh, {25, 25, 35, 245});
            DrawRectangleLinesEx({static_cast<float>(bx), static_cast<float>(by),
                                  static_cast<float>(bw), static_cast<float>(bh)}, 2, LIGHTGRAY);

            DrawText("Import Schematic  (type to filter, Up/Down, Enter)", bx + 10, by + 8, 13, LIGHTGRAY);

            // Search box.
            bool show_cursor = (static_cast<int>(GetTime() * 2) % 2 == 0);
            std::string search_display = picker_query + (show_cursor ? "|" : " ");
            DrawRectangle(bx + 8, by + 28, bw - 16, 22, {50, 50, 50, 255});
            DrawRectangleLinesEx({static_cast<float>(bx + 8), static_cast<float>(by + 28),
                                  static_cast<float>(bw - 16), 22.0f}, 1, YELLOW);
            DrawText(search_display.c_str(), bx + 12, by + 32, 13, WHITE);

            // File list.
            int list_top  = by + 56;
            int entry_h   = 20;
            int visible_n = (bh - 70) / entry_h;
            int scroll_start = std::max(0, picker_sel - visible_n / 2);
            BeginScissorMode(bx + 4, list_top, bw - 8, bh - 70);
            for (int i = scroll_start; i < static_cast<int>(picker_filtered.size()) && i < scroll_start + visible_n + 1; ++i) {
                int ey = list_top + (i - scroll_start) * entry_h;
                if (i == picker_sel) {
                    DrawRectangle(bx + 4, ey, bw - 8, entry_h - 2, {60, 100, 200, 200});
                }
                Color tc = (i == picker_sel) ? WHITE : LIGHTGRAY;
                DrawText(picker_filtered[i].c_str(), bx + 10, ey + 3, 12, tc);
            }
            EndScissorMode();
            if (picker_filtered.empty())
                DrawText("(no schematics found)", bx + 10, list_top + 4, 13, DARKGRAY);
            if (!overlay_error.empty())
                DrawText(overlay_error.c_str(), bx + 10, by + bh - 18, 12, RED);
            else
                DrawText("Enter: load   ESC: cancel", bx + 10, by + bh - 18, 12, DARKGRAY);
        }

        // Tagging mode instruction banner.
        if (overlay == Overlay::TagSchematic) {
            DrawRectangle(palette_w, 0, WIN_W - palette_w - rules_w, 28, {40, 20, 80, 220});
            DrawText("Tag I/O: LMB=input(blue)  RMB=output(red)  MMB=clear  Enter=save  ESC=cancel",
                     palette_w + 6, 6, 13, WHITE);
        }

        // ── Benchmark overlay (play mode, F3 to toggle, R to reset) ──────
        if (show_bench && ed.mode() == editor::EditorMode::Play) {
            auto br = ed.bench_report();

            // Build sorted index (descending by total_ns).
            constexpr int N = static_cast<int>(core::Phase::Count);
            int order[N];
            for (int i = 0; i < N; ++i) order[i] = i;
            std::sort(order, order + N, [&](int a, int b) {
                return br.phases[a].total_ns > br.phases[b].total_ns;
            });

            const int FONT  = 12;
            const int ROW_H = 14;
            const int PAD   = 6;
            const int PW    = 270;
            const int PH    = PAD + ROW_H + PAD + N * ROW_H + PAD + ROW_H + ROW_H + PAD;
            const int PX    = WIN_W - rules_w - PW - 4;
            const int PY    = 4;

            DrawRectangle(PX, PY, PW, PH, {10, 10, 10, 210});
            DrawRectangleLinesEx({static_cast<float>(PX), static_cast<float>(PY),
                                  static_cast<float>(PW), static_cast<float>(PH)},
                                 1, {80, 80, 80, 200});

            // Header row.
            char hdr[64];
            std::snprintf(hdr, sizeof(hdr), "BENCH %d ticks  [R=reset]", br.tick_count);
            DrawText(hdr, PX + PAD, PY + PAD, FONT, YELLOW);

            int row_y = PY + PAD + ROW_H + PAD;
            for (int idx : order) {
                auto const& s = br.phases[idx];
                if (s.count == 0) { row_y += ROW_H; continue; }
                double pct = br.total.total_ns > 0
                             ? 100.0 * s.total_ns / br.total.total_ns : 0.0;
                // Color: hot phases (>15%) in orange, warm (>5%) yellow, rest gray.
                Color col = (pct > 15.0) ? Color{255, 140, 0, 255}
                          : (pct > 5.0)  ? YELLOW
                          :                LIGHTGRAY;
                char row[80];
                std::snprintf(row, sizeof(row), "%-22s %5.1f  %3.0f%%",
                              s.name, s.mean_us(), pct);
                DrawText(row, PX + PAD, row_y, FONT, col);
                row_y += ROW_H;
            }
            // Object count row.
            {
                std::size_t obj_count = ed.world().all_ids().size();
                char obj[64];
                std::snprintf(obj, sizeof(obj), "%-22s %5zu", "objects", obj_count);
                DrawText(obj, PX + PAD, row_y, FONT, LIGHTGRAY);
                row_y += ROW_H;
            }
            // Total row.
            DrawLine(PX + PAD, row_y, PX + PW - PAD, row_y, {80, 80, 80, 180});
            row_y += 2;
            char tot[64];
            std::snprintf(tot, sizeof(tot), "%-22s %5.1f  100%%",
                          "TOTAL", br.total.mean_us());
            DrawText(tot, PX + PAD, row_y, FONT, WHITE);
        }

        // Paste mode instruction banner.
        if (state.paste_state == PasteState::Schematic) {
            DrawRectangle(palette_w, 0, WIN_W - palette_w - rules_w, 28, {20, 40, 80, 220});
            DrawText("Schematic: LMB=stamp  Ctrl+R=rotate  Ctrl+B=toggle abstract  ESC=cancel",
                     palette_w + 6, 6, 13, WHITE);
        } else if (state.paste_state == PasteState::Clipboard) {
            DrawRectangle(palette_w, 0, WIN_W - palette_w - rules_w, 28, {20, 60, 40, 220});
            DrawText("Paste: LMB=stamp  ESC=cancel", palette_w + 6, 6, 13, WHITE);
        }

        EndDrawing();
    }

    CloseAudioDevice();
    CloseWindow();
    return 0;
}

}  // namespace baba::app
