# tests/scenarios/ — behavior contract

Each `.test` file is a self-contained scenario: a starting World, a
sequence of inputs, and assertions about the resulting state. Format
spec: [docs/file-format-v1.md](../../docs/file-format-v1.md).

These files are the source of truth for engine behavior. Any new tick
phase or rule property gets a scenario file BEFORE any implementation
code is written.

## Current files (Phase 1)

| File | What it locks down |
|------|--------------------|
| `01-baba-is-you-basic-move.test` | A YOU object steps one tile in the input direction; facing updates even on blocked moves. |
| `02-push-single-wall.test` | One pushable in the way slides forward; the YOU lands on its old tile. |
| `03-push-blocked-by-stop.test` | A non-PUSH STOP-property object aborts the entire chain — neither the pushable nor the YOU moves. |
| `04-base-rule-text-is-push.test` | The implicit `TEXT IS PUSH` base rule means rule text tiles can be pushed even with no explicit rule on the board. |
| `05-rule-breaks-on-push.test` | Pushing a property text PERPENDICULAR to its strip breaks the rule mid-game. The wall stops being STOP and becomes traversable on subsequent ticks. |
| `06-flag-is-win-triggers-won.test` | A YOU stepping onto a tile that holds a WIN-property object reports `won = true`. |

## Adding a new scenario

1. Pick the next free `NN-` prefix — order does not matter to the
   runner but humans read these top-down.
2. Pick the simplest possible board that exercises the behavior. Wide
   empty borders are fine; clarity beats compactness.
3. Use named text tokens (`baba`, `wall`, `is`, `you`, ...) — see
   `kind_from_name` for the full list.
4. Run `make test`. Confirm the new file is listed in the output and
   that it RED-fails before you write code.
5. Implement, then commit RED → implementation → GREEN per the TDD
   skill the user has authorized.

## Conventions in scenarios

- y grows DOWN (screen coordinates).
- `tick N` asserts the runner ran N tick calls — useful for catching
  off-by-one regressions in the runner.
- `not_won` is sometimes more useful than asserting state; pair them.
- Comments at the top of each file should describe the "why" — what
  rule edge case the scenario exercises. The mechanical setup is
  obvious from the lines that follow.
