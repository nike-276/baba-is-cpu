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
| `Q` / `E`                       | Previous / next palette entry. |
| `Tab`                           | Toggle text-twin of selected entry (nouns only). |
| `R`                             | Rotate selected entry's default facing. |
| Click palette entry             | Select that entry directly. |
| Click search box / type         | Filter palette by substring (case-insensitive); `ESC` exits the search box. |
| `Ctrl+S` / `Ctrl+O` / `Ctrl+N`  | Save / open / new level. |
| `Ctrl+Z`                        | Undo last edit (place / delete). Buffer cap: 500. |
| `Enter` / `Space`               | Enter Play mode (snapshots world). |

## Controls — Play mode

| Input | Action |
|-------|--------|
| Arrow keys / `WASD`             | One tick of input in that direction. |
| `Space`                         | Wait tick. |
| `Z` (or `Ctrl+Z`)               | Undo one tick (cap: 10,000). |
| Numpad `4`/`6`/`8`/`2`          | Pan viewport. |
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

## Known issues

See [feature-status.md §9](feature-status.md#9-known-bugs-open) for the
current bug list. The most visible currently:

- **BUG-1 / BUG-3**: Undo of a destroyed object respawns it with a fresh id;
  subsequent Move records in the same undo batch target the now-missing original
  id, so the object may land in the wrong place. Fix requires `world.respawn(id, …)`.
