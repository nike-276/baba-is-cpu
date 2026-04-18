# baba-is-true — Claude session guide

A from-scratch Baba Is You simulator in C++. Long-term goal: a complete,
deterministic simulator that supports the full Baba rule catalog and can
be authored + played from the bundled raylib editor. This file orients
new Claude sessions; nested `CLAUDE.md` files exist in every meaningful
subdirectory and go deeper.

## Where we are

Phase 1 (headless engine) is GREEN: deterministic 9-phase tick pipeline,
sparse spatial index, rule parser with `NOT`/`AND`/`ON`/`NOT ON`/`MAKE`/`EAT`
resolution + `X IS X` protection + `X IS Y` conditional/unconditional transforms,
.level/.test loaders, scenario runner, in-tree unit harness.
44 scenarios + 39 unit tests pass.

Noun catalog: `BABA`, `WALL`, `ROCK`, `FLAG`, `WATER`, `LAVA`, `SKULL`,
`KEY`, `DOOR`, `KEKE`, `FOFO`, `ME`, `BOX`, `LEAF`, `CLOUD`, `SUN`, `MOON`,
`STAR`, `PLANET`, `BOLT`, `LOVE`, `BOMB`, `WIND`, `TEXT`.

Phase 2 (raylib GUI editor + play loop) is **in progress**. CMake build
exists; `babaiwt --edit` / `--play` work; palette + search + zoom are
wired. One known bug (BUG-1 undo of destroyed objects) is tracked in
[docs/feature-status.md §9](docs/feature-status.md#9-known-bugs-open).
BUG-2 (zoom shake) is **FIXED**.

## Source-of-truth docs (`docs/`)

- [architecture.md](docs/architecture.md) — module layout, tick
  pipeline, determinism rules, layer enforcement.
- [rule-engine-spec.md](docs/rule-engine-spec.md) — what each property
  means, parse grammar, edge cases.
- [file-format-v1.md](docs/file-format-v1.md) — `.level` and `.test`
  text formats.
- [feature-status.md](docs/feature-status.md) — **master checklist of
  every Baba construct** with implementation status + known bugs.
- [gui-guide.md](docs/gui-guide.md) — build, run, controls, modes.
- [src/core/STATUS.md](src/core/STATUS.md) — co-located mirror of the
  rule-engine portion of feature-status (update this when you touch
  the rule engine).

If you change behavior, update the spec docs in the same commit.

## Build / run

Two parallel build systems on purpose:

```sh
# Headless (Phase 1) — plain Makefile, stdlib only, no raylib.
make            # → build/babaiwt + build/unit_tests
make test       # runs every tests/scenarios/*.test through babaiwt
make check      # unit + scenario tests
make clean

# GUI (Phase 2) — CMake + FetchContent raylib 5.0.
cmake -S . -B build-cmake
cmake --build build-cmake -j
./build-cmake/babaiwt --edit [file.level]
./build-cmake/babaiwt --play file.level
./build-cmake/babaiwt --test scenario.test
```

`build/` and `build-cmake/` are gitignored.

## Layout

```
src/
  core/    pure simulation (no I/O, no GUI).
  sim/     Simulator + UndoBuffer wrapping core/.
  render/  raylib drawing (consumes World + RuleSet, never mutates).
  editor/  palette, place/delete, play↔edit toggle, dual undo.
  cli/     scenario runner.
  app/     entry point (`babaiwt`); GUI loop or headless stub.
tests/
  scenarios/  human-readable .test files (the behavior contract).
  unit/       C++ unit tests with the in-tree harness (tests/unit/check.hpp).
docs/      v1 specs + status + GUI guide.
```

Each of those directories has its own CLAUDE.md.

## How we work here

- **TDD always.** Write failing test (RED, commit), implement to pass
  (GREEN, commit), optional refactor (commit). The `tdd-workflow`
  skill formalises this; commits per checkpoint are pre-authorised.
- **Determinism is non-negotiable.** See
  [architecture.md §5](docs/architecture.md#5-determinism-contract)
  for the ordering rules (ascending object id, ascending (y, x), no
  RNG, no hash-iteration leakage).
- **Engine generality > narrow implementation.** The rule engine
  must architecturally accommodate the entire Baba catalog
  (`docs/feature-status.md`) even if only a subset is currently
  wired. New properties = new rows in `kTable` + a tick-phase hook.
- **Spec deviations are explicit.** Marked `[DEVIATION]` in
  rule-engine-spec.md with rationale. Current notable deviation:
  undo of `Destroy` will preserve the original object id (spec §6
  said fresh id; preserving avoids a chained-Move bug — see BUG-1).
- **Wiki is reference, not contract.** `babaiswiki_pages_current.xml`
  lives in repo root; consult it before guessing semantics.

## What's next

1. **Phase B — Bug fixes** (TDD). BUG-1 (id-preserving respawn for
   undo); BUG-2 (float scroll for zoom).
2. **Phase C — Rule engine extensions** (TDD per group). Order:
   per-object property derivation → directional → MOVE/AUTO/FALL →
   WEAK → PULL/SHIFT/SWAP → EAT → MAKE → TEXT predicate → ON
   condition. Status tracked in `docs/feature-status.md` and
   `src/core/STATUS.md`.
3. **Phase D — Sprite atlas** recreated from the wiki (static pixel
   sprites, not animated). Replaces the placeholder coloured tiles
   in `render/tile_colors.hpp`.
