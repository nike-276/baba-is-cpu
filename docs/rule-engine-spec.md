# Rule Engine Specification — v1 (MVP)

This document defines the exact semantics our simulator must implement. It is the
source of truth for the test harness. When wiki behavior and this spec disagree,
this spec wins (we deviate intentionally where the wiki documents engine quirks
that aren't worth replicating). Deviations are marked
**[DEVIATION]**.

References: `babaiswiki_pages_current.xml` — pages *Order of Operations*, *Rule*,
*IS*, *NOT*, *AND*, *Category:Removal Methods*.

---

## 1. Scope

### 1.1 In-scope (v1 / MVP)

**Operators**: `IS`, `NOT`, `AND`

**Nouns**: object kinds present in the level (e.g. `BABA`, `WALL`, `ROCK`,
`FLAG`, `WATER`, `LAVA`, `KEY`, `DOOR`, `BOX`). Plus reserved `TEXT`, `EMPTY`,
`ALL`.

**Properties**: `YOU`, `PUSH`, `STOP`, `WIN`, `DEFEAT`, `SINK`, `HOT`, `MELT`,
`OPEN`, `SHUT`.

**Base rules** (always active, not parseable):
- `TEXT IS PUSH`

### 1.2 Planned (Phase 2 / next pass)

The following are **scoped for the upcoming implementation pass**. They
must fit the v1 architecture without rework — see
[feature-status.md §8](feature-status.md#8-engine-architecture-notes-gating-implementation)
for the structural changes that gate them.

- Movement properties: `MOVE`, `AUTO`, `FALL` / `FALLUP` / `FALLLEFT` /
  `FALLRIGHT`, directional `UP` / `DOWN` / `LEFT` / `RIGHT`,
  `SHIFT`, `PULL`, `SWAP`.
- Removal properties: `WEAK`, `EAT`.
- Spawn properties: `MAKE` (new tick phase post-DESTRUCT).
- Transform properties: `TEXT` as a predicate (`X IS TEXT` swaps the
  object's text-twin in place).
- Conditional operator: `ON` (`X ON Y IS P`). Forces per-object property
  derivation in the resolver.
- Subject-side `NOT` (`NOT X IS P`).

### 1.3 Deferred (post-Phase 2)

`CHILL`, `FEAR`, `NUDGE*`, `BOOM`, `HAS`, `SAFE`, `FLOAT`, `PHANTOM`,
`HOLD`, `SELECT`, `REVERT`, `WRITE`, `MIMIC`, `LOCKED*`, `MORE`, `DONE`,
`PLAY`, `BONUS`, `END`, `BACK`, `TELE`, `FOLLOW`, `YOU2`, `3D`,
conditions other than `ON` (`NEAR`, `FACING`, `LONELY`, `POWERED`, …),
`LEVEL` semantics, stack limits (6), `TOO COMPLEX` / `INFINITE LOOP`
overflow.

These can all be added incrementally without changing the architecture
if we follow the phased structure below. The full catalog with status
flags lives in [feature-status.md](feature-status.md).

---

## 2. Object model

```
Object {
  id        : uint32   // permanent, never reused; used for object-priority order
  x, y      : int32    // sparse coordinates
  kind      : ObjectKind
  facing    : Direction { Right, Up, Left, Down }
}
```

**Layering**: implicit. Within a single tile, objects with `kind` in the `Text`
category render above non-text. Within the same category, render in insertion
order. No explicit z-field.

**Coexistence**: multiple objects can share a tile. The default `can_coexist`
rule for v1 schematic stamping:
- Two text objects on the same tile: **never allowed** (forbidden by stamping).
- Anything else: allowed (a `BOX` can sit on `WATER`, etc.).

This is a stamping rule only. The simulator itself does not police coexistence;
it lets interactions resolve in the block phase.

---

## 3. Rule parsing

Rules are parsed *fresh* every time the parse phase fires. They are derived,
never persisted, never undoable.

### 3.1 Grammar (v1)

```
rule       := subject IS predicate
subject    := nounphrase
predicate  := propertyphrase | nounphrase
nounphrase := [NOT] noun (AND [NOT] noun)*
propertyphrase := [NOT] property (AND [NOT] property)*
noun       := <object kind> | TEXT | EMPTY | ALL
property   := YOU | PUSH | STOP | WIN | DEFEAT | SINK
            | HOT | MELT | OPEN | SHUT
```

### 3.2 Parsing procedure

For every horizontal row and vertical column on the grid:
1. Walk the line collecting contiguous *text objects* (objects whose `kind` is
   any `TEXT_*` variant).
2. Treat consecutive text objects as a token stream. A blank cell terminates the
   stream; the next token stream starts after the blank.
3. Try to match the grammar greedily, longest-first. Each successful match
   produces one rule. Submatches that are themselves complete rules also produce
   rules. Example: `BABA AND KEKE IS PUSH AND STOP` produces 4 rules
   (`{BABA,KEKE}` × `{PUSH,STOP}`).
4. Both row and column scans run independently. The same text cell may
   participate in a row rule *and* a column rule simultaneously.

**[DEVIATION]** v1 does not implement stacked-text-on-one-cell parsing. A tile
holds at most one text object (enforced by editor + level format).

### 3.3 Rule resolution

After parsing, the rule list is processed into a **derived property table**:

```
derived[(object_id)] : set<Property>
transforms[(source_kind)] : list<target_kind>
```

Resolution algorithm:

1. **NOT cancellation**. Two passes:
   - First, collapse double negations (`NOT NOT X` → `X`).
   - For each property `P`, if both `X IS P` and `X IS NOT P` exist, neither
     applies for that tick. (Wiki: "ROCK IS NOT PUSH disables ROCK IS PUSH".)
   - For each transformation `X IS Y`, if `X IS NOT Y` also exists, the
     transformation is suppressed.

2. **NOT-as-everything-except**. `NOT X IS P` (with X a single noun) applies P
   to every kind in the level except X. `NOT X IS NOT P` applies "not P" to
   every kind except X. Composition with `AND` follows distribution.

3. **`X IS X` protection**. If `X IS X` is in the rule list, all `X IS Y`
   transformations (where Y is a noun ≠ X) are suppressed for X. `X IS X` does
   *not* affect `Y IS X` (transforming things into X is still allowed).

4. **Property assignment**. For each rule `X IS P` (P is a property), every
   live object with `kind == X` (or in the implicit set if X is `ALL`/`NOT Y`)
   gets `P` added to its derived property set.

5. **Implicit base rules**. After resolution, every object with a text kind
   (`TEXT_*`) gets `PUSH` added unconditionally — *unless* a rule containing
   `TEXT IS NOT PUSH` (or `NOT X IS PUSH` etc. resolving the same way) was
   generated. (Wiki: base `TEXT IS PUSH` is overridable by `NOT`.)

6. **Self-destruction `X IS NOT X` / `X IS EMPTY`**. Marks all instances of X
   for destruction in the destructions phase.

7. **Transformation dictionary**. For each surviving rule `X IS Y`, build
   `transforms[X] += Y`. If `transforms[X]` contains multiple targets, every
   X-instance is replaced with one instance of *each* target (Wiki:
   `BABA IS KEKE AND ME` duplicates).

### 3.4 Determinism

All iteration over rules, objects, and properties uses **object-id order**
(ascending). All set/dict iteration in the engine must be ordered, not
hash-iteration. Property sets are stored as sorted vectors for v1; can become
bitsets later.

---

## 4. Tick phases (v1)

A single tick consists of these phases, executed in order. The wiki's full
order has ~12 phases; v1 collapses those covering deferred properties.

```
1. PARSE_INITIAL          — rebuild rules + derived properties
2. APPLY_INPUT            — move every YOU object once in input direction
                             (resolves push chains, STOP, OPEN/SHUT collision)
3. PARSE_POST_MOVE
4. TRANSFORM              — apply X IS Y transformations
                             (X IS X protection enforced; duplications applied)
5. PARSE_POST_TRANSFORM
6. DESTRUCT               — apply removal effects in precedence order (§5)
7. PARSE_POST_DESTRUCT
8. CHECK_WIN              — any YOU sharing a tile with WIN → win
9. COMMIT                 — finalize undo record for the tick
```

If `APPLY_INPUT` produces zero changes (e.g., YOU is blocked), phases 3–7 still
run because rules might have changed for other reasons (none in v1 — but the
phase order is invariant for forward compatibility).

### 4.1 Movement (APPLY_INPUT)

Iterate every YOU object in **object-id order**. For each:

1. Compute target tile = current + input direction.
2. Walk the *push chain* starting at the target:
   - For each cell in chain, collect all PUSH objects.
   - If the next cell contains a STOP (and not coexistent PUSH that is also
     STOP — STOP wins), the chain is blocked → no movement for this YOU.
   - If the next cell is empty (after collecting PUSH), chain succeeds.
   - **[DEVIATION]** v1 has no level border. The world is infinite; there is no
     edge. STOP is the only blocker.
3. If chain succeeded, move all PUSH objects in the chain by one tile in the
   input direction, then move the YOU object. Update `facing` to input
   direction regardless of whether movement succeeded.
4. Each YOU's chain is computed against the world state *after* prior YOUs in
   this tick have moved. (Sequential, deterministic by id order.)

Movement also triggers **per-step OPEN/SHUT collision** as the wiki documents,
but in v1 we collapse this into the single DESTRUCT phase. **[DEVIATION]**

### 4.2 Transformations (TRANSFORM)

For each object whose `transforms[kind]` is non-empty:
- If `transforms[kind] = [Y]`: change kind to Y in place. Same id, same
  position, same facing.
- If `transforms[kind] = [Y1, Y2, ...]`: replace with N new objects, one per
  target, all at the original position. The original object is destroyed; new
  objects get fresh ids assigned in iteration order.
- If `X IS NOT X` (or `X IS EMPTY` in future): destroy.

Transformations within a tick all apply *simultaneously* against the
pre-transform state (compute the full transform map first, then apply). This
avoids ordering bugs when chains like `BABA IS KEKE`, `KEKE IS BABA` exist.

### 4.3 Destructions (DESTRUCT)

Iterate **all coexisting pairs at every occupied tile** and apply removals in
the following precedence (matches wiki block() order, restricted to v1
properties):

```
a. SINK    — if any two objects share a tile and at least one has SINK,
              both are destroyed.
b. HOT/MELT — if a tile contains both a HOT object and a MELT object,
              the MELT object is destroyed; HOT survives.
c. DEFEAT   — if a tile contains a YOU object and a DEFEAT object,
              the YOU object is destroyed; DEFEAT survives.
d. OPEN/SHUT — if a tile contains an OPEN object and a SHUT object,
              both are destroyed.
```

Within each step, all destructions resolve *simultaneously* (gather first, then
apply). Between steps, the world updates.

**Notes**:
- A single object may have multiple of these properties. SINK on a YOU+SINK on
  the same tile destroys both. (Mirrors wiki.)
- Properties are re-derived only at PARSE_POST_DESTRUCT, not within DESTRUCT.

### 4.4 Win check (CHECK_WIN)

If any tile contains both a YOU and a WIN object, set `win = true` for the tick.
The simulator freezes input but the level is not destroyed. Undo restores.

**[DEVIATION]** Wiki: "WIN deletes the level." We do not — the user wants to
inspect post-win state.

### 4.5 Commit (COMMIT)

Collect all `Change` records produced during phases 2–7 into one tick record
and push to the undo stack. If zero records, do not push. (Wiki-consistent.)

---

## 5. Push chain resolution — exact algorithm

```
fn try_move(world, you_id, dir):
    pos = world.pos(you_id)
    chain = []                    # list of object ids to move (excl. YOU)
    cur = pos + dir
    loop:
        cell = world.at(cur)
        cell_pushable = [id for id in cell if has_push(id)]
        cell_blocking = [id for id in cell if has_stop(id) and id not in cell_pushable]
        if cell_blocking:
            return MOVE_BLOCKED
        if not cell_pushable:
            break                 # empty (of pushables/stops); chain succeeds
        chain.extend(cell_pushable)
        cur += dir

    # Apply: move chain in reverse, then YOU.
    for id in reversed(chain):
        world.move(id, world.pos(id) + dir)
    world.move(you_id, pos + dir)
    return MOVE_OK
```

A single object that is both PUSH and STOP behaves as PUSH (it can be pushed
but doesn't itself block — wiki-consistent: "PUSH" is a movability property).
**[DEVIATION-RISK]** Verify this against wiki PUSH page in a later pass.

---

## 6. Multiple YOU resolution

YOU objects are processed in ascending **object-id order**. The id corresponds
to insertion order: on level load, ids are assigned column-major top-to-bottom;
on spawn, ids are assigned in append order.

**[DEVIATION]** On undo of a destruction, the destroyed object is
re-inserted with its **original id**, not a fresh one. The wiki (and
the original v1 of this spec) called for a fresh id, but that breaks
chained reverse-application of `Move` records that target the same id
within the same tick (e.g., a tick that moves an object and then
destroys it — the inverse is "respawn, then unmove", which fails if
the respawn allocates a different id). The user has explicitly chosen
this deviation. Implementation: `World::respawn(id, pos, kind, text,
facing)` is the only way to reuse an id; `spawn` always allocates a
fresh one.

---

## 7. Determinism guarantees

- No RNG in v1.
- All hash maps must iterate in deterministic order. Use sorted vectors for
  derived structures, or `std::map` where iteration is needed.
- Object-id order is the canonical tiebreaker everywhere.
- Floating-point is not used in the simulator.

---

## 8. Test scenarios required

Phase-3 (rule engine) test set must include:

1. `BABA IS YOU`, single YOU, push chain through one ROCK
2. `BABA IS YOU AND PUSH` — YOU+PUSH on same object
3. `WALL IS STOP` blocks YOU
4. Two YOU objects move in same direction, second's chain depends on first
5. `BABA IS NOT YOU` when `BABA IS YOU` also present — neither applies
6. `BABA IS BABA` protects against `BABA IS WALL`
7. `BABA IS KEKE AND ME` duplicates
8. `BABA IS NOT BABA` destroys BABA
9. `NOT BABA IS PUSH` makes everything except BABA pushable
10. SINK destroys both objects on contact
11. HOT melts MELT but not YOU
12. DEFEAT kills YOU
13. OPEN+SHUT destroys both
14. Multiple destructions in one tick: SINK applies before DEFEAT
15. WIN check: BABA on FLAG with `BABA IS YOU` + `FLAG IS WIN` → win
16. Undo restores pre-tick state for every phase
17. No-op tick (YOU blocked) does not push to undo stack
18. Transformation chain: `BABA IS KEKE`, `KEKE IS WALL` simultaneously —
    both apply to pre-transform state, BABA → KEKE and existing KEKE → WALL.

These map 1:1 to test fixture files in `tests/fixtures/`.

---

## 9. Open questions

- **OPEN/SHUT during movement**: wiki distinguishes "collision check on
  movement" vs "overlap check after". v1 collapses to overlap-only. Revisit if need door logic during a single tick.
- **`ALL` semantics**: wiki says `ALL` excludes `TEXT`/`EMPTY`/`LEVEL`. v1
  treats `ALL` as "every non-text kind in the level".
- **Letter words / WORD objects**: deferred entirely.
- **Stacked text on one tile**: deferred.

These are deliberately out of v1. They are listed so the test harness can
xfail-mark scenarios involving them.
