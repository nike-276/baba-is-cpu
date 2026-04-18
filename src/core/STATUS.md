# src/core/ — implementation status (rule engine)

Co-located mirror of [docs/feature-status.md](../../docs/feature-status.md).
This file is the one to update when you touch the rule engine; the docs
file is the authoritative master checklist.

## Properties recognized by `Kind`

| Property      | In `Kind` enum? | In `kTable` (`is_property`)? | Has tick-phase consumer? |
|---------------|-----------------|------------------------------|--------------------------|
| YOU           | yes             | yes                          | APPLY_INPUT              |
| PUSH          | yes             | yes                          | push-chain walker        |
| STOP          | yes             | yes                          | push-chain walker        |
| WIN           | yes             | yes                          | CHECK_WIN                |
| DEFEAT        | yes             | yes                          | DESTRUCT step c          |
| SINK          | yes             | yes                          | DESTRUCT step a          |
| HOT / MELT    | yes             | yes                          | DESTRUCT step b          |
| OPEN / SHUT   | yes             | yes                          | DESTRUCT step d          |
| MOVE          | **no**          | —                            | **planned** APPLY_AUTO_MOVE |
| AUTO          | **no**          | —                            | **planned** APPLY_AUTO_MOVE |
| FALL / FALLUP / FALLLEFT / FALLRIGHT | **no** | — | **planned** APPLY_AUTO_MOVE |
| UP / DOWN / LEFT / RIGHT | **no** | —                            | **planned** APPLY_DIRECTIONAL |
| WEAK          | **no**          | —                            | **planned** DESTRUCT step inserted before DEFEAT |
| EAT           | **no**          | —                            | **planned** DESTRUCT step inserted after SINK |
| MAKE          | **no**          | —                            | **planned** new MAKE phase |
| TEXT (predicate) | **no**       | —                            | **planned** TRANSFORM extension |
| SHIFT / PULL / SWAP | **no**    | —                            | **planned** push-chain hook |
| ON (operator) | **no**          | —                            | **planned** RuleSet rewrite (per-object) |

## Operators recognized

| Token | In `Kind` enum? |
|-------|-----------------|
| IS    | yes             |
| AND   | yes             |
| NOT   | yes (predicate-side only) |
| ON    | **no** — planned |
| NEAR / FACING / LONELY | deferred |

## Tick-phase pipeline

```
PARSE_INITIAL
  ↓
APPLY_DIRECTIONAL          ← planned (UP/DOWN/LEFT/RIGHT)
  ↓
APPLY_INPUT                ← done (push chain, STOP, multi-YOU by id)
  ↓
APPLY_AUTO_MOVE            ← planned (MOVE/AUTO/FALL*)
  ↓
PARSE_POST_MOVE
  ↓
TRANSFORM                  ← done (X IS Y, X IS X protection, duplication)
                              planned: X IS TEXT
  ↓
PARSE_POST_TRANSFORM
  ↓
DESTRUCT                   ← done (SINK, HOT/MELT, DEFEAT, OPEN/SHUT)
                              planned: EAT (after SINK), WEAK (before DEFEAT)
  ↓
MAKE                       ← planned (X IS MAKE Y → spawn Y on X-tiles)
  ↓
PARSE_POST_DESTRUCT
  ↓
CHECK_WIN
  ↓
COMMIT
```

## Architectural debts (must land before per-property work)

- `RuleSet::object_has_property` keys on `Kind` only → blocks `ON` and
  every other condition. Refactor to per-object derivation.
- `World` lacks `respawn(id, ...)` → blocks correct undo of Destroy
  (BUG-1, BUG-3 in feature-status §9).
- Tick pipeline has no extension points for the three new phases →
  refactor `apply_tick` to call a series of phase functions instead of
  inlined blocks.

## See also

- [../../docs/rule-engine-spec.md](../../docs/rule-engine-spec.md) —
  semantic source of truth.
- [../../docs/feature-status.md](../../docs/feature-status.md) —
  full Baba Is You catalog.
- [../../babaiswiki_pages_current.xml](../../babaiswiki_pages_current.xml)
  — wiki dump (canonical).
