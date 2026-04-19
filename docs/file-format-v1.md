# File Format Specification v1

> Source of truth for `.level`, `.schem`, and `.test` text formats.
> Plaintext, hand-editable, line-oriented, UTF-8.

## 1. Goals

- Hand-editable in any text editor without tooling.
- Stable diffs (line order is meaningful only where stated).
- Version field on every file; loader rejects unknown versions.
- One internal representation maps 1:1 to one disk representation. No JSON.
- Cheap to parse: single pass, no lookahead beyond the current line.

## 2. Common Lexical Rules

- Encoding: UTF-8. BOM tolerated and stripped.
- Line ending: LF or CRLF. Writer emits LF.
- Comments: a line whose first non-whitespace character is `#` is a comment. Trailing comments after a record are NOT supported (keeps parsing single-pass).
- Blank lines are ignored.
- Tokens within a record are separated by ASCII whitespace (space or tab). Writer emits single space.
- Identifiers (object kind names, property names) are ASCII, lowercase, `[a-z][a-z0-9_]*`. Examples: `baba`, `wall`, `flag`, `is`, `you`, `text_baba`.
- Integers are decimal, optional leading `-`. No underscores, no hex.
- Strings are bare tokens unless they need to contain whitespace; then they are double-quoted with `\"` and `\\` escapes. Used only by `name` and `meta` records.

## 3. Record Grammar

Every non-blank, non-comment line is a record:

```
<keyword> <arg>*
```

Records are order-independent unless the spec for that record states otherwise. The keyword is matched case-sensitively.

## 4. `.level` File

A complete, playable level. Self-contained: no external references.

### 4.1 Required header records (exactly once each, in any order)

| Record           | Form                          | Notes                                    |
|------------------|-------------------------------|------------------------------------------|
| `version`        | `version <int>`               | Currently `1`. Loader rejects mismatch.  |
| `name`           | `name "<string>"`             | Display name. Empty string allowed.      |

### 4.2 Optional header records (at most once each)

| Record  | Form               | Notes                                                 |
|---------|--------------------|-------------------------------------------------------|
| `meta`  | `meta <key> <val>` | Free-form key/value. Repeated allowed (one per key).  |
| `seed`  | `seed <int>`       | Reserved. v1 sim is deterministic and ignores it.     |

### 4.3 Body records (zero or more, order-significant for ids)

```
object <x> <y> <kind> <facing>
text   <x> <y> <kind>
```

- `<x>`, `<y>`: signed 32-bit integers. World is infinite; no clamp.
- `<kind>`: identifier. For `object`, names a non-text noun (e.g. `baba`, `wall`). For `text`, names a text token (e.g. `baba`, `is`, `you`, `and`, `not`, `wall`, `push`).
  - Note: `text` records take the underlying noun/operator/property name; the parser knows it is a text object because the record keyword is `text`. So `text 0 0 baba` is the text "BABA".
- `<facing>`: one of `right`, `up`, `left`, `down`. Writer emits lowercase. Text objects always face `right` on disk; loader ignores facing on `text` records.

### 4.4 Object identity and ordering

- Object ids are assigned by load order: first body record gets id 0, second gets id 1, and so on.
- Ids are NOT serialized. Round-tripping a level may renumber ids, but only in a stable, deterministic way (file order).
- Tools that need stable external references should use `(x, y, kind)` tuples, not ids.

### 4.5 Coexistence constraint

Per the rule engine spec: a tile may hold at most one `text` record. Loader rejects a level that violates this.

Multiple `object` records on the same tile are allowed (e.g. `baba` standing on `flag`).

### 4.6 Example

```
# A trivial level: BABA IS YOU, FLAG IS WIN
version 1
name "Hello"

# Rule strip
text 0 0 baba
text 1 0 is
text 2 0 you

text 0 2 flag
text 1 2 is
text 2 2 win

# Playfield
object  5 5 baba right
object 10 5 flag right
```

## 5. `.schem` File (Schematic)

A reusable fragment, paste-anywhere. Same record grammar as `.level` with additional header and body records.

### 5.1 Header

| Record    | Form                  | Notes                                              |
|-----------|-----------------------|----------------------------------------------------|
| `version` | `version <int>`       | Required. Same versioning as `.level`.             |
| `name`    | `name "<string>"`     | Required.                                          |
| `origin`  | `origin <x> <y>`      | Required. Reference point for paste offset math.   |

### 5.2 Body records

```
object <x> <y> <kind> <facing>   — same as .level
text   <x> <y> <kind>            — same as .level
tag    <x> <y> input|output      — mark a tile as an I/O port (visual only)
schem  <x> <y> "<rel-path>"      — embed a nested schematic at this position
```

- `tag` records mark tiles as **input** (shown blue) or **output** (shown red) in the
  abstracted preview. Tags are visual metadata only; they do not affect simulation.
- `schem` records reference another `.schem` file by path. On paste, the referenced
  schematic is loaded and stamped recursively at `(tx + (x - ox), ty + (y - oy))`.
  Sub-schematics are rotated by the same amount as the parent when the user rotates
  the schematic before stamping.

Coordinates are stored as authored — they are NOT pre-normalized to the origin.

### 5.3 Paste semantics

Given a schematic with `origin (ox, oy)` and a body record `object x y kind facing`, pasting at target `(tx, ty)` places the object at `(tx + (x - ox), ty + (y - oy))`.

### 5.4 Rotation semantics

Rotating N × 90° CW in screen space (y-down): each tile's relative position
`(dx, dy) = (x - ox, y - oy)` transforms as `(dx, dy) → (-dy, dx)` per step.
Facing direction per step: Right → Down → Left → Up → Right.

### 5.5 Abstracted view

The editor can toggle between a **normal** preview (actual tiles at 50% alpha) and an
**abstracted** preview:
- A conforming outline traced along tile grid edges where occupied meets unoccupied.
- All occupied tiles filled gray.
- Input-tagged tiles filled blue; output-tagged tiles filled red.
- Schematic name centered in the bounding box.

### 5.6 Overlap policy on paste

- An `object` record may stamp onto a tile already holding `object`s.
- A `text` record stamps only if the destination tile holds no other `text`.
  Conflicts on `text` records are silently skipped (partial paste continues).

## 6. `.test` File (Test Scenario)

Combined input + expected-output format for the test harness. One scenario per file. Three sections, in order, each opened by a `[section]` header on its own line.

### 6.1 Section: `[setup]`

A complete level body in the format of section 4. The `version` and `name` records are required; everything else from section 4 is allowed.

### 6.2 Section: `[inputs]`

Zero or more lines, each one of:

```
input <action>
wait
```

Actions: `up`, `down`, `left`, `right`, `wait` (alias for the `wait` form), `undo`.

Each line advances the simulation by exactly one tick. `wait` issues no movement input; `undo` triggers an undo step (and so does not consume a tick of forward simulation — undo is its own step).

### 6.3 Section: `[expected]`

Zero or more assertions about the world after all inputs have been applied. Each is a record:

| Record       | Form                                | Meaning                                                      |
|--------------|-------------------------------------|--------------------------------------------------------------|
| `at`         | `at <x> <y> <kind>`                 | An object of `<kind>` exists at `(x, y)`.                    |
| `not_at`     | `not_at <x> <y> <kind>`             | No object of `<kind>` at `(x, y)`.                           |
| `text_at`    | `text_at <x> <y> <kind>`            | A text of `<kind>` exists at `(x, y)`.                       |
| `count`      | `count <kind> <int>`                | Total non-text objects of `<kind>` in the world equals `<int>`. |
| `text_count` | `text_count <kind> <int>`           | Total text objects of `<kind>` in the world equals `<int>`.  |
| `won`        | `won`                               | The win condition fired during the run.                      |
| `not_won`    | `not_won`                           | The win condition did not fire.                              |
| `tick`       | `tick <int>`                        | The forward tick counter equals `<int>` (undo steps are not counted). |
| `sound_count`| `sound_count <int>`                 | Total `SoundEvent`s emitted across all ticks equals `<int>`. Used to test `PLAY` rules. |

The harness reports the first failing assertion with line number; subsequent assertions are still checked and reported but the test as a whole fails.

### 6.4 Example

```
[setup]
version 1
name "you can push a wall once when wall is push"

text 0 0 baba
text 1 0 is
text 2 0 you

text 0 1 wall
text 1 1 is
text 2 1 push

object 5 5 baba right
object 6 5 wall right

[inputs]
input right

[expected]
at 6 5 baba
at 7 5 wall
not_at 5 5 baba
tick 1
not_won
```

## 7. Versioning

- `version 1` is the initial format. Adding a record type, an optional header field, or a new action keyword bumps to `version 2`.
- The loader rejects unknown record keywords with a hard error pointing at the offending line. There is no silent forward compatibility.
- Writers always emit the highest version they can; round-trips upgrade old files.

## 8. Parser Error Reporting

Every parse error reports `<path>:<line>: <message>`, where:

- `<line>` is 1-based.
- `<message>` is a single sentence; first word names the offending construct (`record`, `field`, `coord`, `kind`, `facing`, ...).
- The first error stops the parse; the loader does not attempt recovery (avoids cascading garbage diagnostics).

## 9. Non-Goals

- No binary format.
- No compression. Levels are small; gzip externally if you must.
- No embedded scripting.
- No layer/z-index field. Layering is implicit (text above non-text, then by id) per rule-engine-spec.md.
- No comments inside a record line.
