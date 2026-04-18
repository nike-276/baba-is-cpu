# src/sim/ — runtime around the core

Owns the per-session state that `core/` is deliberately stateless about:
the world being simulated, the undo buffer, and the tick counter.

## Files

| File | Purpose |
|------|---------|
| `simulator.hpp` / `simulator.cpp` | `Simulator` class. `step_forward(Input) → TickReport` calls `core::apply_tick` and pushes the change list to undo. `step_back()` pops and inverse-applies. |
| `undo_buffer.hpp` | Ring buffer of `vector<Change>` with cap (default 10,000). `push` / `pop` / `clear`. |

## The undo bug (BUG-1)

`step_back` currently inverse-applies a `Destroy` record by calling
`world.spawn(...)`, which allocates a *fresh* id. If the same tick
contained later `Move` records against the original id (which is
common — kill-then-shove), reverse-application targets a now-missing
id and the resurrected object ends up in the wrong place.

The fix is to add `World::respawn(id, ...)` so the inverse of Destroy
restores the original id. This deviates from
[rule-engine-spec.md §6](../../docs/rule-engine-spec.md#6-multiple-you-resolution)
which calls for a fresh id on undo — the user has explicitly chosen
the deviation. Spec is being updated in lockstep.

## Layer rules

- Depends on `core/` only. No raylib, no editor.
- Stateless from the caller's perspective: a `Simulator` is
  constructed, ticked, undone. No global state, no singletons.
- All iteration over Changes uses the order produced by `apply_tick`;
  do not re-sort.
