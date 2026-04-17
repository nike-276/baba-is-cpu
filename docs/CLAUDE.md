# docs/ — v1 specs

These three files are the source of truth. Code conforms to them, not
the other way around. If a spec is wrong, fix the spec in the same
commit as the code change and call it out in the message.

## Files

- **`architecture.md`** — module layout, the 9-phase tick model, what's
  in scope for Phase 1 vs deferred to later phases, determinism rules.
- **`rule-engine-spec.md`** — the rule grammar (`NOUN [AND NOUN]* IS
  PROPERTY [AND PROPERTY]*`), the property catalog (YOU, PUSH, STOP,
  WIN, DEFEAT, SINK, HOT, MELT, OPEN, SHUT), the base rule
  `TEXT IS PUSH`, and edge cases like NOT and contradiction handling.
- **`file-format-v1.md`** — the `.level` and `.test` text formats
  (header records, body records, `[setup]` / `[inputs]` / `[expected]`
  sections, tokenizer rules, error semantics).

## Phase scope

Phase 1 (current, GREEN):
- The PARSE → APPLY_INPUT → re-PARSE → CHECK_WIN slice of the tick
  pipeline.
- YOU, PUSH, STOP, WIN.
- `.level` + `.test` loaders, scenario runner.

Deferred to later phases (specs already cover them — implementation
will catch up):
- DEFEAT / SINK / HOT+MELT / SHUT+OPEN, transformation rules, NOT
  modifiers.
- Undo stack (snapshots between ticks).
- Raylib GUI + sprite atlas (Phase 2).

## When you change a spec

1. Update the markdown.
2. If the change touches engine behavior, update or add the
   corresponding `.test` scenario.
3. Update or add code so the new scenario goes RED → GREEN.
4. Commit message should mention all three.

## Cross-references

- `babaiswiki_pages_current.xml` (in repo root) is the dump of the
  Baba Is You wiki at the time we started; the canonical source for
  vanilla rule semantics. Use it before guessing.
- `architecture.md §5` is the determinism contract — read it before
  iterating any container that came out of an `unordered_map`.
