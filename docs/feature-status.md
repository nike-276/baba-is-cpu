# Feature Status — Baba Is You simulator

Master checklist of every Baba Is You construct, indexed against the wiki
(`babaiswiki_pages_current.xml` in repo root). Status flags:

- **DONE** — implemented + scenario-tested
- **PLANNED** — scoped for the next implementation pass
- **DEFERRED** — engine architecture must accommodate it but no
  implementation work is committed yet
- **N/A** — intentionally out of scope (see [architecture §10](architecture.md#10-out-of-scope-for-v1))

When you change status, also update `src/core/STATUS.md` (the co-located
checklist) and any scenario file that locks the behavior down.

---

## 1. Operators

| Token | Status | Notes |
|-------|--------|-------|
| `IS`     | DONE     | Property + transform + identity (`X IS X`). |
| `AND`    | DONE     | Subject and predicate distribution. |
| `NOT`    | DONE (predicate side) | Cancels positive of same property; negative-only is a no-op. **PLANNED**: subject side (`NOT X IS P`). |
| `ON`     | DONE     | `X ON Y IS P`: X has P only when sharing a tile with a non-text Y. Checked per-object in `object_has_property`. |
| `NEAR`   | DEFERRED | Same shape as `ON` but uses 8-neighborhood. |
| `FACING` | DEFERRED | Subject must be facing a tile containing the noun. |
| `LONELY` | DEFERRED | Subject must be the only object on its tile (or in 4-neighborhood per wiki variant). |
| `WITHOUT`| DEFERRED | Negation of `NEAR`. |
| `HAS`    | DEFERRED | Pairs with `MAKE`/`HAS` semantics for spawning on destruction. |

## 2. Nouns

| Token | Status | Notes |
|-------|--------|-------|
| Object kinds present in palette (`BABA`, `WALL`, `ROCK`, `FLAG`, `WATER`, `LAVA`, `SKULL`, `KEY`, `DOOR`) | DONE | See `src/core/kind.cpp` `kTable`. |
| `KEKE`, `ME`              | PLANNED | Required for transform/duplicate scenarios. |
| `TEXT` (meta-noun)        | PARTIAL | Recognized in rule grammar (base rule `TEXT IS PUSH`). PLANNED: as predicate (`X IS TEXT` transforms X into its text twin). |
| `EMPTY`                   | PLANNED | As subject = "empty tiles"; as predicate = self-destruct. Currently neither is wired. |
| `ALL`                     | DEFERRED | Distribution-over-everything semantics. |
| `LEVEL`, `IMAGE`, `CURSOR`| N/A      | Out of scope for v1. |
| Letter / WORD objects     | N/A      | Deferred entirely (see spec §9). |

## 3. Properties — movement

| Property | Status | Notes |
|----------|--------|-------|
| `YOU`        | DONE     | Multi-YOU resolved by ascending id. |
| `PUSH`       | DONE     | Chain walks forward, applies back-to-front. |
| `STOP`       | DONE     | Blocks the entire chain. |
| `MOVE`       | DONE     | Self-propelled; reverses facing on block. Phase APPLY_AUTO_MOVE (phase 2.5) runs after APPLY_INPUT using PARSE_INITIAL rules. |
| `AUTO`       | DONE     | Self-propelled but stays put on block (no flip). Same phase as MOVE. |
| `FALL`       | DONE     | Slides downward until blocked in one tick; does NOT push (stops at PUSH or STOP objects). |
| `FALLUP` / `FALLLEFT` / `FALLRIGHT` | DONE | Same slide semantics as FALL in respective directions. |
| `UP` / `DOWN` / `LEFT` / `RIGHT` | DONE | Sets object's facing each tick (APPLY_DIRECTIONAL phase 1.5, before APPLY_INPUT). No movement by themselves; combine with MOVE/AUTO for motion. |
| `SHIFT`      | PLANNED  | Carries any object that ends a tick on top of it one tile in SHIFT's facing direction. |
| `PULL`       | PLANNED  | Mirror of PUSH on the back of the chain. |
| `SWAP`       | PLANNED  | Two SWAP-property objects on neighbouring tiles trade places when one moves. |
| `FLOAT`      | DEFERRED | Layered overlap; FLOAT objects only interact with other FLOAT objects on a tile. |
| `TELE`       | DEFERRED | Teleports overlapping objects to another TELE-tagged tile. |
| `FOLLOW`     | DEFERRED | Moves in the same direction as YOU after YOU moves. |

## 4. Properties — interactions / removal

Removal precedence inside DESTRUCT (lower = earlier):

```
1. SINK
2. EAT      ← PLANNED (inserted between SINK and HOT)
3. HOT/MELT
4. WEAK     ← PLANNED (any movement of WEAK + collision destroys WEAK)
5. DEFEAT
6. OPEN/SHUT
```

| Property | Status | Notes |
|----------|--------|-------|
| `SINK`   | DONE     | Any two objects sharing a tile (one with SINK) → both destroyed. |
| `HOT`    | DONE     | Survives MELT. |
| `MELT`   | DONE     | Destroyed by HOT. |
| `DEFEAT` | DONE     | Destroys YOU on tile. |
| `OPEN` / `SHUT` | DONE | Mutual destruction. |
| `EAT`    | DONE     | EAT object on tile with any non-EAT, non-text object → other object destroyed. EAT itself survives. DESTRUCT sub-step (b). |
| `WEAK`   | DONE     | Destroyed when any object arrives on its tile this tick (determined from Move changes in the log). DESTRUCT sub-step (d). |
| `MAKE`   | DONE     | `X MAKE Y` (MAKE is an operator, not a property). Phase APPLY_MAKE post-DESTRUCT: spawns a Y on every tile containing X. Idempotent (skips if Y already present). |
| `HAS`    | DEFERRED | `X HAS Y` spawns Y when X is destroyed; needs to hook DESTRUCT. |
| `BOOM`   | DEFERRED | Destroys self + neighbouring tiles' contents. |
| `SAFE`   | DEFERRED | Immune to DEFEAT/SINK/MELT/etc. |
| `PHANTOM`| DEFERRED | Skips collision checks entirely. |
| `HOLD`   | DEFERRED | YOU+HOLD object can be carried. |
| `FEAR`   | DEFERRED | YOU adjacent to FEAR-tagged object cannot move toward it. |
| `CHILL`  | DEFERRED | Random subset of MOVE behavior; needs RNG → likely permanent N/A. |

## 5. Properties — transforms

| Property | Status | Notes |
|----------|--------|-------|
| `TEXT` (as predicate) | PLANNED | `X IS TEXT` swaps every X-instance with its text twin (kind unchanged, `text` flag flipped). |
| `WORD` | DEFERRED | Variant treating a non-text object as a noun for parsing. |
| `MIMIC`| DEFERRED | Subject takes on properties of overlapped object. |
| `WRITE`| DEFERRED | Cosmetic; pulls letters together. |

## 6. Win / loss / level

| Property | Status | Notes |
|----------|--------|-------|
| `WIN`    | DONE     | Any YOU+WIN tile sets `won=true`. Level not destroyed (DEVIATION from wiki). |
| `DEFEAT` | DONE     | See §4. |
| `END`    | DEFERRED | Same as WIN but ends the game (level browser). |
| `DONE`   | DEFERRED | Persists win flag to save data. |
| `MORE`   | DEFERRED | Spawns more of subject on idle ticks. |
| `BACK`   | DEFERRED | Returns to level select. |
| `BONUS`  | DEFERRED | Bonus objective marker. |
| `PLAY`   | DEFERRED | Re-enters a level. |
| `SELECT` | DEFERRED | Cursor target on level map. |
| `REVERT` | DEFERRED | Auto-undo on overlap. |

## 7. Misc semantics

| Construct | Status | Notes |
|-----------|--------|-------|
| Base rule `TEXT IS PUSH`               | DONE | Always injected; cancelled by `TEXT IS NOT PUSH`. |
| `X IS X` protection                    | DONE | Suppresses transforms `X IS Y` when `X IS X` is active. |
| `X IS NOT X` / `X IS EMPTY` self-destruct | PLANNED | Should resolve in DESTRUCT. |
| Stack limit (6 text per tile)          | N/A  | We forbid >1 text per tile entirely. |
| `TOO COMPLEX` / `INFINITE LOOP`        | N/A  | We allow arbitrary chains. |
| Stacked-text-on-tile parsing           | N/A  | Out of scope. |
| Conditions composition (`ON Y AND Z`)  | DEFERRED | Comes with the per-object property refactor. |

---

## 8. Engine-architecture notes (gating implementation)

These structural changes are required **before** the per-property work
to keep the engine general:

1. **Per-object property derivation.** `RuleSet::object_has_property`
   currently keys on kind alone. Conditional rules (`ON`, `NEAR`,
   `FACING`, `LONELY`) need the object's tile context. Refactor to
   `object_has_property(World, ObjectId, Property)` and let the
   resolver build a `unordered_map<ObjectId, PropertySet>` lazily.

2. **Tick-phase plug-in points.** The 9-phase pipeline gets three new
   phases:
   - `APPLY_DIRECTIONAL` (UP/DOWN/LEFT/RIGHT) — between PARSE_INITIAL
     and APPLY_INPUT.
   - `APPLY_AUTO_MOVE` (MOVE/AUTO/FALL\*) — after APPLY_INPUT, before
     PARSE_POST_MOVE.
   - `MAKE` — after DESTRUCT, before PARSE_POST_DESTRUCT.
   Existing DESTRUCT gains EAT and WEAK steps in the precedence table.

3. **Push-chain extension hooks.** PULL, SHIFT, and SWAP all bolt onto
   the existing chain walker. Make `try_move` accept policy callbacks
   for "what to do with the back of the chain" and "what to do when
   the chain succeeds" so each property is one short callback.

4. **id-preserving respawn (undo fix).** `World::respawn(id, ...)` so
   the inverse of Destroy resurrects the original object (DEVIATION
   from spec §6 — chosen because chained Move replays against the same
   id break otherwise).

---

## 9. Known bugs (open)

| ID | Where | Symptom | Status |
|----|-------|---------|--------|
| BUG-1 | `src/sim/simulator.cpp:39-40`, `src/editor/editor.cpp:80-81` | Undo of Destroy spawns a fresh id; subsequent reverse-applied Move records target a now-missing id and the resurrected object lands in the wrong place. | PLANNED fix in Phase B (id-preserving `respawn`). |
| BUG-2 | ~~`src/app/input_handler.hpp`~~ | ~~`scroll_x/y` is `int`, accumulating ½-tile rounding errors on zoom~~ | **FIXED** — `scroll_x/y` changed to `float`; zoom math is now exact; grid offset uses fractional part. |
| BUG-3 | `src/editor/editor.cpp:81` | Editor's edit-undo also loses original ids (same root cause as BUG-1). | Same fix as BUG-1. |

Add new bugs above this line with the next free ID and the same shape.
