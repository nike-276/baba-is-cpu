# src/ — code layout

Layers, kept strictly one-way:

```
app/  ──▶  { cli/ , editor/ , render/ }  ──▶  sim/  ──▶  core/
```

`core/` depends on the C++ stdlib only. `sim/` adds runtime state
(undo, tick counter) around `core/`. `render/` and `editor/` consume
`sim/` + `core/`. `cli/` consumes `core/` directly. `app/` is the binary
entry point and the only place that knows about both raylib and the
headless code path.

CMake enforces these directions (see
[../docs/architecture.md §4](../docs/architecture.md#4-build-time-layer-enforcement)).
Adding `#include <raylib.h>` to anything below `render/` fails the
configure step.

## Subdirectories

- [core/](core/CLAUDE.md) — pure simulator: World, Kind/Object, RuleSet
  parser, tick engine, .level/.test loader. See
  [core/STATUS.md](core/STATUS.md) for what's actually implemented.
- [sim/](sim/CLAUDE.md) — `Simulator` + `UndoBuffer` ring. Wraps the
  pure tick engine with per-session state.
- [render/](render/CLAUDE.md) — raylib drawing only. No mutation.
- [editor/](editor/CLAUDE.md) — palette, place/delete, play↔edit
  toggle, dual undo stacks (tick + edit), file I/O.
- [cli/](cli/CLAUDE.md) — scenario runner: takes a parsed `TestScenario`,
  drives the tick engine, evaluates assertions, prints pass/fail.
- [app/](app/CLAUDE.md) — `babaiwt` entry point. Argument parsing,
  GUI loop dispatch (`gui_loop.cpp`) or headless stub (`gui_stub.cpp`).

## Includes & namespacing

- All code lives in `namespace baba::core { ... }` (or `baba::cli`,
  `baba::app`).
- Headers use `#pragma once`.
- Internal helpers go in anonymous namespaces inside `.cpp` files,
  not in headers.
- Build flags treat warnings as errors for `return-type`; do not
  silence other warnings — fix them.
