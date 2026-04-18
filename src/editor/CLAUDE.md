# src/editor/ — authoring + mode glue

Owns palette state, file I/O wiring, the play/edit mode toggle, and a
*separate* edit-mode undo stack distinct from the simulator's tick
undo stack.

## Files

| File | Purpose |
|------|---------|
| `editor.hpp` / `editor.cpp` | `Editor` class. Wraps a `sim::Simulator`; tracks `EditorMode`; owns the pre-play `World snapshot_`; `place_object`, `delete_all_at`, `undo_edit`, `play_step`, `play_undo`, `serialize`, `load_level`, palette navigation. |
| `palette.hpp` | `PaletteEntry{kind, is_text, default_facing}` and `default_palette()` — the canonical author-time list. |

## Mode toggle

```
Edit ──enter_play()──▶ Play
        snapshot_ = world  (clone)
Edit ◀──enter_edit()── Play
        world = snapshot_  (restore)
        sim_  = Simulator(snapshot_)  (resets tick + tick-undo)
```

Edit-undo is unaffected by the mode toggle and persists across plays.

## Coexistence policing

`place_object` is the only caller that enforces "at most one text per
tile" (see
[architecture.md decision #10](../../docs/architecture.md#2-locked-design-decisions)).
The simulator does not police coexistence at runtime.

## The undo bug (BUG-3)

`undo_edit()` on the `Destroy` branch calls `world.spawn(...)` →
fresh id. Same root cause as `sim::Simulator::step_back` (BUG-1).
Same fix: switch to `world.respawn(c.id, ...)` once it exists.

## Layer rules

- Depends on `core/`, `sim/` only. No raylib.
- Holds *no* raylib types — keyboard/mouse mapping happens in
  `app/input_handler.hpp`, which calls editor methods.
