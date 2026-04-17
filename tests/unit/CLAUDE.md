# tests/unit/ — C++ unit tests

Tiny in-tree harness. No external test framework dependency.

## The harness (`check.hpp`)

```cpp
#include "check.hpp"
#include "core/world.hpp"

TEST(World, spawn_assigns_monotonic_ids) {
    baba::core::World w;
    auto id = w.spawn({0, 0}, baba::core::Kind::N_Baba, false);
    CHECK_EQ(id, baba::core::ObjectId{0});
    CHECK(w.occupied({0, 0}));
}
```

- `TEST(suite, name)` auto-registers via a static `Registrar`.
- `CHECK(cond)`, `CHECK_EQ(a, b)`, `CHECK_NE(a, b)`, `CHECK_TRUE`,
  `CHECK_FALSE`. Failures record `file:line` and a message.
- `main.cpp` calls `baba::testing::run_all()` and returns 0/1.

### Gotcha — braced initializers in `CHECK`

The macro is function-like; `CHECK(o->pos == Coord{2, 1})` confuses the
preprocessor (the `,` inside the brace is read as a macro arg
separator). Wrap such expressions: `CHECK((o->pos == Coord{2, 1}))`.

## What's tested today

- `world_test.cpp` — every public method of `World`, plus its core
  invariants:
  - monotonic + non-reused ids;
  - empty-cell purge from the spatial index;
  - `at` / `get` safe defaults on unknown lookups;
  - `move`'s same-position no-op; multi-object cells preserved;
  - sort orders for `all_ids` / `all_cells`.
- `loader_test.cpp` — `.level` and `.test` parsers, including:
  - missing / duplicate version + name;
  - unknown keywords / sections;
  - object kind must be a noun, facing whitelist, integer parsing;
  - text coexistence rule (≤1 text per tile);
  - `serialize_level` round-trips to identical worlds.

## Build

The Makefile globs `tests/unit/*.cpp` into `build/tests/unit/*.o` and
links them with all `core/` objects into `build/unit_tests`. Add a new
file by dropping it in this directory; nothing else to wire.

## When to add a unit test vs a scenario

- Unit test: invariant on a class or pure function (`World`, loader,
  later RuleSet helpers). Runs fast, gives precise failure location.
- Scenario test: anything observable as input → world state → assert.
  Almost all tick-engine work lands as a scenario first.
