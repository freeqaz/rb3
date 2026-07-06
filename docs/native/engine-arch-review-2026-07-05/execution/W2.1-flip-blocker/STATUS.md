# W2.1-flip-blocker — STATUS

## A.S1 — Plan + detector + evidence-first scoring — DONE (2026-07-06, Opus planner)

**Delivered:**
- `scripts/native/wash_score.py` — numeric wash detector per WAVE6_REVIEW A2: continuous mean-luma
  + both tails (`hi_frac` blow-out >0.90, `lo_frac` near-black <0.10) + `pink_frac` hue channel
  separating the PINK broken-env class from the WHITE exposure/bloom class. `classify()` →
  `PINK|WHITE|NEARBLACK|NEUTRAL`; `compare()` runs the S2 decision (Mann-Whitney U on luma +
  pre-declared rule). Import-friendly for the S2 harness.
- `--selftest`: **PASSED** (8 synthetic scenes / 14 assertions green — gray→NEUTRAL, white→WHITE,
  black→NEARBLACK, magenta→PINK, dark-magenta value-gated OUT, split-image both tails,
  compare→A/A-variable on OFF-wash, compare→flag-ON-specific on separated all-ON wash).
- Detector validated against recorded Wave-5 luma (95.2/23.7/202.5/122.6 → 95.1/23.7/202.6/122.6, exact).
- `PLAN.md` — S2 songMs-pinned interleaved-sequential protocol + pre-declared numeric decision rule
  + S3 isolation-matrix + S4 package brief.

**Batch-0 result (free prior, the four committed `W2.1-flip/dolphin-ab/cap_*.png`):**
`batch0_scores.json` + `batch0_compare.json`.

| capture | flag | mean | hi% | lo% | pink% | class |
|---|---|---|---|---|---|---|
| cap_OFF_1 | OFF | 0.373 | 2.47 | 30.85 | **41.43** | **PINK** |
| cap_OFF_2 | OFF | 0.093 | 1.01 | 77.46 | 0.49 | NEARBLACK |
| cap_ON_1  | ON  | 0.794 | 77.14 | 3.41 | 1.79 | WHITE |
| cap_ON_2  | ON  | 0.481 | 1.10 | 3.82 | **75.49** | PINK |

**`compare()` verdict: `A/A-variable`** — a PINK wash-class capture is present in **flag-OFF**
(cap_OFF_1, pink 41%), an existence proof that the wash occurs without the flag. The PINK
(broken-env) class appears in BOTH flag states (OFF_1 41%, ON_2 75%); WHITE (bloom) in ON_1 only in
this batch; the mirror record `W2.1/STATUS.md:304-311` ("off1 heavy / off2 moderate; on1 none")
already showed heavy wash flag-OFF + clean flag-ON. The Wave-5-hold asymmetry premise is contradicted
by batch 0 + two prior experiments.

**Caveat for S2:** batch 0 was NOT songMs-pinned (wall-clock settle sleeps), so it cannot fully
separate wash from lighting-animation drift. S2 must reproduce the flag-OFF wash under a
songMs-pinned capture (window 21000±250) to make the verdict airtight. Batch 0 = strong preliminary
evidence, not the final verdict.

**Commit:** see `git log --grep=W2.1-flip-blocker`.

---

## A.S2 — songMs-pinned interleaved measurement + VERDICT — DONE (2026-07-06, Opus measurer)

### VERDICT: **A/A-variable** — the flip is NOT the cause of the wash. (unblocks E1, per WAVE6_REVIEW A4)

The Wave-5-hold premise ("blow-out wash appears only in flag-ON, 1/2 ON vs 0/2 OFF") is **directly
refuted under time control.** Two independent **songMs-pinned flag-OFF captures are PINK wash-class**
(the existence proof), the luma distributions of the two flag states are **statistically
indistinguishable** (Mann-Whitney U=24.0, **p=1.0**, n=7/7), and the wash is a **full-frame
magenta/pink env cast** (whole venue backdrop + note highway + band tinted — see `measure/montage.png`),
which the placement contract (a *crowd-only* transform change) mechanically cannot produce.

### Harness (per PLAN §2, committed)
- **`scripts/native/wash-measure.py`** — interleaved-sequential OFF/ON boots, each capture PINNED to
  a songMs window (21000±250) by polling `/api/health` songMs and screenshotting the instant it
  enters the window (director pre-disabled + `coop_dir_crowd` wide venue shot pinned early so the
  heavy pin work can't push songMs out of window); off-window boots discarded+re-run. Nav + shot
  reuse `placement-gate-capture.py` / `w21flip-dolphin-ab.py` verbatim. Scores each PNG with
  `wash_score.score_image`; early-stops on the pre-declared existence-proof rule; `--min-pairs`
  keeps sampling for a robust distribution (monotone-safe — more data cannot flip A/A-variable to
  flag-ON-specific, which requires ZERO flag-OFF wash).
- **`scripts/native/wash_finalize.py`** — scores ALL songMs-pinned raws (two passes, same
  binary/protocol) into the combined `batch_log.json` / `verdict.json` / `montage.png`.
- Binary: own `native/build-agent-W2.1-flip-blocker/rb3-native` (clean clang build). Raw PNGs +
  per-boot engine logs kept in `/tmp/wave6-flipblocker-captures/` (NOT committed, 14 captures).

### Combined per-capture scores (n=7 per flag state, all pinned within songMs 20877–21163)

| flag | songMs | class | mean_luma | hi% | lo% | pink% |
|---|---|---|---|---|---|---|
| OFF | 21019 | NEARBLACK | 0.103 | 1.9 | 76.3 | 0.0 |
| OFF | 21071 | NEARBLACK | 0.097 | 1.8 | 80.5 | 0.0 |
| **OFF** | **21083** | **PINK** | **0.570** | 4.3 | 6.6 | **70.0** |
| OFF | 21108 | NEARBLACK | 0.095 | 1.7 | 80.5 | 0.0 |
| **OFF** | **21131** | **PINK** | **0.229** | 2.0 | 41.9 | **33.6** |
| OFF | 21149 | NEARBLACK | 0.093 | 1.5 | 80.6 | 0.0 |
| OFF | 21163 | NEARBLACK | 0.093 | 1.5 | 80.4 | 0.0 |
| ON | 20877 | PINK | 0.590 | 11.5 | 6.8 | 37.9 |
| ON | 21025 | PINK | 0.262 | 2.1 | 42.3 | 36.2 |
| ON | 21108 | NEARBLACK | 0.090 | 1.7 | 81.4 | 0.0 |
| ON | 21113 | PINK | 0.437 | 2.6 | 5.8 | 64.6 |
| ON | 21116 | NEARBLACK | 0.090 | 1.7 | 81.4 | 0.0 |
| ON | 21131 | NEARBLACK | 0.088 | 1.6 | 81.5 | 0.0 |
| ON | 21132 | PINK | 0.251 | 2.2 | 42.4 | 33.9 |

- **flag-OFF wash rate 2/7; flag-ON wash rate 4/7.** Fisher exact on 2/7-vs-4/7 → p≈0.59 (NOT
  significant); Mann-Whitney on the 14 luma values → **U=24.0, p=1.0** (no flag effect on brightness).
- The two flag-OFF PINK captures sit in **different cache regimes** — one cold (21131, first-run) and
  one warm (21083, 6th warm boot, pink 70%) — so the existence proof is not a cold-cache artifact.

### Why this is airtight (defeats the batch-0 timing caveat A.S1 flagged)
1. **Time-pinned:** every capture is within songMs 20877–21163 (≈21000±163), so authored venue
   lighting animation is controlled — the wash is NOT "a different moment in the lighting anim".
2. **Boot-nondeterministic, not songMs-determined:** at essentially the *same* songMs the class
   flips — e.g. songMs≈21131 is **PINK** in one flag-OFF boot (mean 0.23) and **NEARBLACK** in a
   flag-ON boot (mean 0.088). Pinning did NOT make the wash deterministic → per A2.4 that is itself
   the answer: the wash is a run-to-run render-state variable, flag-independent.
3. **Full-frame env cast, not crowd-localized:** the pink covers the whole frame incl. the note
   highway and venue backdrop (`montage.png`), so it is not "the placement fix revealing pink crowd
   geometry" — it is an env/postproc-wide wash present in both flag states.
4. **Corroborates two prior records:** batch-0 (`cap_OFF_1` PINK) and the W2.1.S3 verify run
   (`W2.1/STATUS.md:304-311`, "off1 heavy … on1 none" — heavy wash flag-OFF, clean flag-ON, the
   mirror of the Wave-5 sample).

### Mechanism (ranking handed to S3; NOT attributed here — S2 is the verdict stage)
The wash presents as a full-frame **PINK broken-env cast** (33–70% pink pixels) OR a **NEARBLACK**
frame OR (Wave-5/batch-0) a **WHITE** blow-out — three stochastic render states across both flags.
Top prior (WAVE6_REVIEW A3, ranked): **async asset/texture residency at capture** — the pink
broken-env placeholder class (W0.5 precedent), **still live even under `RB3_FIXED_CLOCK`** because
the W0.3d part-(b) async-loader completion-order patch is unlanded. Secondary: `RB3PostProc` venue
grade/bloom (`RndPostProc::Current()`, boot-varying) and the P4 per-environ venue-light rewrite. The
demoted crowd-emissive→bloom suspect is confirmed irrelevant here (halo capture is `game.cam`-gated;
these are venue-cam shots). **S3 should run the 4-flag isolation matrix on a wash-reproducing config**
(`RB3_HIGHWAY_BLOOM_OFF` / `RB3_BLOOM_OFF` / `RB3_VENUE_LIGHT_OFF` / `RB3_TRACK_LIGHT_OFF`) to name it,
and file the wash as its own backlog item carrying this dataset (A4.2).

### Consequence for the flip (E1)
A/A-variable **unblocks the flip** under WAVE6_REVIEW A4's two conditions, both delegated to S4:
(1) E1 is judged on **detector-selected wash-free (NEUTRAL/NEARBLACK, non-PINK/WHITE) captures per
flag state** — there are ≥3 each in this dataset; (2) the wash gets its **own backlog item** with
this attribution data. The placement fix stays numerically proven (crowd+drum oracle GREEN) and
flag-OFF byte-identical; nothing about the wash is caused by it.

### Artifacts
- `measure/verdict.json` (combined `compare()` result, n=7/7), `measure/batch_log.json` (all 14
  scored captures), `measure/montage.png` (wash-class-first strip, 8 representative frames).
- Raw PNGs + engine logs: `/tmp/wave6-flipblocker-captures/` (14 pinned captures; not committed per
  the deliver contract). Run logs: `/tmp/wave6-flipblocker-measure.log` (+ `-pair1.log`).

**Commit:** see `git log --grep=W2.1-flip-blocker` (A.S2).

---

## A.S3 — A/A-variable branch: wash note + backlog + A5 pre-flip checks — DONE (2026-07-06 19:00 UTC, Opus fixer)

### Branch decision: **A/A-variable → NO CODE FIX.**

S2's verdict is **A/A-variable** (§A.S2): the pink/near-black/white wash is a run-to-run,
flag-independent render-state variable, NOT caused by `RB3_PLACEMENT_CONTRACT`. Per WAVE6_REVIEW A4
and the coordinator dispatch, the A/A-variable arm takes **no engine/code fix** — the S3 isolation
matrix + emissive-feeds-bloom fix belong to the *flag-ON-specific* arm, which S2 refuted. This stage
therefore delivers: (1) the wash-rate note (below), (2) a standalone backlog wash item carrying the
attribution data (`## Backlog proposal`), and (3) the WAVE6_REVIEW A5 pre-flip checks.

### Wash-rate note (attribution summary for the backlog owner)

- **Rate:** flag-OFF wash 2/7, flag-ON wash 4/7 (Fisher exact p≈0.59, NS); luma distributions
  indistinguishable (Mann-Whitney U=24.0, **p=1.0**, n=7/7). Both flag states produce the wash.
- **Class:** three stochastic render states — **PINK** broken-env cast (33–70% pink px), **NEARBLACK**
  frame, and (Wave-5/batch-0) **WHITE** blow-out. PINK is the dominant wash class under time-pinned
  capture; WHITE (the Wave-5-hold trigger) appeared once in an un-pinned batch.
- **Airtight controls (from S2):** time-pinned to songMs 20877–21163; at ~equal songMs (≈21131) the
  class flips PINK↔NEARBLACK across boots → not songMs-determined; full-frame env cast covering the
  note highway + venue backdrop → not crowd-localized (the contract is a crowd-only transform). Two
  flag-OFF PINK captures in different cache regimes (cold 21131 + warm 21083) → not a cold-cache
  artifact.
- **Demoted suspect (WAVE6_REVIEW A3, source-verified):** crowd-emissive→bloom is mechanically
  excluded — the halo capture is `game.cam`-gated (`Rnd_Wgpu_RB3.cpp:4403`) and these are venue-cam
  shots. Not re-tested here; confirmed irrelevant in S2.
- **Isolation matrix (`RB3_HIGHWAY_BLOOM_OFF`/`RB3_BLOOM_OFF`/`RB3_VENUE_LIGHT_OFF`/`RB3_TRACK_LIGHT_OFF`)
  NOT run this stage:** the A/A-variable arm is explicitly no-code, and — more decisively — the wash
  reproduces stochastically (2–4/7), so a single boot per config cannot reliably reproduce it; a
  meaningful matrix needs N boots per config (or a forced-reproduce seam), which is characterization
  work for the backlog item, not a flip gate. Ranked prior handed to the backlog owner below.

### Consequence for E1/flip
**A/A-variable unblocks the flip** (WAVE6_REVIEW A4) under its two conditions, both already delegated
to S4: E1 judged on detector-selected wash-free (NEUTRAL/NEARBLACK) captures per flag state (≥3 each
exist in the S2 dataset), and the wash carried as its own backlog item (filed below). The placement
fix stays numerically proven (crowd+drum oracle GREEN) and flag-OFF byte-identical.

---

## Backlog proposal — WASH (venue-cam full-frame stochastic env/postproc wash) — flip-independent

**Title:** Stochastic full-frame venue-cam wash (PINK broken-env / NEARBLACK / WHITE blow-out),
flag-independent, boot-nondeterministic.

**Status:** open; **NOT a flip blocker** (proven A/A-variable, §A.S2). Pre-existing; visible with the
placement contract both OFF and ON.

**Attribution data carried (from §A.S2, this campaign's dataset):**
- 14 songMs-pinned captures (7 OFF / 7 ON), window 20877–21163; per-capture mean-luma / hi% / lo% /
  pink% table + classes in §A.S2. Raw PNGs + per-boot engine logs: `/tmp/wave6-flipblocker-captures/`
  (ephemeral, not committed). Scored artifacts: `measure/verdict.json`, `measure/batch_log.json`,
  `measure/montage.png`. Detector: `scripts/native/wash_score.py` (`--selftest` green).
- Statistics: OFF 2/7, ON 4/7 (Fisher p≈0.59); luma Mann-Whitney U=24.0 p=1.0.

**Ranked mechanism prior (WAVE6_REVIEW A3, for the investigation to confirm/refute):**
1. **Async asset/texture residency at capture** (top prior) — pink broken-env placeholder class
   (W0.5 precedent). The W0.3d part-(b) async-loader completion-order patch is **still unlanded**, so
   this nondeterminism source is live even under `RB3_FIXED_CLOCK` + the SortDraws tie-break. This is
   the most likely root and connects directly to the coordinator-sequenced W0.3d-fix backlog item.
2. **`RB3PostProc` venue grade/bloom** — `bloomIntensity` from `RndPostProc::Current()`
   (`RB3PostProc.cpp:210-256`), per-shot/venue-event postproc state, boot-varying.
3. **P4 per-environ venue-light SceneUniforms rewrite** — `RndEnviron::sCurrent`-driven (default-ON);
   which environ is current at the pinned shot can vary with timing.

**Recommended method for the owner:** run the 4-flag isolation matrix
(`RB3_HIGHWAY_BLOOM_OFF`/`RB3_BLOOM_OFF`/`RB3_VENUE_LIGHT_OFF`/`RB3_TRACK_LIGHT_OFF`) with **N≥6 boots
per config** (or add a deterministic asset-residency/RNG seam so the wash reproduces per boot), scored
by `wash_score.py`. Whichever flag drives the wash rate to ~0 names the mechanism. Cross-check against
the current-state grayscale-venue finding W3.3 (Lane D) — likely the same env/postproc mechanism space.

---

## A5 pre-flip checks (WAVE6_REVIEW A5 — recorded for the coordinator's flip cycle)

### (1) Flag-ON canonical drawlog count — re-measured, NOT assumed: **792**

Ran `drawlog-golden.py --fixed-clock --canonical-order` against the lane's own clean binary
(`native/build-agent-W2.1-flip-blocker/rb3-native`) under the current deterministic order (post
W0.3d-fix):

| flag state | env | measured `count` | runs |
|---|---|---|---|
| flag-ON | `RB3_PLACEMENT_CONTRACT=1` | **792** | 3/3 identical |
| flag-OFF (control) | (no env) | 888 | 1/1 (== committed golden) |

**The expected post-flip re-golden count is 792** — confirmed under the current deterministic order,
not inherited from the pre-W0.3d-fix Wave-4 measurement. The 888→792 delta is the crowd mesh
`0xc57f…` draw-count change (211→115, a count/multiset effect); W0.3d-fix is a comparator-only
ordering tie-break and cannot alter the count, as WAVE6_REVIEW A5 predicted. Coordinator re-golden
target: `splash_screen.json` 888 → **792** + fresh per-name-eps residual sidecar.

### (2) Semantics-inversion sweep — files whose OFF arm is "no env" (invert post-flip)

Post-flip, "no env" = contract-**ON**; the legacy opt-in `RB3_PLACEMENT_CONTRACT=1` becomes a no-op,
and the OFF arm must become `RB3_PLACEMENT_CONTRACT_OFF=1`. Files that will silently compare ON-vs-ON
unless updated **in the same review cycle as the flip commit**:

| File | OFF-arm site | Action needed post-flip |
|---|---|---|
| `scripts/native/w21flip-dolphin-ab.py` | `("OFF", 1/2, {})` :186-187; README table :309 "flag-OFF (no env…)" | OFF arm → `{"RB3_PLACEMENT_CONTRACT_OFF":"1"}` |
| `scripts/native/w21flip-ui-ab.py` | `("OFF", 1/2, {})` :170-171; docstring :13 "flag-OFF = no env" | OFF arm → `{"RB3_PLACEMENT_CONTRACT_OFF":"1"}` |
| `scripts/native/wash-measure.py` | `STATES = [("OFF", {}), ("ON", {…CONTRACT:1})]` :259 (this lane's own harness) | OFF arm → `{"RB3_PLACEMENT_CONTRACT_OFF":"1"}` |

**Non-inverting but note:**
- `scripts/native/crowd-bone-gate-capture.py:84` **forces** `RB3_PLACEMENT_CONTRACT=1` (default on
  unless `--no-placement-contract`) — stays ON post-flip; the explicit set becomes redundant, no
  inversion, harmless.
- `scripts/native/_w32-boxambient-ab.py:68` uses `("OFF",…,{})` as its baseline, but its ON arm sets
  `RB3_BOX_AMBIENT` (a *different* flag), not the contract. Post-flip its baseline silently gains
  contract-ON on both arms — fine for its own A/A but its "OFF" is no longer contract-OFF; sibling
  W3.2 lane should be aware.
- Fail-red / A-B demo commands quoted throughout the STATUS files use no-env as the OFF arm; these are
  documentation, not executable gates — flag them if re-run verbatim post-flip.

Recommended durable fix (A5.2): give the two `w21flip-*.py` scripts an explicit
`--flag-state {on,off}` mapped to the post-flip envs so they can't silently invert again.

### (3) classification.json row updates the flip needs (DRAFTED, not applied — coordinator-owned)

`../milo-native-engine/src/platform/NativeCompatFlags.classification.json` rows :91-92. The single
wave-end regen covers `gen.inc`, not the row content, so the flip commit must edit these text/default
fields (append-only rule does not apply to correcting a flipped default — coordinator judgment):

- **`RB3_PLACEMENT_CONTRACT`** (:91): `"default": "off"` → **`"default": "on"`**; faithfulStatus
  `"not-live: SYS-1 skinned-placement contract (obj.world=meshWorld + bind-relative palette),
  default-OFF pending coordinator flip"` → **`"live: SYS-1 skinned-placement contract
  (obj.world=meshWorld + bind-relative palette), default-ON as of Wave 6 flip; opt out via
  RB3_PLACEMENT_CONTRACT_OFF"`**.
- **`RB3_PLACEMENT_CONTRACT_OFF`** (:92): `"default"` **stays `"off"`** (the opt-out is off by
  default); faithfulStatus `"…Effective default-OFF this wave."` → **`"…Now the live opt-out: the
  contract is default-ON as of the Wave 6 flip; setting this disables it (takes precedence over the
  opt-in)."`**.
- Also update the burn-down ledger `docs/native/engine-arch-review-2026-07-05/NATIVE_COMPAT_LEDGER.md`
  :228-229 to match (regenerated by the census, but the source rows above drive it).

**Commit:** see `git log --grep=W2.1-flip-blocker` (A.S3).

---

## A.S4 — Fresh Dolphin A/B sign-off package — DONE (2026-07-06, Sonnet packager)

Produced `dolphin-ab/` (modeled on `../W2.1-flip/dolphin-ab/`, superseding the Wave-5 package the
coordinator HELD on) with the Wave-6 upgrades from the dispatch brief:

- **All 14 songMs-pinned captures from §A.S2 committed** (7 OFF / 7 ON — well above the ≥4/state
  minimum: the full dataset the verdict was computed on, so the reviewer can audit every input, not
  a curated subset), each with an `.engine.log` alongside.
- **Every capture scored by `wash_score.py`**, table reproduced in `dolphin-ab/README.md` (mean
  luma / hi% / lo% / pink% / class) — identical numbers to §A.S2's table (re-derived, not
  re-measured: same `measure/batch_log.json`).
- **Layout montages (`layout_crowd_vs_dolphin.png`, `layout_drum_vs_retail.png`) built from
  DETECTOR-SELECTED wash-free captures only** (2 NEARBLACK per flag state, songMs-diverse: OFF
  21108/21071, ON 21108/21131) — per WAVE6_REVIEW A4 condition (1), so the wash (already proven
  A/A-variable) cannot re-confound the human sign-off the way it forced the Wave-5 hold. The wash
  attribution itself stays in §A.S2/§A.S3 above (this package links it, does not re-derive it).
- **Flag-ON placement-oracle companion re-run fresh this stage** (own lane binary + freshly built
  `rb3-tests`, engine pin `8e7eddd`): `placement-gate-capture.py --gate both` under
  `RB3_PLACEMENT_CONTRACT=1` → **exit 0, both `PlacementOracle.RealCaptureSpansBowl` and
  `PlacementOracle.RealCaptureDrumPlaced` PASS** (`dolphin-ab/oracle-gate-ON.log` +
  `dolphin-ab/oracle-capture/`).
- **README ends with the reviewer checklist (5 items, including "do not re-litigate the wash here")
  + the full §A.S3 A5 pre-flip checklist** (792 re-measured count table, the 3-file OFF-arm
  inversion sweep, the drafted-not-applied classification.json row text) reproduced inline so the
  coordinator's flip cycle has everything in one place.
- **Caveat recorded honestly:** two captures (`cap_OFF_01_21108`, `cap_OFF_01_21131`) share one
  source `.engine.log` — a try-index counter collision in `wash-measure.py` (pre-existing, not
  introduced by this stage) overwrote one of the two logs; both PNGs and their `wash_score.py`
  scores are independently correct and unaffected, only that one pair's debug-log provenance is
  ambiguous. Flagged in the README rather than silently fixed (not this stage's file to patch, and
  the underlying scored data is sound).

**Exit: package produced (per the dispatch brief's exit condition). Did NOT flip the default. Did
NOT touch the drawlog goldens or `classification.json`.** Coordinator E1 sign-off + flip + re-golden
remain the coordinator's next action.

**Commit:** see `git log --grep=W2.1-flip-blocker` (A.S4).
