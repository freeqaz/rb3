# Wave 12 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE12_REVIEW.md` rb3 `464d6041`) — **all 11
amendments adopted**; dispatched with the corrected shape below.
Parent: `execution/README.md` (Wave 11 results + Wave 12 menu). Engine pin `146fd19`.

## COORDINATOR ACCEPTANCE (2026-07-07) — final dispatched shape

Fable review: **dispatch-with-amendments** (11). Adopted in full:

- **A1 (RECORD CORRECTION) — the "staged since Wave 4" loader patch DOES NOT EXIST.** The only
  staged W0.3d part-b artifact was the SortDraws name tie-break, which LANDED in Wave 5
  (`76f51077`, live `Utl.cpp:192-199`). `WAVE7_REVIEW.md:40-58` already refuted this claim once;
  it regressed via BOOTRNG's backlog wording. **Lane A is NEW design work**; a staged-design exit
  is legitimate but "staged" must mean a `git apply --check`-able artifact in the repo. README
  Wave-12 menu + memory corrected in this commit.
- **A2 — mechanism model rewritten.** "Insertion order → gRand consumption order" is unsupported:
  per-frame consumption is name-sorted (`SortPolls` `Utl.cpp:207-214`), completion callbacks draw
  no gRand (`Rand.cpp:80-97` MainThread assert), and BOOTRNG's ~11,231-draw spread is a COUNT axis
  an order-only permutation cannot produce. **Prime suspect = completion-FRAME TIMING** (ThreadCall/
  async I/O completions landing on different sim frames → per-frame consumers start/advance at
  different offsets, value-feedback amplifies). S1 = attribution-first: per-frame `RB3GRandDrawCount`
  + per-dir load-completion frame, N≥6 boots, find the FIRST divergent frame; pre-register H-TIMING
  vs H-ORDER (the unsorted `mAnims` hash-order walk `Dir.cpp:50-63` is the one genuine order
  channel) vs the cheap third candidate: **re-seed gRand at a canonical mid-boot anchor** under
  fixed clock (0x5EED precedent).
- **A3 — seam shape corrected.** "SortDraws shipped default-ON" was FALSE (it is
  `RB3FixedClockActive() && !RB3DrawSortDeterministicOff()`-gated). The Lane-A seam =
  **fixed-clock-scoped + own opt-in flag (`RB3_LOAD_DETERMINISM`) during the wave**, coordinator
  flips opt-in→fixed-clock-default at wave end with a re-golden provision (regression gate reads
  "flag-OFF 792 byte-identical; flag-ON 792-or-coordinator-re-golden"). Default-ON would also fight
  the shipped incremental-load stack. Other lanes' fixed-clock gates stay stable mid-wave.
- **A4 — gate hygiene:** land S3's `--tol` tighten FIRST so the SECONDARY spread gate compares
  same-tol arms vs a fresh OFF-arm baseline; re-state the A9 escape clause (PRIMARY-pass/
  SECONDARY-fail = file the FX/swept-light-phase finding, no gate fudge); fail-red proof = seam-ON
  10/10 identical stream position **under induced contention + env-gated worker-latency jitter**,
  seam-OFF reproduces the spread under the same jitter (the W0.3c quiescent-machine trap).
- **A5 — Lane B gates gain Instrument B** (per-vertex shell invariant `‖s(v)−ŝ(v)‖` with the A7
  co-variation bar — the fix gate `S2_WAVE12_INSTRUMENT_DESIGN.md` mandates): S1 BUILDS it;
  S2 gates add "Instrument B co-varies with wext pre-fix, reads ~0 post-fix, guard-DROP census
  unchanged (no fix-by-hiding)". A wext-only gate is gameable by a vertex clamp.
- **A6 — S1 must DISTINGUISH authored-SPACE error from per-vertex WEIGHT/INDEX/decode error**
  (all current evidence consistent with both; the CPU mirror shares the decode, so "GPU exonerated"
  ≠ decode exonerated). Pre-registered branches: Instrument B RED co-varying → composition axis;
  Instrument B GREEN while wext RED → weights/indices/decode axis. Cheap check: do sampled verts
  sharing a dominant bone move by ONE rigid rotation (space) or scatter (decode)? Composition
  oracle = the in-repo W0.1 skin golden `RefSkinVertex` extended with real `hands_naked` inputs;
  DC3 = corroboration only. Line ranges re-declared: dualskin block now `:4453-4735`, wext
  `:4360-4394` (TU 5,775 lines).
- **A7 — C1's regression arm SKIPPED (already resolved):** binder byte-unchanged
  `a94762f..146fd19` (empty git log); **Wave 7's own `W4.2/cs2_hub_off_vs_on.png` flag-ON panel
  already shows pale-yellow-on-gold** (the hub top-level items were never dark post-flip — Wave 7
  verified QUICKPLAY, a different label route); chroma-preserve is genuinely venue-gated
  (`venueGrade>0.5`, menus pass false). C1 = path tracing from day one: which material/color route
  hub items take and where retail's DARK focused color comes from (UILabel focus-state color never
  applied natively, or a font-material variant bypassing the binder's UI-text branch). Keep the
  Wave-7-rescued-labels no-regression clause.
- **A8 — C2c starts at `BandSongMgr::RankTier`** (`BandSongMgr.cpp:380-401`): one log line
  (song, instrument, rank, returned tier) decides data-vs-widget in one boot; two named probes =
  `std::find(mSongRankings…)` end()-deref garbage tier, and empty `mTierRanges` → −1 → sentinel/
  devil glyph. Devil-scribble = separate atlas sub-finding (RndFont::CellDiff precedent).
- **A9 — C3 fence contradiction repaired:** text renders via the GENERIC mesh path in
  `Rnd_Wgpu_RB3.cpp:5042-5086` (cull None, xfm as-is, no determinant handling) — NOT RB3Quad.
  C3 = **diagnosis-first, read-only** (draw-log the three labels' world xfms + determinants vs
  authored milo xfms); if the fix must touch `Rnd_Wgpu_RB3.cpp`, ESCALATE to the coordinator for a
  declared-range grant (`~:5040-5090`, disjoint from Lane B's `:4360-4735+`); game-side negative
  scale → game-side fix, fence holds.
- **A10 — C-lane ranking pre-authorized:** C1 > C2c > C3 > C2a ≈ C2b > C4, partial return OK.
  R-D: no loader-flag pinning needed (A3 seam is opt-in during the wave); pin SETTLE-FRAMES
  (the W4.1 frame-390 mid-zoom trap).
- **A11 — C1 gate made re-runnable:** percentile rule on the focused-bar ROI — text stroke = p5
  luma, bar field = p60 luma, gate `p60/p5 ≥ 2.0`, calibrated on the retail ref (~3-6:1; current
  ~1.1-1.3:1 → achievable AND fail-red). Same rule on the song-select row + partdiff GUITAR.

---

_(Original draft below, retained for provenance; superseded where the acceptance above differs.)_

## Where we are (entering Wave 12)

Wave 11 (diagnosis-only) named both mechanisms:

1. **BOOTRNG** — the boot-varying state is the **global `gRand` stream POSITION** (~11k draw-count
   spread at the pinned shot): seed is pinned (0x5EED) but consumption ORDER varies per boot via
   async loader completion order. Upstream owner = **W0.3d part-b** (staged since Wave 4). Render
   stack exonerated at value level (per-light VALUE digest + resolved ColorXfm identical across
   boots; preset pick 10/10 deterministic). Free harness lever: wash capture `--tol` 2000→100-150ms
   (songMs Pearson 0.77 = the dominant measurable confound).
2. **Hands (W2.8f)** — palette/skeleton axis EXONERATED (Tier-2 joint-attachment ≤0.33u/2,214
   samples ON the visible-smear frames) and GPU exonerated (wext reproduces in a pure CPU 4-bone
   blend of the same authored verts × weights × uploaded palette). Named axis =
   **authored-vertex-to-offset composition (the mesh SHELL)**: the shell rotates about each joint
   by ΔR while joints stay attached (48.5·sin(87.3°)=48.4u/bone → 95-106u finger chain).

Plus a **fresh user report (2026-07-07), coordinator-verified against retail refs**
(`images/retail-screenshots/`, current captures `/tmp/wave12-current-state/`):

- **U1 focused-text contrast** — hub selected items render pale-yellow-on-gold; retail is
  DARK-on-gold. Wave 7 verified dark-on-gold live post-flip (`RB3_UI_TEXT_FLOOR_STRICT` relaxation,
  `RB3MaterialBinder.cpp:188-205`) → either a Wave 8-11 regression or a different label path than
  the one verified. Same family visible on song-select highlighted row (white-on-yellow vs
  retail's black-on-white) and part-select `GUITAR` (grey-on-gold).
- **U2 song-library instrument-difficulty sidebar** — three defects: (a) the sidebar's dark PANEL
  background is missing (icon/dot grid floats over the venue backdrop); (b) the album-art panel
  OVERLAPS the header row (star count occluded; retail header ends before the panel); (c)
  difficulty ratings show corrupted red devil-head rows on instruments whose real tier is ~4-5
  dots ("25 or 6 to 4" guitar/pro-guitar/drums all-devil = wrong; devil = "impossible" tier only)
  + the devil glyph itself renders as red scribble. Same devil corruption on the part-select
  SONG DIFFICULTY mini-panel.
- **U3 (new, coordinator-spotted)** — bottom action-bar hold-labels render VERTICALLY FLIPPED
  ("HOLD TO MAKE SETLIST" / "HOLD FOR SHORTCUTS" mirrored upside-down) while "VIEW MORE INFO"
  between them is correct.
- **U4** — hub ticker: "NEXT MESSAGE (n/n)" label and message body draw overlapping on one line
  (retail stacks label above message). Known residual from W4.1 (quad fixed, text not).

## Proposed Wave 12 lanes

**Lane A — W0.3d part-b: async-loader completion-order determinism (Opus; loader path, NOT
Rnd_Wgpu_RB3.cpp):**
- **S1:** re-derive the staged Wave-3/4 analysis on today's tree: where async loader completion
  order feeds object-list insertion (single invariant multiset, order-only — the W0.3c finding)
  and where that varies `gRand` CONSUMPTION order. Name the minimal seam.
- **S2:** land the determinism seam flag-first (default-OFF or fixed-clock-scoped per the 0x5EED
  precedent — a determinism SEAM, not de-randomization of retail behavior). Gates (pre-registered):
  PRIMARY = the Wave-11 A.S1 instrument re-run N≥10: `gRand` draw-count spread at the pinned shot
  collapses (~11k → ~0) 10/10 same stream position; SECONDARY = per-boot `mid_sat` spread
  shrinks vs the Wave-10 0.067-0.362 envelope (variance gate, N≥10/arm); REGRESSION = drawlog 792
  canonical + eps sidecar green + lineup PASS + no boot-time regression >5%.
- **S3:** harness lever (small, same lane to avoid a 5th lane): wash/BOOTRNG capture `--tol`
  2000→100-150ms + re-measure the songMs confound share.

**Lane B — W2.8g hands SHELL axis (Opus; dualskin probe region of `Rnd_Wgpu_RB3.cpp` +
`BandCharacter.cpp` read-mostly):**
- **S1:** instrument the authored-vertex→offset composition for a worst-offender finger vertex
  (from the B.S1 instrument's sample list): dump the authored vertex position + its basis
  assumptions, the per-bone offset actually composed, and the same composition DC3/Wii-side
  expectations (what space are `hands_naked.mesh` verts authored in vs what space the offset
  maps from?). The Wave-9 constant ~42-87° ΔR and Wave-10's inv(off)=106.0°-across-members are
  the priors: the offset is baked against the shared magnet — the question S1 answers is what the
  CORRECT vert-space→bone-space map is for these appendage meshes.
- **S2:** fix flag-first (default-OFF). Gates (pre-registered): joint-attachment stays GREEN
  (≤1u); wext on the sighting protocol drops 95-106u → ≤60u (body-coherent range ~50u) WITHOUT
  freezing (worldExt must vary frame-to-frame and track the pose; the Wave-10 freeze trap);
  RealPathFixture; band lineup-gate PASS; no regression on gloves/torso (the W2.8-POSEAWARE
  106→205u distortion trap).
- **STOP-TRIPWIRE:** five bind-side bake classes are dead (static rebake, rigid anchor,
  conjugation, world-space rest, asset rebake) — if S1's named map degenerates into any of them,
  STOP and report; do not land a 6th bake.

**Lane C — W4.3 UI retail-diff parity (Opus planner + Sonnet impl; game-side preferred +
`RB3MaterialBinder.cpp`; fenced OUT of Lane B's Rnd_Wgpu_RB3.cpp regions):**
- **C1 (U1 focused text):** FIRST establish regression-vs-path: A/B `RB3_UI_TEXT_FLOOR_STRICT`
  both arms + `git log` RB3MaterialBinder/text path Wave 8→11 + reproduce the Wave-7 QUICKPLAY
  verification protocol verbatim. If regression: name the commit. If different path: name the
  label/material route the hub items take. Fix flag-first; gate = hub capture shows dark-on-gold
  (pixel-sample the highlight bar text region, luma contrast ratio ≥2:1) + song-select highlight
  row + partdiff GUITAR same check + no regression on the Wave-7 verified screens.
- **C2 (U2 sidebar):** characterization-first, three sub-defects likely distinct: (a) missing
  panel background — draw-log census on song_select: is the panel mesh submitted-and-dropped
  (guard? alpha? heuristic?) or never submitted (game-side milo diff)? (b) header/art overlap —
  layout transform on the art panel vs header (360-ARK 720p layout on Wii-decomp = SYS-5 family;
  compare authored positions in the milo vs drawn quad rects); (c) devil ratings — trace the
  per-instrument tier value from songs.dta → rank→dots mapping (game code) vs what the widget
  receives; separately eyeball the devil glyph atlas UVs. Fixes flag-first game-side.
- **C3 (U3 flipped hold-labels):** the two flipped labels vs the correct center label = a
  per-element V-flip (negative-scale text mesh? UV winding on a specific UILabel variant the
  native path mishandles). Find the transform/UV difference between the three elements in the
  draw log; fix where the asymmetry lives (likely native text-quad path, `RB3Quad`/font path).
- **C4 (U4 ticker):** the label/body share a y — authored anchor/offset lost (same SYS-5 layout
  family as (b)). Diagnose with the same milo-authored-vs-drawn-rect comparison; fix game-side
  (`MainHubPanel.cpp` precedent) if it is a per-panel offset, engine-side only if the anchor math
  is generically wrong.
- Gate for all C fixes: before/after captures reviewed by coordinator eye (E1) + retail-ref
  side-by-side; drawlog 792 flag-OFF unchanged.

**Deferred:** FX/swept-light phase co-sampling probe (needs Lane A landed to be readable — one
wave later is cheaper than fighting the noise floor it removes), WHITE real-lever reframe (same
dependency), 4→8 lights (DC3 gates), W2.4 BandPatchMesh, song_select residuals beyond U2.

## Process rules (carried)

Locks (`/tmp/rb3-git.lock`, `/tmp/milo-engine-git.lock`, `/tmp/rb3-native-build.lock`,
`/tmp/milo-engine-classjson.lock` append-only + single coordinator regen), checkpoints
(`/tmp/wave12-checkpoints/<stage>.json`, check-first/write-before-return), commit-per-review-cycle,
PLAN.md/STATUS.md per item under `execution/<KEY>/`, agents use own build dirs, NO pin bumps or
default flips by lanes (coordinator-gated), all refuted-experiment flags UNSET in all arms. Six
defaults ON. Leave the uncommitted FxSendNative.cpp audio edit untouched.

## Risks / open questions for the reviewer

- **R-A:** Lane A scope — the loader seam touches boot-critical code shared with the web build.
  Is fixed-clock-scoped (determinism seam) the right default shape, or default-ON (the SortDraws
  tie-break precedent shipped default-ON with opt-out)? What's the fail-red proof that the seam
  actually pins CONSUMPTION order (not just completion order)?
- **R-B:** Lane B S1's "what space are the verts authored in" — is this answerable from the
  native reader + DC3 reference without Wii ground truth? Which DC3 source files are the
  composition oracle?
- **R-C:** Lane C is 4 sub-items — too fat for one lane? C1 is small; C2 may be three separate
  bugs. Split proposal welcome. Also: C1's "regression" prior — is there a cheaper first probe
  (e.g. the Wave-7 verification screenshot still in a STATUS.md to diff against)?
- **R-D:** Any Lane A×C collision via LoadMgr (C2's panel could be a load-order casualty that
  Lane A changes) — should Lane C captures pin the loader flag OFF for stability?
