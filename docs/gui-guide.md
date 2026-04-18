# GUI guide — `babaiwt` editor + play loop

## Build

The GUI is a CMake build that pulls raylib 5.0 via FetchContent (no system install required).
The plain `Makefile` build still works for headless scenarios but stubs out `run_gui()`.

```sh
# headless (scenarios + unit tests)
make            # → build/babaiwt, build/unit_tests
make check      # runs both

# GUI
cmake -S . -B build_cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build_cmake -j
build_cmake/src/app/babaiwt --edit                # blank editor
build_cmake/src/app/babaiwt --edit my.level       # open existing level
build_cmake/src/app/babaiwt --play my.level       # jump straight into play
build_cmake/src/app/babaiwt --test scenario.test  # legacy headless runner
```

CMake enforces layer dependencies (see
[architecture.md §4](architecture.md#4-build-time-layer-enforcement));
adding `#include <raylib.h>` to `core/` will fail to configure.

`build_cmake/` is gitignored; `build/` (Makefile artifacts) is also gitignored.

## Modes

| Mode | Entered via | What happens |
|------|-------------|--------------|
| Edit | `--edit`, or pressing `ESC` from Play | World is mutable via mouse; tick engine paused; edit-undo stack active. |
| Play | `--play`, or pressing `Enter`/`Space` from Edit | World is snapshotted; tick engine accepts inputs; tick-undo stack active. |

Switching back to Edit restores the snapshot taken on the last Play entry
(playthroughs are non-destructive to the level being authored).

## Controls — Edit mode

| Input | Action |
|-------|--------|
| Left mouse drag (viewport)      | Place selected palette entry on hovered tile (one per tile while held). |
| Right mouse drag (viewport)     | Delete every object on hovered tile. |
| Middle mouse drag               | Pan viewport. |
| Mouse wheel (over viewport)     | Zoom, anchored at cursor position. |
| Mouse wheel (over palette)      | Scroll palette list. |
| Arrow keys                      | Pan viewport (1 tile per press). |
| `Ctrl+Q` / `Ctrl+E`             | Previous / next palette entry. |
| `Ctrl+Tab`                      | Toggle text-twin of selected entry (nouns only). |
| `Ctrl+R`                        | Rotate selected entry's default facing. A direction triangle on the tile edge shows facing. |
| Click palette entry             | Select that entry directly. |
| Type any char (no Ctrl)         | Jump to palette search box, pre-filled with the typed character. `ESC` exits. |
| Click search box                | Focus search box directly. |
| `Ctrl+S`                        | Save. Prompts for filename if the level has never been saved. |
| `Ctrl+Shift+S`                  | Save as (always prompts). |
| `Ctrl+O`                        | Open level (modal text-input dialog). |
| `Ctrl+N`                        | New blank level. |
| `Ctrl+Z`                        | Undo last edit (place / delete). Buffer cap: 500. |
| Shift + left mouse drag         | Box-select a rectangular region. |
| `Ctrl+C` (with selection)       | Copy selection to clipboard. |
| `Ctrl+X` (with selection)       | Cut selection to clipboard (undoable). |
| `Ctrl+V` (with clipboard)       | Enter paste mode; hover to preview, LMB to stamp. |
| `Ctrl+Shift+E` (with selection) | Save selection as a schematic. Prompts for name → saves to `schematics/<name>.schem`. Then enter tag mode: LMB=input tile (blue), RMB=output tile (red), MMB=clear, Enter=save. |
| `Ctrl+I`                        | Import schematic. Opens fuzzy picker of `schematics/`; type to filter, Up/Down to navigate, Enter to load. Then LMB to stamp; `Ctrl+R` to rotate 90° CW. |
| `Ctrl+B`                        | Toggle global abstract view for all placed schematics (conforming outline, gray fill, blue=input, red=output, name label). |
| `Enter` / `Space`               | Enter Play mode (snapshots world). |

## Controls — Play mode

| Input | Action |
|-------|--------|
| Arrow keys / `WASD`             | One tick of input in that direction. |
| `Space`                         | Wait tick. |
| `Z` (or `Ctrl+Z`)               | Undo one tick (cap: 10,000). |
| Numpad `4`/`6`/`8`/`2`          | Pan viewport. |
| `P` / `F5`                      | Toggle auto-tick (timed auto-advance). HUD shows `AUTO:Nms`. |
| `+` / `-`                       | Increase / decrease auto-tick speed by 50 ms (range 50 ms – 5000 ms). |
| `0`                             | Toggle max-speed auto-tick (one tick per frame). |
| `ESC`                           | Return to Edit mode (restores snapshot). |

Note: `ESC` does **not** close the window (raylib's default exit key is disabled).
Use the window's close button or Alt+F4 to exit.

## Palette panel

- Fixed 40px width — does not resize when you zoom the viewport.
- **Search box** at the top: click it, type to filter by substring, Backspace to edit,
  ESC to dismiss. The list scrolls independently of the viewport.
- `Tab` flips the `is_text` flag for the currently-selected entry (nouns only).
- **Tile appearance**: object tiles use solid vivid colors + ALL-CAPS labels
  (e.g. pink "BABA"); text tiles use color-coded backgrounds by word type:
  - Nouns: cream background, dark blue text
  - Operators (IS, AND, NOT): light blue background
  - Properties (YOU, WIN, PUSH, …): light green background
- Defined in `src/editor/palette.hpp` (`default_palette()`).

## Rendering

No sprites yet — all tiles are colored rectangles with text labels. The
draw order per tile is: non-text objects first (ascending id), then text
objects (ascending id). Multiple objects on the same tile stack with a
small vertical offset.

Grid lines and a fixed right-side rules panel ("ACTIVE RULES") are always
visible. The HUD bar at the bottom shows current mode, filename (with `*`
when dirty), and a keyboard hint.

## File formats

See [file-format-v1.md](file-format-v1.md) for the canonical `.level`
and `.test` grammar. Save uses `core::serialize_level`; load uses
`core::load_level_file`. Round-trips are lossless for everything the
loader knows about.

## Schematics

Schematics (`.schem` files) are reusable snippets of tile layouts.

**Creating a schematic:**
1. Shift-drag to select the region.
2. `Ctrl+Shift+E` → type a name → Enter.
3. In tag mode, click tiles to mark them as inputs (LMB, blue) or outputs (RMB, red). Enter to save.
4. The file is written to `schematics/<name>.schem` and abstract view is enabled automatically.

**Importing a schematic:**
1. `Ctrl+I` → fuzzy-search the list → Enter.
2. Move cursor to position the preview; `Ctrl+R` to rotate 90° CW.
3. LMB to stamp. `ESC` to cancel.
4. `Ctrl+B` toggles the abstract overlay for all placed schematics.

Schematics can be nested: a `.schem` file may reference other `.schem` files via `schem` records (see [file-format-v1.md §5](file-format-v1.md)).

## Known issues

See [feature-status.md §9](feature-status.md#9-known-bugs-open) for the current bug list.
BUG-1 and BUG-3 (id-preserving undo) are **FIXED**. No open GUI bugs at this time.
