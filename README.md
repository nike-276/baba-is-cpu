# baba-is-true

A from-scratch Baba Is You simulator and level editor in C++. Implements the full rule engine (IS, AND, NOT, ON, MAKE, EAT, HAS, FOLLOW, FEAR, POWERED, PLAY, and more) with a raylib GUI editor and headless test harness.

---

## Requirements

- **C++20** compiler (GCC 12+ or Clang 15+)
- **CMake 3.16+** for the GUI build
- **Make + GNU toolchain** for the headless build
- raylib 5.0 — fetched automatically by CMake (no install required)

---

## Building

### GUI build (editor + play loop)

```sh
cmake -S . -B build-cmake
cmake --build build-cmake -j
```

Binary: `build-cmake/babaiwt`

### Headless build (scenarios + unit tests only)

```sh
make            # → build/babaiwt  +  build/unit_tests
make check      # run unit tests + all scenario tests
make test       # scenario tests only
```

---

## Running

### Open the level editor

```sh
./build-cmake/babaiwt --edit                 # blank level
./build-cmake/babaiwt --edit my.level        # open an existing level
```

### Play a level directly

```sh
./build-cmake/babaiwt --play my.level
```

### Run a test scenario (headless)

```sh
./build-cmake/babaiwt --test tests/scenarios/01-baba-is-you.test
# or with the Makefile build:
./build/babaiwt tests/scenarios/01-baba-is-you.test
```

---

## Editor controls

### Edit mode

| Input | Action |
|-------|--------|
| Left mouse drag | Place selected tile on hovered cell |
| Right mouse drag | Delete all objects on hovered cell |
| Middle mouse drag | Pan viewport |
| Mouse wheel (viewport) | Zoom, anchored at cursor |
| Mouse wheel (palette) | Scroll palette |
| Arrow keys | Pan viewport 1 tile |
| Click palette entry | Select that entry |
| Type any character | Open palette search (type to filter, ESC to close) |
| `Ctrl+Q` / `Ctrl+E` | Previous / next palette entry |
| `Ctrl+Tab` | Toggle text ↔ object variant (nouns only) |
| `Ctrl+R` | Rotate selected entry's default facing |
| `Ctrl+Z` | Undo last edit |
| `Ctrl+S` | Save (prompts for filename on first save) |
| `Ctrl+Shift+S` | Save as |
| `Ctrl+O` | Open level |
| `Ctrl+N` | New blank level |
| Shift + left drag | Box-select a region |
| `Ctrl+C` / `Ctrl+X` | Copy / cut selection |
| `Ctrl+V` | Paste — hover to preview, LMB to stamp |
| `Ctrl+Shift+E` | Save selection as a schematic |
| `Ctrl+I` | Import schematic (fuzzy picker) |
| `Ctrl+B` | Toggle abstract view for all placed schematics |
| `Enter` or `Space` | Switch to Play mode |

### Play mode

| Input | Action |
|-------|--------|
| Arrow keys / WASD | Move one tick |
| Space | Wait tick |
| Z / Ctrl+Z | Undo one tick |
| P / F5 | Toggle auto-tick (HUD shows `AUTO:Nms`) |
| + / - | Speed auto-tick up / down by 50 ms |
| 0 | Toggle max-speed auto-tick (one tick per frame) |
| ESC | Return to Edit mode (restores the pre-play snapshot) |

Closing the window: use the OS close button or Alt+F4. ESC only exits Play mode.

---

## Palette

The left panel lists every available tile. Each entry shows a colored preview square and label. Colors indicate tile type:
- **Object tiles** — vivid solid color, ALL-CAPS label (e.g. pink "BABA")
- **Noun text tiles** — cream background, dark blue label (e.g. "BABA")
- **Operator tiles** — light blue (IS, AND, NOT, PLAY, …)
- **Property tiles** — light green (YOU, WIN, PUSH, …)

Type any character to filter the palette by name substring.

---

## File formats

| Extension | Purpose |
|-----------|---------|
| `.level` | A playable level (version header + tile records) |
| `.test` | Test scenario: `[setup]` level + `[inputs]` actions + `[expected]` assertions |
| `.schem` | Reusable schematic fragment with optional I/O port tags |

All formats are plain text, hand-editable. Full grammar in [`docs/file-format-v1.md`](docs/file-format-v1.md).

---

## Writing a level

Level files are plain text:

```
version 1
name "Hello"

# Rule strip
text 0 0 baba
text 1 0 is
text 2 0 you

text 0 2 flag
text 1 2 is
text 2 2 win

# Objects
object 5 5 baba right
object 10 5 flag right
```

Rules are formed by placing text tiles on the grid so they read `NOUN IS PROPERTY` horizontally or vertically. The engine re-parses rules every tick.

---

## Writing a test scenario

```
[setup]
version 1
name "baba can push wall"

text 0 0 baba
text 1 0 is
text 2 0 you

text 0 1 wall
text 1 1 is
text 2 1 push

object 5 5 baba right
object 6 5 wall right

[inputs]
input right

[expected]
at 6 5 baba
at 7 5 wall
tick 1
not_won
```

Supported assertions: `at`, `not_at`, `text_at`, `count`, `text_count`, `won`, `not_won`, `tick`, `sound_count`.

Run all scenarios: `make test`

---

## Implemented rules

The engine supports: `IS`, `AND`, `NOT`, `ON`, `NOT ON`, `MAKE`, `EAT`, `HAS`, `FOLLOW`, `FEAR`, `POWERED` / `POWERED2` / `POWERED3`, `PLAY`.

Properties: `YOU`, `PUSH`, `STOP`, `WIN`, `DEFEAT`, `SINK`, `HOT`, `MELT`, `OPEN`, `SHUT`, `WEAK`, `MOVE`, `AUTO`, `FALL*`, `UP/DOWN/LEFT/RIGHT`, `NUDGE*`, `POWER` / `POWER2` / `POWER3`.

Full catalog with implementation status: [`docs/feature-status.md`](docs/feature-status.md)

---

## Project layout

```
src/
  core/      Pure simulation — World, rules, tick engine, loader
  sim/       Simulator + undo buffer wrapping core/
  render/    raylib drawing (no mutation)
  editor/    Palette, place/delete, play↔edit toggle
  cli/       Headless scenario runner
  app/       Entry point + GUI loop
tests/
  scenarios/ .test scenario files (the behavior contract)
  unit/      C++ unit tests
docs/        Architecture, rule-engine spec, file format spec, GUI guide
```
