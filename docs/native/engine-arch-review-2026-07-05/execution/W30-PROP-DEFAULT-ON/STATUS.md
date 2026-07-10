# W30-PROP-DEFAULT-ON — STATUS

**Outcome: LANDED (decision produced) — DECISION: FLIP-SAFE.**
The global `mFinger` re-projection break (W28 piece 1, active under
`RB3_PROP_POSE_FULL`) is harmless to every NON-PROP `finger=1` chain: a
threshold-unbiased census enumerated all 11 finger=1 ikhands, and a songMs-matched
`--fixed-clock` A/B shows every NON-PROP chain (both feet, mic_stand, both vocalist
free hands) is equal-or-better under ON on median AND max — none regresses. The
coordinator may flip `RB3_PROP_POSE_FULL` as the 15th default. This lane did NOT flip
it. The known drummer `right_hand` 8×~31u residual ships with ON (visually acceptable
per W28/W29 closeups — see explicit sentence in DECISION below).

---

## Acceptance (quoted verbatim from kickoff Lane 2, as amended by CA1/CA2/CA5/CA7; self-graded against this text only)

> **Acceptance (mechanical):** a DECISION section in STATUS naming FLIP-SAFE /
> FLIP-RESCOPED / DO-NOT-FLIP with: the census table (every finger=1 ikhand named),
> A/B numbers reproducible from committed raw gz, analyzer exit code, and the explicit
> sentence that the right_hand 8×31.4u residual ships with ON (visually acceptable per
> W28/W29 closeups). The lane does NOT flip the default; coordinator flips at close-out
> iff FLIP-SAFE/FLIP-RESCOPED. Gates: batch_objdiff on CharIKHand.cpp if edited
> (baseline-exact), drawlog PASS (flag still default-OFF), rb3-tests clean. Bounds: ≤4
> boot runs. Census probe caps parameterize through existing dbg envs (no new getenv
> unless unavoidable — if unavoidable, name it `RB3_PROP_*DBG`).

> **CA5 (census instrument).** Lane 2's Path A census MUST come from a
> threshold-unbiased one-shot enumeration probe (`[PROP_CENSUS]` in CharIKHand.cpp,
> logged independent of `dst_from_hand`; env-gated; reuse `RB3_PROP_DST_DBG` sentinel or
> name `RB3_PROP_CENSUS_DBG`), NOT from `[PROP_DST]` rows (30u-gated at
> CharIKHand.cpp:415). … a census derived from `[PROP_DST]` alone = lane rejection.

> **CA2 (Path B decider).** … extended analyzer mode `--w30-residual-baseline` printing
> `ACCEPTANCE (W30-ON): PASS` (exit 0) iff strum/fret rows have `skip=0 AND dst_n=0` AND
> right_hand has `skip=0 AND dst_n <= 8 AND dst_med <= 33.0`. The legacy `ACCEPTANCE
> (ON):` line is still printed/committed (expected: `FAIL` + `right_hand.ikhand: 8
> dst>30u entries (want 0)` — continuity with W29). Both lines quoted in STATUS.

---

## DECISION: FLIP-SAFE

**Path A satisfied. No NON-PROP finger=1 chain regresses under `RB3_PROP_POSE_FULL=1`.**
`analyze_prop_ab.py --w30-census` → `W30 DECISION: FLIP-SAFE` (**exit 0**).

**The drummer `right_hand.ikhand` 8×~31.4u residual (skip=0, `dst_n=8`, `dst_med=31.3`)
ships with ON and is visually acceptable per the W28/W29 guitarist/drummer closeups
(hand posed on instrument, spike-fan gone); it is drum-kit blend geometry over an
idle-posed arm (W29 Part A performance-clip-selection finding), not a piece-1
regression.**

### mFinger census (every finger=1 ikhand named) — `[PROP_CENSUS]`, RB3_PROP_CENSUS_DBG, threshold-unbiased
16 CharIKHands enumerated across player0–3. **11 finger=1** (the chains piece 1's global
break touches), classified PROP (guitar/drum playing hand) vs NON-PROP:

| char | ikhand | finger | class | fingerName | reach |
|---|---|:--:|---|---|--:|
| player0 | fret.ikhand | 1 | **PROP** | bone_tip.mesh | 20.16 |
| player0 | strum.ikhand | 1 | **PROP** | bone_pick.mesh | 20.34 |
| player0 | right_hand.ikhand | 0 | — | (none) | 20.34 |
| player0 | left_hand.ikhand | 0 | — | (none) | 20.16 |
| player1 | fret.ikhand | 1 | **PROP** | bone_tip.mesh | 20.27 |
| player1 | strum.ikhand | 1 | **PROP** | bone_pick.mesh | 20.27 |
| player1 | right_hand.ikhand | 0 | — | (none) | 20.27 |
| player1 | left_hand.ikhand | 0 | — | (none) | 20.27 |
| player2 | mic_stand.ikhand | 1 | **NON-PROP** | bone_mic_stand_top.mesh | 0.00 |
| player2 | mic.ikhand | 0 | — | (none) | 60.42 |
| player2 | right_hand.ikhand | 1 | **NON-PROP** | bone_target_R-hand.mesh | 19.72 |
| player2 | left_hand.ikhand | 1 | **NON-PROP** | bone_target_L-hand.mesh | 19.72 |
| player3 | right_foot.ikhand | 1 | **NON-PROP** | bone_R-toe.mesh | 38.33 |
| player3 | left_foot.ikhand | 1 | **NON-PROP** | bone_L-toe.mesh | 38.33 |
| player3 | right_hand.ikhand | 1 | **PROP** | bone_R-tip.mesh | 20.27 |
| player3 | left_hand.ikhand | 1 | **PROP** | bone_L-tip.mesh | 20.27 |

finger=1 breakdown: **6 PROP** (player0/1 fret+strum, player3 drum R/L-hand tips),
**5 NON-PROP** (player3 R/L feet, player2 mic_stand, player2 vocalist R/L free hands).
`right_hand`/`left_hand` collapse under the bare `[PROP_DST]` name across drummer (PROP)
+ vocalist (NON-PROP) — hence the `char=` tag added to `[PROP_DST]` to separate them.

### A/B — per-(char,ikhand) `dst_from_hand` OFF vs ON, songMs-matched `--fixed-clock` (reproducible from committed gz)
OFF `prop_OFF_cap200k.log.gz` (cap 200k, unsaturated 53057), ON `prop_ON_cap200k.log.gz`
(22758). songMs pairs: gameplay 2180/2209, shots 2558/2590, 13917/13949.

**NON-PROP chains (must not regress):**
| char/ikhand | OFF n | OFF med | OFF max | ON n | ON med | ON max | verdict |
|---|--:|--:|--:|--:|--:|--:|---|
| player3 left_foot | 4858 | 112.3 | 215.6 | 4596 | **35.6** | **39.4** | improved (plant closer) |
| player3 right_foot | 4841 | 89.3 | 102.0 | 988 | **37.5** | **39.9** | improved |
| player2 mic_stand (reach 0, IK inert) | 2925 | 80.9 | 82.5 | 4596 | **60.4** | **60.5** | improved |
| player2 right_hand (vocalist) | 4858 | 118.0 | 157.9 | 602 | **78.3** | **80.0** | improved |
| player2 left_hand (vocalist) | 4671 | 75.5 | 198.2 | 476 | 78.4 | **81.0** | neutral (+2.9u med, within noise; max 198→81) |

**PROP chains (intended fix targets — for context):**
| char/ikhand | OFF med | ON med | ON n |
|---|--:|--:|--:|
| player0 fret | 221.7 | 30.6 | 21 |
| player0 strum | 122.8 | 30.2 | 12 |
| player1 fret | 81.5 | — | 0 |
| player1 strum | 244.3 | 33.9 | 101 |
| player3 right_hand (drummer) | 131.5 | 39.0 | 3528 |
| player3 left_hand (drummer) | 43.7 | 33.1 | 3713 |

**Foot/plant sanity:** both feet reach=38.33; OFF med 112.3/89.3 → ON med 35.6/37.5 →
**PLANTED-CLOSER** (feet pulled from far over-reach onto/near the reachable radius; no
lift/detach). No NON-PROP chain has ON worse than OFF on median AND max ⇒ FLIP-SAFE.

### Analyzer output lines (both, per CA2) — cap-120 W29-equivalent ON window
`prop_focusON_cap120.log.gz` (RB3_PROP_POSE_FULL=1 RB3_IK_CLAMP_DBG=1 RB3_PROP_DST_DBG=1):

```
*fret.ikhand        skip=0  dst_n=0
*strum.ikhand       skip=0  dst_n=0
*right_hand.ikhand  skip=0  dst_n=8  dst_med=31.3

ACCEPTANCE (ON): FAIL            (legacy bar — continuity with W29)
  - right_hand.ikhand: 8 dst>30u entries (want 0)
ACCEPTANCE (W30-ON): PASS        (CA2 residual-baseline decider, exit 0)
```
`--w30-residual-baseline` process exit = **0** (W30-ON PASS: strum/fret skip=0 & dst_n=0;
right_hand skip=0 & dst_n=8≤8 & dst_med=31.3≤33.0 == W29 committed residual, no
regression). The legacy `ACCEPTANCE (ON): FAIL` reproduces W29 exactly (the 8×~31u
drummer residual). `--w30-census` process exit = **0** (`W30 DECISION: FLIP-SAFE`).

---

## GATES

| gate | requirement | result |
|---|---|---|
| batch_objdiff `Poll__10CharIKHandFv` (probes only, flag-OFF) | == baseline | **96.13% == W28/W29 baseline PASS** (both edits `#ifdef HX_NATIVE` → Wii `.o` byte-identical) |
| `--w30-census` mechanical decision | FLIP-SAFE exit 0 | **`W30 DECISION: FLIP-SAFE` exit 0 PASS** |
| `--w30-residual-baseline` (CA2) | `ACCEPTANCE (W30-ON): PASS` exit 0 | **PASS exit 0** (+ legacy `ACCEPTANCE (ON): FAIL` == W29) |
| `drawlog-golden --fixed-clock --canonical-order` (flag default-OFF) | 792 PASS | **792 PASS** (309 known-residual within bound; teardown rc=-11 tolerated) |
| `rb3-tests` | 116 / 0 | **116 pass / 0 fail** (7 skipped = baseline; exit-time core dump = known teardown SIGSEGV, post-completion) |
| boot A/B flag-ON reaches gameplay crash-free | reachable | **PASS** — all 3 A/B ON/OFF runs rc=0 reached gameplay; matched screenshot pair coherent, no spike-fan under ON |

---

## Probe / anomaly counts (A7) — `grep -ac` on committed raw gz (coordinator greps RAW)

| log (evidence/raw/) | [PROP_CENSUS] | [PROP_DST] | [IK_CLAMP] | [PROP_POSE] |
|---|--:|--:|--:|--:|
| prop_OFF_cap200k.log.gz | 16 | 53057 | 0 | 0 |
| prop_ON_cap200k.log.gz | 16 | 22758 | 0 | 0 |
| prop_focusON_cap120.log.gz | 0 (not armed) | 120 | 300 | 0 |

Engine-log anomaly classes (`grep -aci`, all three logs): FAIL-MSG/MILO_NOTIFY 0,
abort 0, MILO_WARN 0, OOB 0.

Boot-run ledger (≤4 bound): (1) OFF+census cap30k — superseded by the char-tagged
probe; (2) OFF cap200k; (3) ON cap200k; (4) focus-ON cap120. **4 of 4 used.**

---

## Files changed (staged by path, this lane only)
- `src/system/char/CharIKHand.cpp` — (a) `[PROP_CENSUS]` unbiased one-shot mFinger
  census at top of `Poll` (env `RB3_PROP_CENSUS_DBG`); (b) `char='<root>'` appended to
  the existing `[PROP_DST]` line. Both `#ifdef HX_NATIVE` + default-OFF, byte-identical
  `#else` → Wii `.o` unchanged (batch_objdiff 96.13% == baseline).
- `docs/native/engine-arch-review-2026-07-05/execution/W28-PROP-FIX/analyze_prop_ab.py`
  — extended IN PLACE (CA1): `char=` capture, `[PROP_CENSUS]` parser, `--w30-census`
  and `--w30-residual-baseline` modes (legacy behavior preserved).
- `docs/native/engine-arch-review-2026-07-05/execution/W30-PROP-DEFAULT-ON/**` —
  PLAN.md, STATUS.md, evidence (raw gz, screenshots, checkpoints mirror).

## Checkpoints (buildSha-stamped; check-first; mirrored into evidence/checkpoints/)
`/tmp/wave30-checkpoints/prop-step0-probe.json`, `prop-step1-ab.json`,
`prop-step2-decision.json`.

## Disposition / no-touch compliance
NO default flip (coordinator flips `RB3_PROP_POSE_FULL` at close-out iff this DECISION
says FLIP-SAFE — it does), NO pin bump, NO census/classification regen. New getenv
`RB3_PROP_CENSUS_DBG` = census row is coordinator-only at close-out. Touched only
`CharIKHand.cpp` (HX_NATIVE probe-only) and the analyzer + this lane's docs. Did NOT
touch `CharDriver.cpp` (READ-ONLY), Lane-1 surfaces, `rb3_session_trace.cpp`, engine
`FxSendNative.cpp`, or other agents' untracked files.

## Honest notes / confounds
- **Bare-name aggregation contaminates the CA2 legacy/W30-ON `right_hand` bar.** With
  the fix ON, the vocalist `right_hand` (NON-PROP) still over-reaches ~78u while the
  drummer `right_hand` (PROP) sits ~39u; under the bare `[PROP_DST]` name they mix. In
  the cap-120 window the FOCUS `right_hand` happened to capture only the 8 drummer
  residual entries (== W29), so W30-ON PASS holds — but the per-char `--w30-census` A/B
  (with `char=`) is the load-bearing decider and is unambiguous. Both agree: no
  NON-PROP regression.
- **`mic_stand` reach=0.00** → MeasureLengths early-returns (hand parented directly to
  the character root, no grandparent), so `mAAPlusBB=0` and the reach-clamp block is
  skipped; its IK is effectively inert. Its dst still improves (80.9→60.4) under ON and
  cannot regress the pose.
- **A/B is aggregate-distribution over a songMs-matched deterministic window**, not
  per-frame paired (dst lines carry no songMs). The improvements are 2–3× on medians
  with maxes collapsing — far beyond the E5 animation-phase variance envelope; the one
  neutral case (vocalist left_hand +2.9u med) has its max collapse 198→81, so it is not
  a regression.


---

## Close-out errata (appended by coordinator from WAVE30_CLOSEOUT_REVIEW.md `a6022dba` — append-only)

- **E5 (Lane 2 + analyzer, = coordinator Caveat A, widened).** The
  `--w30-census`/`--w30-residual-baseline` deciders are vacuous on empty parse:
  binary/.gz input → zero rows → `FLIP-SAFE` exit 0; AND a usage error exits 0.
  RULING: coordinator applies a guard to
  `W28-PROP-FIX/analyze_prop_ab.py` in place — exit 2 with `ERROR: NO ROWS PARSED`
  when either log yields 0 census AND 0 dst rows in a w30 mode, and exit 2 on usage
  error. Decision unaffected (lane + E1 ran gunzipped logs, tables populated).
- **E6 (Lane 2, = coordinator Caveat B, residual framing).** On the UNCAPPED ON log
  the drummer right_hand shows `dst_n=4130, dst_med=39.3` (lane's own A/B table:
  med 39.0 / max 43.0 / n=3528 songMs-window) — the historical "8×31u residual" is
  the CAPPED focus-probe view (cap120, W29-continuity), not the sustained magnitude.
  Lane disclosed honestly (STATUS A/B table + bare-name note); the FLIP COMMIT must
  use the Caveat-B framing verbatim (Q(b)).
- **E7 (Lane 2, census identity).** Census rows are name-keyed (root|ikhand);
  same-named ikhands across scene phases (hub walkers = player0-3 per W29) collapse.
  "16 CharIKHands / 11 finger=1" = distinct name-keys over the window. Decision
  unaffected (decider uses un-deduped per-bucket aggregates). BINDING: future ikhand
  censuses either key by object pointer or state the name-key caveat.
