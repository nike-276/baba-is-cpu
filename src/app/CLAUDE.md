# src/app/ — `babaiwt` entry point

Single source file. Argument parsing only — all logic delegates to
`src/cli/`.

## Phase 1 CLI surface

```
babaiwt --test <path.test> [<path.test>...]
```

`--test` is currently a marker flag (silently consumed); positional
arguments are also accepted as paths. This is intentional so that
`make test` can invoke `babaiwt $(SCENARIOS)` without sprinkling the
flag everywhere.

Exit codes:

- `0` — all scenarios passed
- `1` — at least one scenario failed (or had a load error)
- `2` — bad arguments / no inputs

## What lives here later

Phase 2 will grow this entry point into a small subcommand dispatcher:

- `babaiwt test <files>` — current behavior, kept for CI.
- `babaiwt play <file.level>` — opens the raylib GUI.
- `babaiwt dump <file.level>` — round-trips through the loader and
  prints the canonical serialization (debugging tool).

Keep `main.cpp` to argument parsing + a tiny dispatch table — push real
work down into `cli/` or `gui/`.
