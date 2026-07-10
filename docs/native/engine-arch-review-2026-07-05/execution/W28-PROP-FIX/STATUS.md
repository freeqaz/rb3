# W28-PROP-FIX — STATUS

**Outcome:** `RB3_PROP_POSE_FULL` (HX_NATIVE, default-OFF) implements the two
CharIKHand-local pieces of the real prop-hand fix — (1) break the `mFinger`
re-projection, (2) redirect the target before the multi-target weight loop. Piece (3)
(prop-tip clip-track binding, `CharDriver`/`CharClip*.cpp`) is **DEFERRED** to the
CROWD-lane-owned files per acceptance A8 (site enumerated in `PLAN.md`).

**Headline result (flag-ON):** every grossly-unreachable IK `skip` is eliminated
(whole-log `mode=skip` 209 → **0**, reproduced across two runs). The played
instrument's hands — **strum + fret — fully meet acceptance** (skip=0 AND 0
`dst_from_hand` entries >30u). The background drummer's `right_hand` (floor-tom) is an
**honest partial**: skip=0 and over-reach collapsed 128→22u, but its redirected at-hand
parent frame sits ~33u from the hand, leaving 12-13 `dst` entries just above the 30u
probe floor — the residual attributable to the deferred piece (3) (only the unbound clip
track brings the floor-tom tip the last few units onto the drumhead). This is the
A8-authorized honest partial.

## Numeric acceptance — computed by committed `analyze_prop_ab.py` (E6)

Harness: `W26-PROP/run_prop_probe.py beastandtheharlot 18`, reach clamp default-ON,
`RB3_IK_CLAMP_DBG=1 RB3_PROP_DST_DBG=1`. `* ` = the three acceptance ikhands.
Full output: `evidence/ab_analysis.txt`. Raw stderr: `evidence/raw/prop_{OFF,ON,ON2}.log.gz`.

| ikhand | skip OFF→ON | clamp OFF→ON | preDist_med OFF→ON | dst_n OFF→ON | dst_med OFF→ON |
|---|---|---|---|---|---|
| *strum.ikhand      | 30 → **0** | 10 → 41-56 | 264.0 → **20.9** | 19 → **0**  | 289.4 → — |
| *fret.ikhand       | 36 → **0** | 7  → 31-58 | 214.7 → **24.6** | 18 → **0**  | 218.7 → — |
| *right_hand.ikhand | 48 → **0** | 24 → 57-61 | 128.1 → **22.0** | 28 → **12-13** | 140.7 → **33.0** |

(strum/fret dst_med "—" = zero entries above the 30u probe floor.) ON and ON2 agree:
strum/fret always 0/0, right_hand skip always 0 with 12-13 dst entries at median ~33u.

**Acceptance verdict (analyze_prop_ab.py exit code):** strum ✅, fret ✅ (skip=0 &
0 dst>30u); right_hand: skip=0 ✅ but 12-13 dst>30u → **honest partial** on the strict
dst bar for the drummer only, per A8 ("if pieces (1)+(2) alone can't reach skip=0
without piece (3), report the honest partial with the numbers — a valid outcome").
skip=0 IS reached for all three; the residual is the dst bar on the background drummer.

## Visible hand-on-instrument (A8)
A/B guitarist closeups via `scripts/native/band-closeup-capture.py --member guitar`
(both passes PASS, pinned 10/10, 0 band drops). `evidence/handOnInstrument_{ON,OFF}_
coop_g_cg{,01}.png`:
- **OFF:** green/yellow IK spike-fan sprays across the top of the frame (arm geometry
  stretched to the unreachable static tip); no hand posed on the fretboard.
- **ON:** spike-fan gone; a coherent posed hand/fingers sits on the guitar neck, venue
  lit and detailed. Hand-on-instrument **confirmed** for the played instrument.

## GATES

| gate | requirement | result |
|---|---|---|
| batch_objdiff (touched fns, flag-OFF) | == baseline | **PASS** — `Poll` 96.13% (base 96.127), `MeasureLengths` 81.34% (base 81.355, untouched); all edits `#ifdef HX_NATIVE` (mwcc doesn't define it) → Wii `.o` byte-identical |
| rb3-tests | 116 / 0 | **PASS** — 116 passed / 0 failed (7 skipped fixtures = baseline) |
| drawlog-golden `--fixed-clock --canonical-order` (flag-OFF) | 792 PASS | **PASS** — 792 draws, 307 known-residual divergences within bound, non-blocking |
| boot A/B (flag-ON) reaches gameplay crash-free | reachable, no crash | **PASS** — both flag-ON `run_prop_probe.py` runs rc=0 reached gameplay & played 18s; band-closeup flag-ON PASS 10/10 pinned, 0 drops |

## Probe-count table (A7) — `grep -ac`, logs carry NUL bytes → use `-a`
Full table: `evidence/probe_counts.txt`. Raw: `evidence/raw/prop_{OFF,ON,ON2}.log.gz`.

| tag | OFF | ON | ON2 |
|---|---|---|---|
| `[IK_CLAMP]` | 300 | 300 | 300 | (probe caps at 300 lines) |
| `[PROP_DST]` | 120 | 120 | 120 | (probe caps at 120 lines) |
| `[IK_CLAMP] mode=skip` (whole log) | **209** | **0** | **0** |
| `[IK_CLAMP] mode=clamp` (whole log) | 91 | 300 | 300 |
| `[PROP_POSE]` | 0 | 0 | 0 | (RB3_PROP_POSE_DBG not armed this sweep) |

## Files changed (staged by path, this lane only)
- `src/system/char/CharIKHand.cpp` — `RB3_PROP_POSE_FULL` flag: `sPropPoseFull()`
  helper; redirect `sOn` forced on under FULL; piece-(2) redirect-before-weight-loop;
  piece-(1) `mFinger` re-projection break. All `#ifdef HX_NATIVE` + env-gated,
  byte-identical `#else`.
- `docs/native/engine-arch-review-2026-07-05/execution/W28-PROP-FIX/{PLAN.md,STATUS.md,
  analyze_prop_ab.py,evidence/}`

## Checkpoint
`/tmp/wave28-checkpoints/PROP-fix.json` (written before cleanup).

## Disposition / no-touch compliance
No default flips, no MILO_ENGINE_PIN bump, no census/classjson/drawlog-sidecar edits
(coordinator-only). Did NOT touch `CharDriver.cpp`/`CharClip*.cpp` (piece 3 deferred),
the gameplay WorldCrowd/RndMultiMesh oracle, the RndMesh loader, hands/finger family, or
concurrent agents' files. `RB3_PROP_POSE_FULL` is a NEW getenv name → census row + any
pin bump are coordinator-only at close-out.
