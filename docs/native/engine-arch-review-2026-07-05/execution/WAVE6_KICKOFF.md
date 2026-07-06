# Wave 6 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE6_REVIEW.md`) — **all amendments adopted**;
dispatched with the corrected shape below.

## COORDINATOR ACCEPTANCE (2026-07-06) — final dispatched shape

Fable review returned **dispatch-with-amendments** (8). The key catch (A3, verified in source): the
kickoff's prime suspect is **mechanically impossible** — the bloom-halo capture fires only under
`strcmp(cam, "game.cam")==0` (`Rnd_Wgpu_RB3.cpp:4403`) and the A/B pins a *venue* cam, so crowd
draws cannot enter the halo capture; W2.1.S2's "emissive feeds bloom" sentence described a
mid-development bug fixed before commit. Adopted changes:

- **A1 (protocol power):** binary wash scoring at N=8/state is underpowered (Fisher needs 4/8-vs-0/8
  for p<0.05; ~11% power at a true 25% ON-rate) and S2's "indistinguishable → pre-existing" would
  convert low power into a ship decision. → **Continuous luma scoring + Mann-Whitney, interleaved
  sequential design** (early-stop on a wash-in-OFF existence proof; cap N=16/state). **Score the
  existing evidence first:** W2.1's own verify run recorded heavy wash flag-OFF + clean flag-ON
  (`W2.1/STATUS.md:304-311`), and the Wave-5 package's OFF_2 is near-black (luma 23.7) — the
  asymmetry premise is already shaky.
- **A2 (detector):** captures sleep on wall-clock post-`is_playing`, so songMs varies and authored
  venue lighting legitimately moves luma. → **Pin captures to a songMs window; score BOTH tails**
  (blow-out and near-black) **+ a pink-hue fraction** (pink = broken-env class per W0.5; white =
  exposure/bloom — possibly two distinct phenomena).
- **A3 (suspect demoted):** S3 ranks alternatives instead — capture-timing/songMs, async asset
  residency (the W0.3d part-(b) async patch is still unlanded), `RB3PostProc` venue grade/bloom,
  P4 per-environ venue-light rewrite — via a cheap **4-flag isolation matrix**
  (`RB3_HIGHWAY_BLOOM_OFF` / `RB3_BLOOM_OFF` / `RB3_VENUE_LIGHT_OFF` / `RB3_TRACK_LIGHT_OFF`) that
  names the mechanism faster than statistics.
- **A4 (R-A answered):** an A/A-variable verdict **unblocks the flip** — E1 is then judged on
  detector-selected wash-free captures, and the wash gets its own backlog item carrying the
  attribution data.
- **A5 (flip checklist additions):** run **one flag-ON canonical drawlog** pre-flip to confirm the
  expected re-golden count (792 was measured pre-W0.3d-fix); update the
  `RB3_PLACEMENT_CONTRACT`/`_OFF` classification rows (they say "pending flip" and go stale); sweep
  harnesses/docs whose OFF arm is "no env" — post-flip the OFF arm becomes
  `RB3_PLACEMENT_CONTRACT_OFF=1` (semantics inversion).
- **A6 (Lane B pre-declared prototype-only):** overlap is near-certain, not speculative —
  per-environ lighting is written only in `WriteSceneUniforms`, and the WGSL include +
  `UniformStructs.h` are the DC3 cross-backend contract Lane A's W3.1b touches this wave. No
  conditional landing.
- **A7 (Lane C file fence):** `rb3/native/src/rb3_render_hook.cpp` hosts name-based render policy
  (incl. `QueryHaloPolicy`) and is where a Lane-A S3 fix would land → **Lane C fixes in band3/UI
  game code only; rb3_render_hook.cpp is FORBIDDEN to Lane C.** `/tmp/visdiff-20260702` still
  exists (verified) but re-derivation stays mandatory.
- **A8 (hygiene):** planner commits go under `flock /tmp/rb3-git.lock` (commit-per-review-cycle
  makes them frequent); corrected stale anchors — fog fill is at `:1444-1457` (not `:1429/:1431`),
  `projLight` is only implicitly zeroed via `SceneUniforms s{}` at `:1176`; R-B's concrete gate =
  the documented `RB3_HIGHWAY_BLOOM_BLEND=0` negative control as the machine-checkable
  gem-halo-present assertion.

**Coordinator addition — Lane D (new, from the current-state screenshot review, 2026-07-06):** a
parallel Sonnet capture of HEAD (`/tmp/wave6-current-state/`) confirmed the user's "not everything
has landed" report with three findings the backlog didn't cover: **(1) part_difficulty screen draws
no part/diff widgets** + solid-black venue poster quads (`partdiff_default.png`, frame-settle
recapture needed to rule out mid-transition); **(2) fully-grayscale venue at songMs≈3015** with
color returning by ≈20153 (`gameplay_default_1.png`; authored B&W camera treatment vs the P4
venue-light grey-fallback misfiring — mechanism-space shared with the wash investigation); **(3)
singer's head renders flat black** while the eye submesh draws and the body is textured
(`gameplay_default_2.png`, songMs≈20153 — the C8-faces family, reproduces flag-ON/OFF identically).
→ **W2.7 (black head) + W3.3 (grayscale venue) = Lane D, characterization-first** (diagnosis with
flag matrices + Dolphin ground truth; fixes land only if game-side/file-disjoint from Lane A —
engine-file fixes are staged as patches + backlog for Wave 7). Finding (1) folds into Lane C as a
third UI subitem.

**Final dispatched shape:** **Lane A** (sequential): W2.1-flip-blocker S1(evidence-first
protocol)→S2(sequential measure)→S3(isolation matrix + conditional fix)→S4(package) → *coordinator
E1 + flip + re-golden (post-workflow)* → W3.1b tail; **Lane B**: W3.2 BoxMap plan+worktree-prototype
ONLY; **Lane C**: W4.1 UI parity (ticker quad, song_select overlap, part_difficulty widgets;
game-side only); **Lane D**: W2.7 + W3.3 characterization-first. Commit-per-review-cycle under
`flock /tmp/rb3-git.lock`; classification.json flock'd append-only, coordinator regens once.

---

_(Original draft below, retained for provenance; superseded where the acceptance above differs.)_

**Draft status:** DRAFT — under Fable pre-dispatch review, not yet dispatched.
Parent: `REFACTOR_PLAN.md` (Phase 2 flip, Phase 3 lighting, Phase 4 UI), `execution/README.md`
(Wave 1–5 results + hard rules 1–8 + standing pre-dispatch review gate). Engine pin `8e7eddd`.

## Where we are (entering Wave 6)

Wave 5 made the crowd/drum flip mechanically ready (one-line `kPlacementContractDefaultOn`,
opt-out-first read, drum oracle, Dolphin A/B package) — and the coordinator **HELD the flip** at the
E1 visual gate: `cap_ON_1` blows out nearly fully white while the other three captures render
normally (1/2 flag-ON, 0/2 flag-OFF). Per the package's own checklist item 3, a wash appearing in
only one flag state is a new finding. W2.1.S2's STATUS names the candidate mechanism: clamped/moved
crowd **emissive geometry feeding the bloom pass** into a full-screen wash. Separately: draw order
is now deterministic (W0.3d-fix — the re-golden prerequisite), fog fill is landed default-OFF but
asset-blocked from visual verification, and the flag registry is clean (321, zero game-root Unknown).

**The user has visually inspected the game and reports "not everything has landed."** Expected in
part — the placement contract is still default-OFF pending the flip — but a parallel current-state
screenshot capture (`/tmp/wave6-current-state/`) is running; the coordinator will review it against
the wave's claims and file discrepancies as backlog items.

## Proposed Wave 6 lanes

**Lane A — UNBLOCK + SHIP the crowd/drum flip, then lighting tail (sequential; engine
`Rnd_Wgpu_RB3.cpp` single-writer):**

- **W2.1-flip-blocker** (LEADS the wave — gates the flip):
  - **S1 (Plan, Opus):** characterization protocol. Deliverable: a scripted capture harness giving
    N≥8 camera-pinned gameplay captures **per flag state** (same venue/shot as the Wave-5 package),
    plus a numeric wash detector (mean-luma / % pixels > threshold) so "blown out" is machine-scored,
    not eyeballed across 16+ PNGs.
  - **S2 (Measure, Opus):** run the protocol. Decide: **(i) A/A-variable** (wash rate statistically
    indistinguishable across flag states → pre-existing, flip-independent) or **(ii)
    flag-ON-specific** (wash concentrated in flag-ON). Checkpoint the verdict + per-capture scores.
  - **S3 (Fix, Opus — only if flag-ON-specific):** root-cause and fix the emissive-feeds-bloom path.
    Prime suspect: the bloom-halo capture-and-replay (`IsHaloSourceMat` / per-draw emissive capture in
    `DrawMesh`, replay in `EndFrame`) now sees crowd emissive draws at their *real* placements under
    the contract. Candidate fixes (planner decides): exclude crowd-instance materials from halo
    capture; clamp halo source luma; or bound the capture to the gem/now-bar material set it was
    designed for. Constraints: **must not alter the vertex-invariance proof** (placement oracle stays
    GREEN), flag-OFF stays byte-identical, fail-red demonstrated (disable the fix → wash reproduces).
    If the wash is A/A-variable instead, S3 becomes a **wash-rate note** in the package (no code).
  - **S4 (Package, Sonnet):** fresh Dolphin A/B sign-off package (≥4 captures per flag state + the
    numeric wash scores) → **coordinator human-eyes E1 sign-off → coordinator flips
    `kPlacementContractDefaultOn` 0→1 + ONE re-golden** (`splash_screen.json` 888→792 + fresh N≥30
    per-name-eps sidecar) → ≥15/15 canonical green under the new default = the ship exit.
- **W3.1b (Lane A tail — same file `WriteSceneUniforms`):** land `projLight` from environ fakespots
  (struct-neutral, fields exist and are hard-zeroed at `:1429/:1431`); build a fog-authoring test
  venue or synthetic `RndEnviron` (all 34 boot-reachable environs have `FogEnable()==false`) and
  visually verify `RB3_ENV_FOG` renders. Default-OFF both. DC3 zero-blast gate as W3.1a
  (`UniformStructs.h` + `standard_wgsl.inc` diff empty, `static_assert 656` untouched,
  `milo-engine-tests` green).

**Lane B — W3.2 BoxMapLighting (SYS-4 redesign; engine, parallel only if file-disjoint):**
- **S1 (Plan, Opus):** map the current inverted-lighting path (where BoxMap/environ lighting is
  approximated or sign-flipped), measure **file overlap with Lane A** (`Rnd_Wgpu_RB3.cpp`,
  `WriteSceneUniforms`, WGSL includes). If it must edit Lane-A files, the lane STOPS at
  plan+worktree-prototype this wave (no mainline engine edits) and lands in Wave 7 after Lane A
  settles.
- **S2 (Prototype, Opus):** faithful per-environ box-map/ambient lighting behind a default-OFF flag,
  prototyped in a worktree, with a before/after venue capture set + lineup gate.

**Lane C — Phase 4 UI parity opener (rb3 game-side; parallel, file-disjoint from A/B):**
- **W4.1 (Plan Opus → impl Sonnet):** open the SYS-5 (360-assets-on-Wii-engine) UI family with the
  two highest-confidence 2026-07-02 visual-diff findings: **(a) main_hub grey ticker quad** (HIGH —
  a placeholder/untextured quad retail doesn't show) and **(b) the song_select overlap family**
  (stale-slot/overlapping text known from the MusicLibrary Text-class issue). Ground truth:
  `images/retail-screenshots/` (+ Dolphin). Each fix: smallest-scope, default-ON only if
  provably-safe-per-lineup-gate, else default-OFF flag + registered. Exit: before/after captures on
  main_hub + song_select, no regression on the other screen (A/A protocol).

**Deferred (unchanged):** 4→8 light arrays (needs `projLight` landed + a DC3-side visual gate; the
656→~1024 cross-backend contract change stays Wave 7+), W2.4 BandPatchMesh decision, W2.6 foot/shoe
lower-body rest-capture.

## Process rules this wave (additions)

- **Commit-per-review-cycle (user directive):** every stage that lands file changes **commits
  immediately after its verify passes** — MOVE-xor-CHANGE, own files only, HARD RULES 7–8 apply
  (never reset/rebase/checkout-- on shared trees; never touch sibling lanes' lines). Planners commit
  `PLAN.md` as soon as written. No end-of-wave commit pileups.
- **Checkpoint-before-return** (CLAUDE.md): every stage writes its structured result JSON to
  `$CLAUDE_JOB_DIR/tmp/wave6/<stage>.json` before returning and short-circuits on a valid existing
  checkpoint.
- **`classification.json` single-writer:** any new flags registered under
  `flock /tmp/milo-engine-classjson.lock`, append-only, NO `gen.inc` regen by lanes — coordinator
  does ONE regen at wave end.
- **Coordinator-only actions:** the flip commit, the re-golden, the pin bump, the E1 sign-off.

## Risks / open questions for the reviewer

- **R-A (the flip's failure semantics):** if S2 says A/A-variable (wash is pre-existing), is a flip
  with a *known intermittent pre-existing wash* acceptable to ship, given the Wave-5 hold was based
  on asymmetry? Proposed answer: yes — the hold reason evaporates if asymmetry disproves; the wash
  becomes its own backlog item independent of the flip.
- **R-B (S3 scope creep):** the bloom/halo path was itself a landed feature (A2/A3/A4 glow,
  default-ON). An S3 fix must not regress gem/now-bar halos — what's the concrete gate? Proposed:
  the existing bloom A/B captures + a gem-halo-present assertion on a gameplay capture.
- **R-C (Lane B file overlap):** is stopping Lane B at plan+prototype too conservative if S1 finds
  it only touches WGSL includes + a new file? Reviewer may loosen to "landable if file-disjoint
  proven."
- **R-D (Lane C ground-truth freshness):** `/tmp/visdiff-20260702` may be gone; the lane must
  re-derive both findings against `images/retail-screenshots/` before fixing, not trust the memory
  summary.
- **R-E (re-golden count):** 888→792 was measured pre-W0.3d-fix; verify the expected count against
  the current deterministic order before treating a different number as failure.
