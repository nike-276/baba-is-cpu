# src/cli/ — scenario runner

The thin layer that drives `core::apply_tick` from a parsed
`TestScenario` and compares the resulting `World` against the
`[expected]` block of a `.test` file.

## Files

- `test_runner.hpp` / `test_runner.cpp`
  - `run_test_file(path) → TestResult` — never throws; surfaces parse
    failures via `TestResult::load_error`.
  - `print_summary` / `print_detail` for the human-facing PASS/FAIL
    lines printed by `babaiwt`.

## Action → tick mapping

| `.test` action | What we feed `apply_tick` |
|----------------|---------------------------|
| `wait` / `input wait` | `Input{InputKind::Wait}` |
| `input right/up/left/down` | `Input{InputKind::Move, dir}` |
| `input undo` | **no-op in Phase 1** — undo stack lands when we add the SNAPSHOT phase |

## Assertion → check mapping

| Kind | Pass when |
|------|-----------|
| `at x y kind` | the cell at (x, y) contains at least one non-text object of that Kind |
| `not_at x y kind` | the inverse of `at` |
| `text_at x y kind` | the cell at (x, y) contains a text object of that Kind |
| `count kind n` | exactly `n` non-text objects of that Kind exist in the world |
| `text_count kind n` | exactly `n` text objects of that Kind exist |
| `won` / `not_won` | last `TickReport.won` matches |
| `tick n` | `n` ticks were actually run (i.e. `inputs.size() == n`) |

## What this layer does NOT do

- It does not parse `.test`. That's `core::loader::load_test`.
- It does not render. There's no GUI dependency.
- It does not own the World. It constructs one fresh per scenario from
  the loader and discards it after assertions.

## Phase 2 plans

When raylib lands, a peer `gui/` package will reuse `core/` directly.
This `cli/` will keep existing for headless test runs and CI.
