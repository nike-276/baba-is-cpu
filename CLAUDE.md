# baba-is-true — Claude session guide

A from-scratch Baba Is You simulator in C++. Long-term goal: Build the Baba Is You simulator and ensure that it works properly. This file orients new Claude sessions; nested CLAUDE.md files
exist in every meaningful subdirectory and go deeper.

## Where we are

Phase 1 is done: deterministic tick engine, sparse spatial index, rule
parser, .level + .test loaders, scenario runner, unit harness.
Phase 2 (raylib GUI + sprites recreated from the wiki) is next.

Ground truth specs live in `docs/`:

- [docs/architecture.md](docs/architecture.md) — module layout, tick
  pipeline, determinism rules.
- [docs/rule-engine-spec.md](docs/rule-engine-spec.md) — what each
  property means, parse grammar, edge cases.
- [docs/file-format-v1.md](docs/file-format-v1.md) — `.level` and
  `.test` text formats.

If you change behavior, update the spec docs in the same commit.

## Build / run

Plain `Makefile` (clang++, C++20, `-Wall -Wextra -Wpedantic -Wshadow
-Werror=return-type`). CMake is deferred to Phase 2 with raylib.

```sh
make            # builds babaiwt + unit_tests into build/
make test       # runs every tests/scenarios/*.test through babaiwt
make check      # unit tests + scenario tests
make clean
```

`build/` and `*.o` are gitignored.

## Layout

```
src/
  core/    pure simulation (no I/O, no GUI). The thing other phases plug into.
  cli/     scenario runner — comparing simulator output to .test expectations.
  app/     entry point (`babaiwt`).
tests/
  scenarios/  human-readable .test files (the behavior contract).
  unit/       C++ unit tests with the in-tree harness (tests/unit/check.hpp).
docs/      v1 specs (architecture + rule engine + file format).
```

Each of those directories has its own CLAUDE.md.

## How we work here

- TDD: write failing test (RED, commit), implement to pass (GREEN, commit),
  optional refactor (commit). The skill `tdd-workflow` formalizes this and
  the user has authorized commits per checkpoint.
- Determinism is non-negotiable. See `docs/architecture.md §5` for the
  ordering rules (ascending object id, ascending (y, x), etc.).
- Phase 1 keeps everything in plain Makefile + stdlib only — no external
  deps yet. Resist adding any until Phase 2.
- Phase 2 GUI: recreate sprites from `babaiswiki_pages_current.xml` and the
  Baba Is You wiki itself. Static pixel sprites, not the animated
  multi-frame originals.

## What's next

1. Phase 2 scaffold: CMakeLists, raylib dependency, sprite atlas pipeline.
2. Implement remaining tick phases (defeat/sink/melt/open-shut, undo
   stack) with the same TDD loop.
3. Once the simulator is feature-complete enough for "WALL IS PUSH" /
   transformations, try to make the simulator robust enough for a user to create a level.
