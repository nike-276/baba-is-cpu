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
| TEXT (predicate) | N_Text noun   | yes          | base rule TEXT IS PUSH; `X IS TEXT` → `do_flip_text` (non-text→text, coexistence guard) |
| POWER               | yes (P_Power)  | yes (property) | `any_has_power_kind(P_Power)` in RuleSet; enables POWERED |
| POWER2 / POWER3     | yes (P_Power2/3) | yes          | independent channels; enable POWERED2/POWERED3 |
| NUDGERIGHT / NUDGEUP / NUDGELEFT / NUDGEDOWN | yes (P_Nudge*) | yes | APPLY_NUDGE phase 2.53; try_move without facing change |
| SHIFT / PULL / SWAP | no      | —            | deferred                 |

## Operators recognized

| Token | In `Kind` enum? | Notes |
|-------|-----------------|-------|
| IS    | yes (O_Is)      | main predicate operator |
| AND   | yes (O_And)     | subject + predicate distribution (subjects, properties, nouns, MAKE/EAT targets) |
| NOT   | yes (O_Not)     | predicate-side cancellation (DONE); subject-side planned |
| ON    | yes (O_On)      | `NOUN ON NOUN [AND NOUN]* IS/MAKE P/NOUN` — compound condition (all must be present). NOT ON variant also DONE. |
| MAKE  | yes (O_Make)    | Unconditional `NOUN MAKE NOUN [AND NOUN]*` + conditional `NOUN ON/NOT ON … MAKE NOUN` (DONE) |
| EAT   | yes (O_Eat)     | NOUN EAT NOUN [AND NOUN]* destruction (DONE) |
| HAS   | yes (O_Has)     | NOUN HAS NOUN [AND NOUN]*: spawn target when subject destroyed via DESTRUCT (DONE). Phase 6.1 `apply_has()`. |
| POWERED (O_Powered) | yes (O_Powered) | Global prefix condition: `[NOT] POWERED NOUN IS PROPERTY`. Stored as `GlobalConditionPropertyRule{conditions}`; evaluated via `any_has_power_kind(P_Power)`. |
| POWERED2 / POWERED3 | yes (O_Powered2/3) | Independent variants; `[NOT] POWERED2 NOUN IS PROPERTY`. AND-chains of POWERED/POWERED2/POWERED3 require ALL channels active. |
| FOLLOW (O_Follow) | yes (O_Follow) | `NOUN FOLLOW NOUN [AND NOUN]*`: move toward nearest non-colocated target (Manhattan, vertical tie-break). Phase 3.5 `apply_follow()`. |
| FEAR (O_Fear) | yes (O_Fear) | `NOUN FEAR NOUN [AND NOUN]*`: move away from adjacent target; priority fwd→CW→CCW→bwd relative to facing. Phase 2.55 `apply_fear()`. |
| PLAY (O_Play) | yes (O_Play) | `NOUN PLAY NOTE [OCTAVE] [ACCIDENTAL]`: each non-text subject emits one `SoundEvent` per tick (phase 7.5 `apply_play()`). NOTE = O_LetterA…O_LetterG; optional OCTAVE = O_Num0…O_Num9 (default 5); optional ACCIDENTAL = O_Sharp / O_Flat. Modifiers in any order. |
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
APPLY_NUDGE                ← DONE (NUDGERIGHT/UP/LEFT/DOWN; R→U→L→D sub-passes)
  ↓
APPLY_FEAR                 ← DONE (NOUN FEAR NOUN; move away from adjacent)
  ↓
PARSE_POST_MOVE
  ↓
APPLY_FOLLOW               ← DONE (NOUN FOLLOW NOUN; move toward nearest)
  ↓
TRANSFORM                  ← DONE (X IS Y, X IS X protection, duplication, X IS TEXT)
  ↓
PARSE_POST_TRANSFORM
  ↓
DESTRUCT                   ← DONE (SINK, EAT, HOT/MELT, WEAK, DEFEAT, OPEN/SHUT)
  ↓
APPLY_MAKE                 ← DONE (NOUN MAKE NOUN spawns target, idempotent)
  ↓
PARSE_POST_DESTRUCT
  ↓
APPLY_PLAY                 ← DONE (NOUN PLAY NOTE: emits SoundEvent per matching object)
  ↓
CHECK_WIN
  ↓
COMMIT
```

## Open architectural debts
- NOT subject-side (`NOT X IS P` → applies P to everything except X)
  is parsed but not resolved.

## See also

- [../../docs/rule-engine-spec.md](../../docs/rule-engine-spec.md) — semantic source of truth.
- [../../docs/feature-status.md](../../docs/feature-status.md) — full Baba Is You catalog.
- [../../babaiswiki_pages_current.xml](../../babaiswiki_pages_current.xml) — wiki dump (canonical).
