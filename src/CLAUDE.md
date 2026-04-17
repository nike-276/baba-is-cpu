# src/ — code layout

Three layers, kept strictly one-way:

```
app/  ──▶  cli/  ──▶  core/
```

`core/` knows nothing about the CLI or rendering. `cli/` only depends on
`core/`. `app/` is the binary entry point.

## Subdirectories

- [core/](core/CLAUDE.md) — pure simulator: World, Kind/Object, RuleSet
  parser, tick engine, .level/.test loader. No I/O beyond the loader's
  `std::istream` and no GUI deps.
- [cli/](cli/CLAUDE.md) — scenario runner: takes a parsed `TestScenario`,
  drives the tick engine, evaluates assertions, prints pass/fail.
- [app/](app/CLAUDE.md) — `babaiwt` entry point. Argument parsing only.

## Why the split

Phase 2 will add a `gui/` peer to `cli/` (raylib renderer + input loop)
that depends on the same `core/`. Keeping `core/` pure makes that swap
trivial and keeps the headless test path fast.

## Includes & namespacing

- All code lives in `namespace baba::core { ... }` (or `baba::cli`,
  `baba::app`).
- Headers use `#pragma once`.
- Internal helpers go in anonymous namespaces inside `.cpp` files,
  not in headers.
- Build flags treat warnings as errors for `return-type`; do not
  silence other warnings — fix them.
