# W2.1-flip.S4 — Dolphin gameplay A/B package (for coordinator sign-off, E1)

**Package produced by `scripts/native/w21flip-dolphin-ab.py`. This is a
package only — the agent did NOT flip `RB3_PLACEMENT_CONTRACT`'s default and
did NOT touch the drawlog goldens.** Coordinator human-eyes sign-off on this
package is the trigger for the flip commit (per WAVE5_KICKOFF COORDINATOR
ACCEPTANCE E1 / WAVE5_REVIEW R-E).

## What this proves and what it doesn't

- The numeric crowd+drum placement oracle (machine-checkable, exact) already
  proves the contract places the crowd at its faithful `spXfm` and the
  drum kit/band near its `SyncPlayMode` waypoint, flag-ON — see
  `oracle-gate-ON.log` below. **That is not what this package is for.**
- This package is scoped to what only human eyes can judge: does the crowd
  *distribution look right* (spread across the venue bowl, not piled at one
  point) and does the drum kit *sit at the kit position* (not floating at
  the venue origin/center) — compared against Dolphin/retail ground truth.
- **A/A protocol is mandatory**: W2.1/STATUS.md deviation #3 records a
  pre-existing, flag-INDEPENDENT pink-bloom/exposure wash that varies
  run-to-run. Two independent boots per flag state (below) let the reviewer
  see that any wash is present in BOTH flag-OFF and flag-ON — i.e. it is not
  something the flip introduces — before judging placement.

## Numeric oracle companion (flag-ON, `--gate both`)

```
$ python3 scripts/native/placement-gate-capture.py --bin <rb3-native> \
      --tests <rb3-tests> --gate both   # (RB3_PLACEMENT_CONTRACT=1)
exit code: 0   (PASS — both crowd + drum oracle GREEN)
```

Full stdout/stderr in [`oracle-gate-ON.log`](oracle-gate-ON.log); raw
drawlog+probe artifacts in [`oracle-capture/`](oracle-capture/). This reuses
the already-tested W2.1-flip.S1/S2 harness verbatim (no re-derivation).

## Captures (camera-pinned, `RB3_FIXED_CLOCK`, wide venue/establishing shot)

| Flag state | Capture 1 | Capture 2 (A/A) |
|---|---|---|
| flag-OFF (no env, current committed default) | `cap_OFF_1.png` | `cap_OFF_2.png` |
| flag-ON (`RB3_PLACEMENT_CONTRACT=1`) | `cap_ON_1.png` | `cap_ON_2.png` |

Engine logs (nav + shot-pin + `is_playing` traces) alongside each capture as
`cap_<FLAG>_<N>.engine.log`.

## Layout vs ground truth

- [`layout_crowd_vs_dolphin.png`](layout_crowd_vs_dolphin.png) — all 4
  captures side-by-side against `gp_00.png` (Dolphin, crowd spread
  house-left) — present.
- [`layout_drum_vs_retail.png`](layout_drum_vs_retail.png) — all 4 captures
  side-by-side against `fandom_gameplay_drums.png` (retail, drum-kit
  position) — present.
- Ground-truth reference images are also copied alongside for convenience:
  `gp_00.png`, `fandom_gameplay_drums.png`.

## Reviewer checklist (E1 sign-off)

1. Open `layout_crowd_vs_dolphin.png`: does flag-ON's crowd look spread
   across the venue bowl (matching `gp_00.png`'s house-left spread), vs
   flag-OFF's crowd piled at one point?
2. Open `layout_drum_vs_retail.png`: does flag-ON's drum kit sit at a
   plausible kit position (matching `fandom_gameplay_drums.png`), vs
   flag-OFF's kit collapsed to the venue origin/center?
3. Confirm the pink-bloom/exposure wash (if visible) appears in BOTH
   flag-OFF and flag-ON captures (A/A-variable, not flag-caused) — if it
   only appears in one flag state, flag that as a new finding, not expected.
4. If satisfied: coordinator flips `kPlacementContractDefaultOn` 0->1 (the
   one-line change staged by W2.1-flip.S1) and re-goldens the drawlog
   goldens (splash canonical count will move 888->792 — expected, see
   WAVE5_REVIEW A4).

**This package's exit is "package produced + oracle GREEN companion
recorded" — never "flip done."**
