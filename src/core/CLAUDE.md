# src/core/ — the simulator

Pure C++20. No I/O, no GUI, no dynamic allocation across the public
boundary that we don't own. Every public type lives in `namespace
baba::core`.

## File map

| File | Purpose |
|------|---------|
| `coord.hpp` | `Coord{int32_t x, y}` + `CoordHash`. y grows DOWN (screen coords). |
| `direction.hpp` | `Direction` enum + `step(Direction) → Coord` delta + `direction_name`. |
| `kind.hpp` / `kind.cpp` | Single `Kind` enum unifying nouns / operators / properties. `is_noun`, `is_operator`, `is_property`, `kind_name`, `kind_from_name`. Backed by one `kTable` array. |
| `object.hpp` | `Object{ObjectId id, Coord pos, Kind kind, bool text, Direction facing}`. `ObjectId = uint32_t`. |
| `change.hpp` | `Change` record (Move/Face) — the unit the undo stack will store. Only Move/Face are produced today. |
| `world.hpp` / `world.cpp` | The World. Dual-indexed: `objects_` by id and `grid_` by Coord. Invariant: `grid_` never holds an empty vector. `spawn` returns a fresh monotonic id; ids are never reused, even after `destroy`. `all_ids()` ascending; `all_cells()` ascending (y, x). |
| `ruleset.hpp` / `ruleset.cpp` | `RuleSet::parse(World)` walks every text-bearing cell, scans horizontally and vertically for `NOUN [AND NOUN]* IS PROPERTY [AND PROPERTY]*`. Always injects `TEXT IS PUSH` as the base rule. Dedup via `index_`. `object_has_property(world, id, prop)`: text objects only ever match TEXT IS <prop>. |
| `tick.hpp` / `tick.cpp` | `apply_tick(World&, Input)`. Phase 1 implements PARSE → APPLY_INPUT → re-PARSE → CHECK_WIN. Multi-YOU resolved in ascending id order. Push chain walks forward collecting PUSH-property objects, aborts on a non-PUSH STOP-property blocker, then slides the chain back-to-front. |
| `loader.hpp` / `loader.cpp` | Single-pass line-oriented parser for `.level` and `.test`. Returns `std::variant<Loaded, ParseError>` — never throws across the API. 1-based line numbers. Tokenizer supports `"quoted"` with `\"` and `\\`. Coexistence rule: at most one text per tile. `serialize_level` round-trips. |

## Determinism contract

Everything that can vary across runs must be sorted before iteration:

- **YOU resolution order**: ascending `ObjectId`.
- **Push chain enumeration on a tile**: ascending `ObjectId` of the
  pushables on that tile.
- **`all_ids()` / `all_cells()`**: ascending id / ascending (y, x).
- **Tile lookups in `World::at`**: order is the order of `spawn` /
  arrival, which is itself driven by ascending-id callers — keep it
  that way.

If you add a new tick phase, sort any container you iterate that came
out of an `unordered_map`.

## Adding a new property

1. Extend `Kind` (`kind.hpp`) and `kTable` (`kind.cpp`) with the new
   `P_*` and `is_property` returning true.
2. If it's a transformation rule (e.g. `BABA IS WALL`), `RuleSet`
   already gathers it as a property rule; the *consumer* lives in the
   tick phase (e.g. a future TRANSFORM phase).
3. Plumb it into the relevant tick phase. Re-parse afterwards if the
   change can affect rules.
4. Add a scenario file under `tests/scenarios/` and (optionally) unit
   tests for any new helpers.

## Testing

- Unit tests for `World` and `loader` live in `tests/unit/`.
- Behavior tests live in `tests/scenarios/*.test` and are the contract
  the engine must honor.
- Run both with `make check`.
