# tests/ — two test pyramids

```
tests/
├── scenarios/   ← behavior contract: human-readable .test files
└── unit/        ← C++ unit tests using the in-tree harness
```

Both run under `make check`.

## scenarios/

Each `.test` file exercises the simulator end-to-end through the
`.test` text format defined in [docs/file-format-v1.md](../docs/file-format-v1.md).
The runner (`src/cli/test_runner.cpp`) loads the scenario, calls
`apply_tick` for each input, and evaluates the `[expected]` block.

These are the contract. Any new tick phase or rule property gets at
least one scenario file before any code is written. See the nested
[scenarios/CLAUDE.md](scenarios/CLAUDE.md) for the file index and what
each one covers.

## unit/

C++ unit tests using `tests/unit/check.hpp` — a ~100-line header-only
harness with `TEST(suite, name)`, `CHECK`, `CHECK_EQ`, `CHECK_NE`. No
GoogleTest dependency in Phase 1; if Phase 2 brings CMake we may swap
for gtest.

Current coverage targets:

- `world_test.cpp` — `World` invariants (monotonic ids, empty-cell
  purge, multi-object cells, ordering of `all_ids` / `all_cells`).
- `loader_test.cpp` — `.level` and `.test` parsing, error paths,
  `serialize_level` round-trip.

Add a unit test when:

- you introduce a new core helper (e.g. a coord arithmetic function);
- you fix a parser edge case (regression test);
- a scenario file would be overkill for a small invariant.

Add a scenario test when the change is observable through inputs and
expected world state — that's almost everything tick-related.

## Running

```sh
make test     # scenarios only
make unit     # build unit binary, doesn't auto-run
make check    # builds + runs both
```

`build/unit_tests` exits non-zero on any failure; `babaiwt --test ...`
exits 1 on any scenario failure.
