# baba-is-true

A from-scratch Baba Is You simulator and level editor in C++. Implements the full rule engine (IS, AND, NOT, ON, MAKE, EAT, HAS, FACING, FACEDBY, FEAR, POWERED, PLAY, and more) with a raylib GUI editor and headless test harness.

---

## Project Overview

A single-cycle CPU using Baba Is You rules and objects. Printing is done via hexadecimal output along two tracks in the simulator, which doubles as a music player. A map of the layout of our CPU can be found at (RENAME THE FILE/ADD IT IN) layout.png.

Signals and data are represented using Objects (we picked all of them for a reason, try to figure out why!):
| Object   | Value |
|----------|-------|
| Baba     | 0b1   |
| Keke     | 0b0   |
| Donut    | 0x0   |
| Stick    | 0x1   |
| Scissors | 0x2   |
| Bubble   | 0x3   |
| Dust     | 0x4   |
| Hand     | 0x5   |
| Sax      | 0x6   |
| Cash     | 0x7   |
| Cog      | 0x8   |
| Cat      | 0x9   |
| Algae    | 0xA   |
| Bottle   | 0xB   |
| Cake     | 0xC   |
| Drink    | 0xD   |
| Egg      | 0xE   |
| Fruit    | 0xF   |

The custom ISA is as follows:

### ISA Binary Code

| Binary Code | Instruction | Description |
|-------------|-------------|-------------|
| `00000000`  | halt        | `pc = pc` |
| `000001tt`  | print       | `print(regs[rt])` |
| `00001000`  | play        | play the stdout as music |
| `0001aatt`  | sub         | `regs[rt] = regs[rt] - regs[ra]` |
| `0010iitt`  | addi        | `regs[rt] = regs[rt] + (immediate value)` |
| `0011aatt`  | cpy         | `regs[rt] = regs[ra]` |
| `1100aatt`  | jz          | `if (regs[ra] == 0): pc = regs[rt]` |
| `1101aatt`  | js          | `if (regs[ra] < 0): pc = regs[rt]` |
| `1110aatt`  | ld          | `regs[rt] = mem[regs[ra]]` |
| `1111aatt`  | st          | `mem[regs[ra]] = regs[rt]` |
| others      | nop         | no operation |

Instructions to run the CPU that we built and the program that plays bad apple and ___OTHER SONGS NAME___.
1. Open cpu.level in the level editor.
2. Using the schematics importer, import the instructions (ex: bad_apple_inst.schem) and place it in the instructions area of the CPU.
3. Again using the schematics importer, import the data (ex: bad_apple_data.schem) and place it in the memory area of the CPU.
4. Hit enter to "run" the CPU.

NOTE: sometimes when importing schematics you may accidentally place an object. CTRL+Z to remove the extra object.

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

Binary: `build-cmake/src/app/babaiwt`

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
./build-cmake/src/app/babaiwt --edit                 # blank level
./build-cmake/src/app/babaiwt --edit my.level        # open an existing level
```

### Play a level directly

```sh
./build-cmake/src/app/babaiwt --play my.level
```

### Run a test scenario (headless)

```sh
./build-cmake/src/app/babaiwt --test tests/scenarios/01-baba-is-you-basic-move.test
# or with the Makefile build:
./build/babaiwt tests/scenarios/01-baba-is-you-basic-move.test
```

### Profile the tick pipeline

```sh
./build/babaiwt --bench <path.level|path.test> [N=500]
```

Runs N wait-ticks (after a 10-tick warm-up) and prints a table of where time goes, sorted by total cost:

```
=== Benchmark — 1000 ticks ===
Phase                     min(µs)  mean(µs)  max(µs)  total(ms)     %
--------------------------------------------------------------------
parse_post_destruct            0.9       1.2      35.7       1.24   14%
apply_destruct                 0.9       1.2       3.9       1.22   14%
parse_post_transform           0.8       1.2       2.0       1.19   13%
parse_post_move                0.8       1.2       1.8       1.17   13%
parse_initial                  0.8       1.1       3.4       1.07   12%
apply_nudge                    0.7       0.9       1.7       0.90   10%
apply_fear                     0.5       0.7       4.7       0.70    8%
...
--------------------------------------------------------------------
TOTAL                          6.8       9.0      44.5       9.03  100%
```

Both `.level` and `.test` files are accepted. Run against your actual slow level for accurate numbers — rule count and object density both affect results significantly.

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
| `Ctrl+R` | Rotate selected entry's default facing; during paste, rotates the paste preview 90° CW |
| `Ctrl+Z` | Undo last edit |
| `Ctrl+S` | Save (prompts for filename on first save) |
| `Ctrl+Shift+S` | Save as |
| `Ctrl+O` | Open level |
| `Ctrl+N` | New blank level |
| Shift + left drag | Box-select a region |
| `Ctrl+C` / `Ctrl+X` | Copy / cut selection |
| `Ctrl+V` | Paste — hover to preview, LMB to stamp |
| `Ctrl+Shift+E` | Save selection as a schematic |
| `Ctrl+F` | Import schematic (fuzzy picker) |
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
| F3 | Toggle benchmark overlay |
| R | Reset benchmark stats |
| Numpad 4/6/8/2 | Pan viewport |

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

The engine supports: `IS`, `AND`, `NOT`, `ON`, `NOT ON`, `MAKE`, `EAT`, `HAS`, `FACING`, `FACEDBY`, `FEAR`, `POWERED` / `POWERED2` / `POWERED3`, `PLAY`.

Properties: `YOU`, `PUSH`, `STOP`, `WIN`, `DEFEAT`, `SINK`, `HOT`, `MELT`, `OPEN`, `SHUT`, `WEAK`, `MOVE`, `AUTO`, `FALL*`, `UP/DOWN/LEFT/RIGHT`, `STILL`, `NUDGE*`, `POWER` / `POWER2` / `POWER3`.

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
