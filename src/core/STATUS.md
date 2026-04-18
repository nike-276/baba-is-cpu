# src/core/ — implementation status (rule engine)

Co-located mirror of [docs/feature-status.md](../../docs/feature-status.md).
Update this file whenever you touch the rule engine; the docs file is the
authoritative master checklist.

## Properties recognized by `Kind`

| Property      | In `Kind` enum? | In `kTable`? | Has tick-phase consumer? |
|---------------|-----------------|--------------|--------------------------|
| YOU           | yes             | yes          | APPLY_INPUT              |
| PUSH          | yes             | yes          | push-chain walker        |
| STOP          | yes             | yes          | push-chain walker        |
| WIN           | yes             | yes          | CHECK_WIN                |
| DEFEAT        | yes             | yes          | DESTRUCT step e          |
| SINK          | yes             | yes          | DESTRUCT step a          |
| HOT / MELT    | yes             | yes          | DESTRUCT step c          |
| OPEN / SHUT   | yes             | yes          | try_move unlock + DESTRUCT step f |
| EAT           | yes             | yes          | DESTRUCT step b          |
| WEAK          | yes             | yes          | DESTRUCT step d          |
| MOVE          | yes             | yes          | APPLY_AUTO_MOVE (push + flip on block) |
| AUTO          | yes             | yes          | APPLY_AUTO_MOVE (push + no flip) |
| FALL / FALLUP / FALLLEFT / FALLRIGHT | yes | yes | APPLY_AUTO_MOVE (slide, no push) |
| LEFT / RIGHT / UP / DOWN | yes | yes     | APPLY_DIRECTIONAL (facing only) |
| MAKE          | O_Make operator | yes (operator) | APPLY_MAKE phase       |
| TEXT (predicate) | N_Text noun   | yes          | base rule TEXT IS PUSH; transform planned |
| SHIFT / PULL / SWAP | no      | —            | deferred                 |

## Operators recognized

| Token | In `Kind` enum? | Notes |
|-------|-----------------|-------|
| IS    | yes (O_Is)      | main predicate operator |
| AND   | yes (O_And)     | subject + predicate distribution |
| NOT   | yes (O_Not)     | predicate-side cancellation (DONE); subject-side planned |
| ON    | yes (O_On)      | conditional rule NOUN ON NOUN IS PROPERTY (DONE) |
| MAKE  | yes (O_Make)    | NOUN MAKE NOUN spawning (DONE) |
| NEAR / FACING / LONELY | deferred | |

## Tick-phase pipeline

```
PARSE_INITIAL
  ↓
APPLY_DIRECTIONAL          ← DONE (UP/DOWN/LEFT/RIGHT set facing)
  ↓
APPLY_INPUT                ← DONE (push chain, STOP, multi-YOU by id,
                                    OPEN unlocks SHUT+STOP)
  ↓
APPLY_AUTO_MOVE            ← DONE (MOVE/AUTO one-tile; FALL* slide-to-block)
  ↓
PARSE_POST_MOVE
  ↓
TRANSFORM                  ← DONE (X IS Y, X IS X protection, duplication)
                              PLANNED: X IS TEXT
  ↓
PARSE_POST_TRANSFORM
  ↓
DESTRUCT                   ← DONE (SINK, EAT, HOT/MELT, WEAK, DEFEAT, OPEN/SHUT)
  ↓
APPLY_MAKE                 ← DONE (NOUN MAKE NOUN spawns target, idempotent)
  ↓
PARSE_POST_DESTRUCT
  ↓
CHECK_WIN
  ↓
COMMIT
```

## Open architectural debts

- `World::respawn(id, ...)` exists but undo of Destroy in the editor
  uses a fresh id (BUG-1, BUG-3 in feature-status §9). Fix is to call
  `respawn` instead of `spawn` in `sim/simulator.cpp` step_back and
  `editor/editor.cpp` undo_edit.
- NOT subject-side (`NOT X IS P` → applies P to everything except X)
  is parsed but not resolved.
- X IS TEXT predicate transform is unimplemented.

## See also

- [../../docs/rule-engine-spec.md](../../docs/rule-engine-spec.md) — semantic source of truth.
- [../../docs/feature-status.md](../../docs/feature-status.md) — full Baba Is You catalog.
- [../../babaiswiki_pages_current.xml](../../babaiswiki_pages_current.xml) — wiki dump (canonical).
