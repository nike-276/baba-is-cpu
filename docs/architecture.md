# Architecture v1

> Source of truth for module layout, dependency direction, and the locked design decisions made during planning.
> Companion to `rule-engine-spec.md` (semantics) and `file-format-v1.md` (disk).

## 1. Goals

1. **Pure simulation core.** Deterministic, header-only-friendly, no I/O, no graphics, no globals. Trivially testable from a string fixture.
2. **One binary, two faces.** A single `babaiwt` executable runs interactively (raylib + dear imgui) or headless (`--headless`), with a CLI for batch scenarios. No DLL split, no separate server.
3. **Edit and play in the same address space.** Mode is a runtime flag, not a separate process; switching to play snapshots the world, switching back restores it.
4. **Diff-based undo, always on.** Every forward tick records a `Change` list; undo is replay-in-reverse. Capped buffer, not unbounded.


## 2. Locked Design Decisions

These are the 17 decisions resolved during planning. Listed here so subsequent work can grep for them.

| # | Decision                                                                                  | Rationale                                                          |
|---|-------------------------------------------------------------------------------------------|--------------------------------------------------------------------|
| 1 | Spatial index is a sparse `unordered_map<Coord, vector<ObjectId>>`.                       | Infinite-world requirement; simple; benchmark before swapping.     |
| 2 | Object id is `uint32_t`, monotonically allocated, never reused.                           | Stable diff records; deterministic tiebreaker.                     |
| 3 | Layering is implicit: text above non-text, then by ascending id.                          | No z-field on disk or in object; renderer derives at draw time.    |
| 4 | Single binary `babaiwt` with `--headless` flag.                                           | Avoids GUI/CLI duplication; lets `--headless` pop GUI on breakpoint. |
| 5 | Headless does NOT render. Computation is what we want to keep fast.                       | Rendering is the optional path, sim is mandatory.                  |
| 6 | Tick rate is configurable (`--tps N`) including `--uncapped`.                             | Required for benchmarking and CPU-driven workloads.                |
| 7 | Breakpoints (tick count, win/defeat, custom predicate) auto-pause and can pop the GUI.    | Headless is interactive when needed.                               |
| 8 | Undo buffer cap is 10,000 ticks by default, even in headless. `--no-undo` disables it.    | Memory bounded but generous; opt-out for long CPU runs.            |
| 9 | Editor mode keeps the pre-play world snapshot. Switching back to edit restores it.        | Unifies edit/play undo without two separate stacks.                |
|10 | Coexistence rule: arbitrary `object` overlap allowed; `text` is unique per tile.          | Matches Baba semantics enough for v1; avoids stamping ambiguity.   |
|11 | No JSON anywhere in `core` or `sim`. Disk format is the line-based text from §3.          | One representation; no schema drift.                               |
|12 | Test fixtures are `.test` files (setup + inputs + expected) parsed by the same loader.    | Same code path as gameplay; no test-only mocks.                    |
|13 | The rule engine is specified independently of the wiki and lives in `rule-engine-spec.md`. | Wiki is reference, not contract.                                   |
|14 | Determinism: no RNG anywhere in `core` or `sim`. Multiple-YOU resolved by ascending id.   | Reproducible behavior is the whole point.                      |
|15 | Test-first for the rule engine: scenario `.test` files exist before the engine compiles.  | Forces the spec to be operational, not aspirational.               |
|16 | CMake enforces layer dependencies (see §4). A `core` source including `<raylib.h>` fails to configure. | Architectural drift is caught at build time.            |
|17 | Phase 1 ships: core skeleton, loader, headless tick loop, ≥6 passing `.test` scenarios. No GUI yet. | Smallest verifiable slice; GUI is a renderer over a working sim.   |

## 3. Layered Module Layout

```
+----------------------------------------------------+
|  app    (main, arg parsing, top-level loop)        |
+----------------------------------------------------+
|  cli    (headless driver, .test runner, breakpoints)|
+----------------------------------------------------+
|  editor (mode state, schematics, paste, undo glue) |
+----------------------------------------------------+
|  render (raylib + dear imgui via rlImGui)          |
+----------------------------------------------------+
|  sim    (tick loop, undo buffer, breakpoint API)   |
+----------------------------------------------------+
|  core   (world, rule engine, file format, kinds)   |
+----------------------------------------------------+
```

Dependencies flow downward only. `core` depends on the C++ standard library and nothing else.

### 3.1 `core` — pure simulation primitives

- `World`: sparse spatial index, object table, id allocator.
- `Object`, `TextObject`, `Kind`, `Direction`, `Coord`.
- `RuleSet`, `Rule`, parser (text strip → rules), resolver (NOT/AND, X-IS-X, base rules).
- `apply_tick(World&, Input) -> TickReport` — implements the 9-phase tick from `rule-engine-spec.md`.
- `Change` records (Move/Face/Transform/Spawn/Destroy) and the inverse function.
- Loader/serializer for `.level`, `.schem`, `.test` (single grammar, three section sets).
- Zero `iostream` use in hot paths (loader uses `std::istream`; sim does not).

### 3.2 `sim` — runtime around the core

- `Simulator` owns a `World`, an `UndoBuffer`, a `Clock`, and a `BreakpointSet`.
- Step API: `step_forward(Input)`, `step_back()`, `run_until(predicate, max_ticks)`.
- Headless API: same as above but the embedder drives the loop. No frame timer.
- `UndoBuffer` is a ring of per-tick `vector<Change>` capped at 10,000. `--no-undo` swaps in a no-op buffer.
- Breakpoints expose a `should_break(World&, TickReport&) -> bool` callback hook so `cli` and `editor` can attach without `sim` knowing about them.

### 3.3 `render` — drawing only

- Pure consumer of `const World&`. Never mutates.
- Sprite atlas, camera, layered draw order (per decision #3).
- Provides imgui panels: rule list, object inspector, breakpoint editor, undo timeline.
- `render` may depend on `sim` to read state and to install breakpoints, but never calls `apply_tick` itself.

### 3.4 `editor` — authoring

- Owns the pre-play snapshot (decision #9). Toggling play stashes a `World` clone; toggling back restores it and discards the play-mode undo buffer.
- Schematic paste calls into `core` for coexistence checks; failed pastes roll back via the same `Change`/undo machinery.
- Brush, selection, fill, save/load — all expressed as batched `Change` lists so undo behaves identically in edit and play.

### 3.5 `cli` — batch + headless driver

- `babaiwt --headless --run scenario.test` → loads, ticks through `[inputs]`, evaluates `[expected]`, exits with status.
- `babaiwt --headless --tps uncapped --break tick=10000 level.level` → run until tick 10000 then either exit or pop the GUI based on `--gui-on-break`.
- Owns argv parsing; everything else is delegated.

### 3.6 `app` — entry point

- `main()`. Decides which surface to start (GUI vs headless), constructs the simulator, hands off.
- The only place that knows about both raylib init and the headless loop.

## 4. Build-Time Layer Enforcement

CMake structure:

```
src/
  core/   CMakeLists.txt   # add_library(core STATIC ...) ; target_link_libraries(core PUBLIC) — none
  sim/    CMakeLists.txt   # links core only
  render/ CMakeLists.txt   # links sim, raylib, imgui, rlImGui
  editor/ CMakeLists.txt   # links sim (and core transitively)
  cli/    CMakeLists.txt   # links sim
  app/    CMakeLists.txt   # links cli, editor, render
```

A `core` translation unit attempting `#include <raylib.h>` fails to configure because `core`'s target has no link or include path to it. CI runs `cmake --build` with `-Werror` and a smoke check that greps `core/` for forbidden includes.

## 5. Determinism Contract

- No `std::rand`, no `std::random_device`, no time-based input in `core` or `sim`. A grep gate enforces this in CI.
- Iteration over the spatial index for sim purposes always sorts by id (or by `(y, x, id)` where geometric order matters). `unordered_map` iteration order is never observed externally.
- Multi-YOU input is applied in ascending id order (decision #14).
- Ties in transformation/destruction are resolved per `rule-engine-spec.md` §5–§7 — the spec is the contract, not the implementation.

## 6. Threading

v1 is single-threaded. The simulator runs on the main thread; raylib renders on the main thread. 

## 7. Error Handling

- `core` returns `std::expected<T, ParseError>` (or equivalent tagged result) for loaders. No exceptions across module boundaries.
- `sim` API never throws on user input (bad direction, etc.); it returns a `TickReport` with the failure reason.
- `app`/`cli` translate failures into argv-friendly exit codes (`0` ok, `2` parse error, `3` test assertion failed, `4` runtime breakpoint with no GUI).

## 8. Logging and Telemetry

- `core` and `sim` are silent. Diagnostics go through a `Sink` interface passed in by the embedder. The default sink in `cli` writes to stderr; the GUI sink routes to an imgui panel.
- No global logger.

## 9. What Phase 1 Builds (decision #17)

In order:

1. `core::Coord`, `Direction`, `Kind`, `Object`, `World` (sparse map + id allocator) with unit tests.
2. `core::loader` for `.level` and `.test` (parse + serialize round-trip).
3. `.test` runner skeleton in `cli` that loads, evaluates `[expected]` against a no-op tick, and reports failures.
4. Six `.test` scenarios written FIRST (per decision #15), all RED:
   - baba-is-you-basic-move
   - push-single-wall
   - push-blocked-by-stop
   - rule-forms-on-formation
   - rule-breaks-on-disassembly
   - flag-is-win-triggers-won
5. Implement the 9-phase tick incrementally until the six scenarios are GREEN.
6. Wire `UndoBuffer` (with cap) and add an undo regression test.

No raylib, no imgui, no editor in Phase 1. They land in Phase 2 once the sim is trustworthy.

## 10. Out-of-Scope for v1

- Multiplayer / networking.
- Save states beyond the in-memory snapshot.
- Procedural generation.
- Mod loading.
- Anything resembling a level browser or asset pipeline.
- Sound.
