# src/render/ — raylib drawing layer

Pure consumer of `const core::World&`. Never mutates anything.

## Files

| File | Purpose |
|------|---------|
| `renderer.hpp` / `renderer.cpp` | `Renderer` — `draw_world`, `draw_grid`, `draw_palette`, `draw_rule_panel`, `draw_hud`. |
| `tile_colors.hpp` | `style_for(Kind, bool text)` → `{bg, text_fg, label}`. The palette of swatches used for v1 (no sprites yet). |
| `palette_layout.hpp` | Constants: `PALETTE_W`, `PALETTE_PX`, `PALETTE_ENTRY_H`, `PALETTE_SEARCH_H`, `PALETTE_ENTRY_TOP`. Shared between renderer and `app/input_handler` so click-hit math agrees with draw layout. |

## The shake bug (BUG-2)

`draw_world` and `draw_grid` take `int scroll_x, scroll_y`. The input
handler's cursor-anchored zoom math wants sub-tile precision (it
converts mouse-pixel → world-tile → mouse-pixel after the zoom).
Truncating to int loses up to one tile per zoom step at low `tile_px`
→ the grid jitters.

`renderer.cpp:103` also has a dead grid-offset:
`-(scroll_x % 1) * tile_px_` is always `0` for an `int`.

Planned fix:
- Convert `scroll_x` / `scroll_y` to `float` in `Renderer` and in
  `InputState` (and use them as float in all draw math).
- Compute the grid offset as `(scroll_x - std::floor(scroll_x)) * tile_px_`.

## Layer rules

- Depends on `core/`, `sim/`, `editor/` (for `RuleSet`, `World`,
  palette types) and on raylib.
- No simulation logic; never calls `apply_tick`.
- Sprite atlas is deferred — current visuals are coloured rectangles
  with the kind's name as a label, plus a dark border for text tiles.
