# GUI guide — `babaiwt` editor + play loop

## Build

The Phase 2 GUI is a separate CMake build that pulls raylib via
FetchContent. The plain `Makefile` build still works for headless
scenarios but stubs out `run_gui()`.

```sh
# headless (Phase 1 / scenarios)
make            # → build/babaiwt, build/unit_tests
make check      # runs both

# GUI (Phase 2)
cmake -S . -B build-cmake
cmake --build build-cmake -j
./build-cmake/babaiwt --edit                 # blank editor
./build-cmake/babaiwt --edit my.level        # open existing level
./build-cmake/babaiwt --play my.level        # jump straight into play
./build-cmake/babaiwt --test scenario.test   # legacy headless runner
```

CMake enforces layer dependencies (see
[architecture.md §4](architecture.md#4-build-time-layer-enforcement));
adding `#include <raylib.h>` to `core/` will fail to configure.

## Modes

| Mode | Entered via | What happens |
|------|-------------|--------------|
| Edit | `--edit`, or pressing `ESC` from Play | World is mutable via mouse; tick engine paused; edit-undo stack active. |
| Play | `--play`, or pressing `Enter`/`Space` from Edit | World is snapshotted; tick engine accepts inputs; tick-undo stack active. |

Switching back to Edit restores the snapshot taken on the last Play
entry (so playthroughs are non-destructive to the level being authored).

## Controls — Edit mode

| Input | Action |
|-------|--------|
| Left mouse drag                  | Place selected palette entry on hovered tile (one per tile while held — no drag-spam). |
| Right mouse drag                 | Delete every object on hovered tile. |
| Middle mouse drag                | Pan viewport. |
| Mouse wheel (over viewport)      | Zoom (anchored at cursor — see BUG-2). |
| Mouse wheel (over palette)       | Scroll palette list. |
| Arrow keys                       | Pan viewport (1 tile per press). |
| `Q` / `E`                        | Previous / next palette entry. |
| `Tab`                            | Toggle text-twin of selected entry (only meaningful for nouns). |
| `R`                              | Rotate selected entry's default facing. |
| Click search box / type          | Filter palette by substring; `ESC` exits the search box. |
| `Ctrl+S` / `Ctrl+O` / `Ctrl+N`   | Save / open / new level (file dialog via `--load-file` request flags). |
| `Ctrl+Z`                         | Undo last edit (place / delete). Buffer cap: 500. |
| `Enter` / `Space`                | Enter Play mode (snapshots world). |

## Controls — Play mode

| Input | Action |
|-------|--------|
| Arrow keys / `WASD`              | One tick of input in that direction. |
| `Space`                          | Wait tick. |
| `Z` (or `Ctrl+Z`)                | Undo one tick (cap: 10,000). |
| Numpad `4`/`6`/`8`/`2`           | Pan viewport. |
| `ESC`                            | Return to Edit mode (restores snapshot). |

## Palette

- Defined in `src/editor/palette.hpp` (`default_palette()`).
- Entry = `{kind, is_text, default_facing}`.
- `Tab` flips the `is_text` flag for the currently-selected entry.
- Search box filters the list against `kind_name` (case-insensitive
  substring).
- Selected entry is what LMB places on click.

## File formats

See [file-format-v1.md](file-format-v1.md) for the canonical `.level`
and `.test` grammar. Save uses `core::serialize_level`; load uses
`core::load_level_file`. Round-trips are lossless for everything the
loader knows about.

## Known issues (link)

See [feature-status.md §9](feature-status.md#9-known-bugs-open) for the
current bug list — most user-visible right now are BUG-1 (undo of
destroyed objects places them wrong) and BUG-2 (zoom shake at high
scroll values).
