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
| `AND`    | DONE     | Subject and predicate distribution (subjects, properties, nouns, MAKE/EAT targets). |
| `NOT`    | DONE (predicate side) | Cancels positive of same property; negative-only is a no-op. **PLANNED**: subject side (`NOT X IS P`). |
| `ON`     | DONE     | `X ON Y [AND …]* IS P/NOUN`: compound condition — all listed nouns must be co-located. Also `X ON Y [AND …]* MAKE NOUN`. AND chains may mix `AND NOT ON Z` terms (forbidden nouns). Checked per-object in `object_has_property` / TRANSFORM / MAKE phases. |
| `NOT ON` | DONE     | `X NOT ON Y [AND …]* IS P/NOUN/MAKE NOUN`: each listed noun must be independently absent. AND chains may mix `AND ON Z` terms (required nouns). Condition met when all `condition_nouns` present AND all `forbidden_nouns` absent. |
| `MAKE`   | DONE     | `X MAKE Y [AND Z]*`: unconditional. `X ON … MAKE Y` / `X NOT ON … MAKE Y`: conditional. Spawns targets post-DESTRUCT. Idempotent. |
| `EAT`    | DONE     | `X EAT Y [AND Z]*`: operator (not a property). Subject destroys listed targets on contact in DESTRUCT. |
| `NEAR`   | DEFERRED | Same shape as `ON` but uses 8-neighborhood. |
| `FACING` | DEFERRED | Subject must be facing a tile containing the noun. |
| `LONELY` | DEFERRED | Subject must be the only object on its tile (or in 4-neighborhood per wiki variant). |
| `WITHOUT`| DEFERRED | Negation of `NEAR`. |
| `HAS`    | DONE     | `X HAS Y [AND Z]*`: when X is destroyed via any DESTRUCT sub-step (SINK, EAT, HOT/MELT, WEAK, DEFEAT, OPEN/SHUT), spawns Y (and Z…) at X's tile with X's facing. Does NOT trigger on transforms. Stored as `HasRule`; processed in `apply_has()` phase 6.1. AND chains supported. `X HAS X` (self-respawn) supported. |
| `POWERED`| DONE     | Prefix condition: `[NOT] POWERED NOUN IS PROPERTY`. True when any live non-text object has `P_Power` (via unconditional or ON-conditional rule). Parsed as `O_Powered` operator token. Stored as `GlobalConditionPropertyRule`; evaluated in `object_has_property` via `any_has_power()`. |
| `FOLLOW` | DONE     | `X FOLLOW Y [AND Z]*`: X moves one tile toward the nearest non-colocated Y (or Z…) each tick. Nearest by Manhattan distance; ties prefer vertical. Operator `O_Follow`; stored as `FollowRule`; processed in `apply_follow()` phase 3.5 (after PARSE_POST_MOVE). |
| `FEAR`   | DONE     | `X FEAR Y [AND Z]*`: X moves away from any 4-directionally adjacent Y (or Z…) each tick. Direction priority relative to X's facing: forward→CW→CCW→backward; skips feared directions. Operator `O_Fear`; stored as `FearRule`; processed in `apply_fear()` phase 2.55. |

## 2. Nouns

| Token | Status | Notes |
|-------|--------|-------|
| `BABA`, `WALL`, `ROCK`, `FLAG`, `WATER`, `LAVA`, `SKULL`, `KEY`, `DOOR` | DONE | Original palette objects. |
| `KEKE`, `FOFO`, `ME`, `BOX`, `LEAF`, `CLOUD`, `SUN`, `MOON`, `STAR`, `PLANET`, `BOLT`, `LOVE`, `BOMB`, `WIND` | DONE | Added as full noun kinds with object tiles + text tiles + palette entries. |
| `TRACK`, `BELT` | DONE | Added as full noun kinds with object tiles + text tiles + palette entries. |
| `TEXT` (meta-noun)        | DONE | Recognized in rule grammar (base rule `TEXT IS PUSH`). `X IS TEXT` converts non-text X objects into text tiles of the same kind. Coexistence guard prevents double-text on same cell. |
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
| `NUDGERIGHT` / `NUDGEUP` / `NUDGELEFT` / `NUDGEDOWN` | DONE | Moves the object 1 tile in the named direction each tick without changing facing. Uses `try_move` (can push PUSH objects). STILL blocks it. Sub-passes run R→U→L→D in `apply_nudge()` phase 2.53. |
| `FLOAT`      | DEFERRED | Layered overlap; FLOAT objects only interact with other FLOAT objects on a tile. |
| `TELE`       | DEFERRED | Teleports overlapping objects to another TELE-tagged tile. |
| `FOLLOW`     | DONE     | See §1 Operators: FOLLOW is an operator (`X FOLLOW Y`), not a property. |

## 4. Properties — interactions / removal

Removal precedence inside DESTRUCT (lower = earlier):

```
1. SINK
2. EAT      (DONE)
3. HOT/MELT
4. WEAK     (DONE)
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
| `EAT`    | DONE     | `X EAT Y [AND Z]*` (EAT is an operator, not a property). Subject destroys listed target kinds on contact. Supports AND for multiple targets. DESTRUCT sub-step (b). |
| `WEAK`   | DONE     | Destroyed when any object arrives on its tile this tick (determined from Move changes in the log). DESTRUCT sub-step (d). |
| `MAKE`   | DONE     | `X MAKE Y [AND Z]*` (MAKE is an operator, not a property). Phase APPLY_MAKE post-DESTRUCT: spawns all targets on every tile containing X. Idempotent (skips if target already present). Supports AND for multiple targets. |
| `POWER`  | DONE     | `X IS POWER` makes the global `POWERED` prefix condition true for this tick. No destruction/removal effect itself. Stored as `P_Power` property; evaluated by `any_has_power()` in `RuleSet`. |
| `HAS`    | DEFERRED | `X HAS Y` spawns Y when X is destroyed; needs to hook DESTRUCT. |
| `BOOM`   | DEFERRED | Destroys self + neighbouring tiles' contents. |
| `SAFE`   | DEFERRED | Immune to DEFEAT/SINK/MELT/etc. |
| `PHANTOM`| DEFERRED | Skips collision checks entirely. |
| `HOLD`   | DEFERRED | YOU+HOLD object can be carried. |
| `FEAR`   | DONE     | See §1 Operators: FEAR is an operator (`X FEAR Y`), not a property. |
| `CHILL`  | DEFERRED | Random subset of MOVE behavior; needs RNG → likely permanent N/A. |

## 5. Properties — transforms

| Property | Status | Notes |
|----------|--------|-------|
| `TEXT` (as predicate) | DONE | `X IS TEXT` converts non-text X objects to text tiles (kind unchanged, `text` flag set). One-directional; coexistence guard skips if text already present on tile. |
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
| Conditions composition (`ON Y AND Z`)  | DONE | `ON Y AND Z` (and `NOT ON Y AND Z`) fully supported; all listed nouns must be present (or absent). |

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
| BUG-1 | ~~`src/sim/simulator.cpp:39-40`, `src/editor/editor.cpp:80-81`~~ | ~~Undo of Destroy spawns a fresh id; subsequent reverse-applied Move records target a now-missing id and the resurrected object lands in the wrong place.~~ | **FIXED** — `World::respawn(id, ...)` added; `sim::Simulator::step_back` and `editor::Editor::undo_edit` both call `respawn` to restore the original id. |
| BUG-2 | ~~`src/app/input_handler.hpp`~~ | ~~`scroll_x/y` is `int`, accumulating ½-tile rounding errors on zoom~~ | **FIXED** — `scroll_x/y` changed to `float`; zoom math is now exact; grid offset uses fractional part. |
| BUG-3 | ~~`src/editor/editor.cpp:81`~~ | ~~Editor's edit-undo also loses original ids (same root cause as BUG-1).~~ | **FIXED** — same `respawn` fix as BUG-1. |

Add new bugs above this line with the next free ID and the same shape.

---

## 10. GUI editor features

| Feature | Status | Notes |
|---------|--------|-------|
| Viewport zoom (mouse wheel, cursor-anchored) | DONE | Float `tile_px`; multiplicative factor; scroll anchored to cursor pixel. |
| Middle-drag pan | DONE | |
| Arrow-key pan (edit mode) | DONE | |
| Object placement (LMB drag) | DONE | One object per tile per drag; coexistence policed for text tiles. |
| Object deletion (RMB drag) | DONE | Deletes all objects on the tile. |
| Edit-mode undo (`Ctrl+Z`) | DONE | Separate from play undo; ring buffer of 500 groups. |
| Palette: search box (click or type) | DONE | Type any char to activate; ESC closes; substring match, case-insensitive. |
| Palette: scroll (mouse wheel over panel) | DONE | |
| Palette: Ctrl+E / Ctrl+Q | DONE | Next / previous entry. |
| Palette: Ctrl+Tab | DONE | Toggle text-twin of selected noun. |
| Palette: Ctrl+R | DONE | Rotate selected entry's default facing; shows direction triangle on tile edge. |
| Orientation triangle | DONE | Small triangle drawn at the facing edge of non-text tiles (hidden below 16 px tile size). |
| Box selection (Shift+drag) | DONE | Inclusive rect in world tile coords; displayed as blue rectangle. |
| Clipboard copy / cut / paste | DONE | `Ctrl+C` / `Ctrl+X` / `Ctrl+V`; paste enters hover mode, LMB to stamp; coexistence checked. |
| File: new level (`Ctrl+N`) | DONE | |
| File: save (`Ctrl+S`) | DONE | Prompts for path if unsaved. |
| File: save-as (`Ctrl+Shift+S`) | DONE | Always prompts. |
| File: open (`Ctrl+O`) | DONE | Modal text-input dialog. |
| Schematics: save (`Ctrl+Shift+E`) | DONE | Saves selection to `schematics/<name>.schem`; tagging mode: LMB=input, RMB=output, Enter=save. |
| Schematics: import (`Ctrl+I`) | DONE | Fuzzy picker over `schematics/`; Up/Down + Enter to load. |
| Schematics: paste preview | DONE | Normal (semi-transparent tiles) or abstract (Ctrl+B to toggle). |
| Schematics: rotate on paste (`Ctrl+R`) | DONE | 90° CW per press; object positions + facings rotated. |
| Schematics: abstract view (`Ctrl+B`) | DONE | Conforming outline + gray fill + blue (input) / red (output) tile highlight + centered name label. Global toggle for all placed schematics. |
| Schematics: nested refs | DONE | `schem` records in `.schem` files; recursively pasted at placement time. |
| Schematics: auto-abstract after creation | DONE | After saving, schematic is immediately recorded as a placed schematic and abstract view is enabled. |
| Play mode: arrow / WASD movement | DONE | |
| Play mode: Space = wait | DONE | |
| Play mode: `Z` / `Ctrl+Z` = tick undo | DONE | |
| Play mode: auto-tick (`P` / `F5`) | DONE | Toggleable timed auto-advance; HUD shows `AUTO:Nms` or `AUTO:MAX`. |
| Play mode: auto-tick speed (`+` / `-`) | DONE | ±50 ms per press; clamped 50 ms – 5000 ms. |
| Play mode: auto-tick max speed (`0`) | DONE | One tick per frame. |
| Rule panel (right side) | DONE | Lists all active property, transform, conditional, EAT, and MAKE rules. |
| HUD bar (bottom) | DONE | Mode, filename (with `*` dirty flag), tick counter, auto-tick label, keyboard hint. |
