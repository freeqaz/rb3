# W3.2 — STATUS

## B.S1 (BoxMapLighting PLAN, Opus) — 2026-07-06 — done

**Deliverable:** `PLAN.md` (this dir) — prototype-only lane per WAVE6_REVIEW A6.

**What landed:**
- File:line inventory of the SYS-4 approx/box-ambient approximations + the real/approx routing
  inversion (`Rnd_Wgpu_RB3.cpp:1312-1434` scalar-ambient/approx-as-Lambert; `Env.cpp:172`
  faithful split; `Mesh.cpp:1463-1480` per-object granularity; `BoxMap.cpp` ground-truth algorithm).
- Faithful-replacement design grounded in the Wii box-ambient source (`BoxMapLighting::ApplyLight`
  cube accumulation `BoxMap.cpp:122-199`; per-vertex sample form flagged as the top correctness risk
  R1, to be Dolphin-validated in S2).
- Prototype build strategy **validated**: engine worktree `/tmp/wave6-boxmap-wt`
  (branch `wave6-boxmap-proto`) created, **HEAD `8e7eddd` == pin** (confirmed);
  scratch build via `-DMILO_ENGINE_PATH=/tmp/wave6-boxmap-wt` into `/tmp/wave6-boxmap-build`
  (pin check is `message(WARNING)` only, non-fatal — `native/CMakeLists.txt:76-86`);
  `BoxMapLighting` already links into `rb3-native` (`BoxMap.cpp.o` present) so no new CMake wiring.
- Visual gate design: G-A Dolphin matched-frame venue A/B (correctness, blocking), G-B songMs-pinned
  before/after venue captures (both luma tails + pink-hue), G-C lineup + drawlog no-regression
  (flag-OFF byte-identical), G-D fail-red anti-blindness.

**Constraints honored:** no engine mainline edits; no flag flips; no pin bump; worktree add does not
touch the engine mainline working tree.

**Handoff to S2 (Wave 7):** implement `RB3_BOX_AMBIENT` in the worktree, run G-A..G-D, rebase onto
post-Lane-A HEAD before any land. Key open risks: R1 (per-vertex cube weighting), R2 (656→752 struct
growth crosses DC3 contract), R3 (per-environ vs per-object granularity), R5 (fakespot double-count).

**Artifacts:**
- `docs/native/engine-arch-review-2026-07-05/execution/W3.2/PLAN.md`
- engine worktree `/tmp/wave6-boxmap-wt` (branch `wave6-boxmap-proto`, HEAD 8e7eddd)
- checkpoint `/tmp/wave6-checkpoints/B-S1.json`

## B.S2 (BoxMapLighting PROTOTYPE, Opus) — 2026-07-06 — done (prototype landed on branch; assessment = design partially REFUTED on real data)

**Deliverable:** working `RB3_BOX_AMBIENT` prototype in the engine worktree, built + captured + gated.
Prototype-only per WAVE6_REVIEW A6 — **no engine mainline code edits, no flag flips, no pin bump.**

### Branch / build
- **Engine branch `wave6-boxmap-proto` HEAD `5efa3219`** (parent `8e7eddd` == pin), worktree `/tmp/wave6-boxmap-wt`.
  3 files: `src/gfx/UniformStructs.h`, `src/gfx/standard_wgsl.inc`, `src/platform/Rnd_Wgpu_RB3.cpp`.
- Flag `RB3_BOX_AMBIENT` **registered in mainline `NativeCompatFlags.classification.json`** (append-only
  under `flock /tmp/milo-engine-classjson.lock`, `class=feature`, `not-live: W3.2 prototype`), left
  **uncommitted** for the coordinator's single wave-end `gen.inc` regen (per protocol).
- Scratch build `/tmp/wave6-boxmap-build` (`-DMILO_ENGINE_PATH=/tmp/wave6-boxmap-wt`): **rb3-native
  links clean.** `static_assert(sizeof(SceneUniforms)==752)` passed (656→752, `boxAmbient[6][4]` +
  `boxAmbientEnabled` repurposed from a proj-pad slot). `BoxMapLighting` links via `rndobj/BoxMap.cpp.o`
  (no new CMake wiring, as S1 predicted).

### What the prototype does
Behind `RB3_BOX_AMBIENT`, the venue path (`world.cam`) feeds the current `RndEnviron`'s `mLightsApprox`
set through the real Wii `BoxMapLighting` kernel and uploads a 6-axis directional-ambient cube
(`boxAmbient[6]`, axis order `{+X,-X,+Y,-Y,+Z,-Z}`). The shader samples it per-normal
(`boxAmbientSample = Σ max(0, N·axis)·cube[i]`) instead of the single scalar `ambientColor`. Approx
directional/fakespot lights → cube (`ApplyQueuedLights(cube, viewPos=null)` = directional/fakespot
only); approx point lights keep the existing per-pixel Lambert path (PLAN §2c per-environ deviation).
Cube base = the clamped/floored env ambient so the shadow side never crushes. flag-OFF leaves
`boxAmbientEnabled=0` → shader `select()` takes the scalar path **byte-identical**.

### Gates
- **G-C no-regression — PASS.** `lineup-gate.py --bin /tmp/wave6-boxmap-build/rb3-native`:
  **flag-OFF** = `verdict=PASS img/segA/ratioB/countC/pin` all PASS against the committed golden
  (scratch binary renders identically → flag-OFF byte-identical confirmed); **flag-ON**
  (`RB3_BOX_AMBIENT=1`) = `verdict=PASS` all layers (no geometry/shard regression).
- **G-B before/after venue A/B** — `scripts/native/_w32-boxambient-ab.py`, 2×OFF + 2×ON at
  songMs≈21.4k (`coop_dir_crowd` wide venue shot). Scores (`captures/scores.json`,
  montage `captures/boxambient_off_vs_on.png`):
  | cap | mean luma | blow%>0.95 | crush%<0.05 | pink% |
  |---|---|---|---|---|
  | OFF_1 | 20.7 | 0.35 | 66.4 | 0.01 |
  | OFF_2 | 24.7 | 0.34 | 50.2 | 0.45 |
  | ON_1  | 22.1 | 0.35 | 64.1 | 0.07 |
  | ON_2  | 22.6 | 0.34 | 66.3 | 0.01 |
  **flag-ON sits INSIDE the OFF A/A variance band** (luma 20.7–24.7), **no blow-out, no pink wash.**
  The prototype is safe; the visible effect at this shot is a subtle direction-shaped brightening.

### KEY FINDING — the §2c per-environ directional-cube is a near-no-op on real RB3 venue data
`RB3_VENUE_PROBE` across **23 boot-reachable environs**: `mLightsApprox` = **27 point (type 0) + 1
fakespot (type 2), ZERO directional (type 1)** (theater/cityscape/street/train/rooftop coloured
stage-spots — all points). The cube is built directional/fakespot-only, so it collects **essentially
no energy** and degenerates to the base ambient; `boxAmbientSample` then ≈ `base·(|Nx|+|Ny|+|Nz|)`
(1–1.73× the flat base) — hence ON≈OFF. The SYS-4 "approx-as-Lambert routing **inversion**" barely
bites in practice because the current code already routes those **point** lights to the **per-pixel
Lambert** path — which is arguably *more* accurate than folding them into a 6-axis ambient cube.

### Is the design right? (honest)
- **Mechanism/wiring: YES.** Builds, links the real `BoxMapLighting`, flag-OFF byte-identical, both
  gates PASS. The uniform-contract + shader plumbing is correct and reusable.
- **Value on real data: the cheap per-environ directional-only path (PLAN §2c) is REFUTED.** RB3
  authored its venue approx lighting as coloured **point-spots**, not directionals, so the directional
  cube has nothing to shape. The genuine Wii-vs-native fidelity gap is on the **point** lights: Wii
  folds them into a **per-object** ambient cube (position-dependent, `BoxMap.cpp` point path), whereas
  both legacy and this prototype use per-pixel Lambert. That is a subtler question, **not** the
  "flat-grey ambient" fix SYS-4 was framed as.
- Two correct secondary behaviors demonstrated: flag-ON drops the synthetic grey-key fallback for
  no-light envs (faithful — Wii adds no synthetic key); base-fill makes ambient direction-shaped.
  **Known deviation:** base-into-every-face over-counts up to 1.73× flat ambient — Wave 7 should
  normalize so flag-OFF and a zero-light flag-ON match at base ambient.

### What lands in Wave 7
1. **Rebase the branch onto post-Lane-A engine HEAD** — W3.1b (projLight/fog) edits the same 3 files.
2. **Decision gate, not just implementation:** either (a) escalate to a **per-draw, point-inclusive**
   cube (PLAN §2c) using the W1.6 `RB3DrawContext` object position — the only path that actually moves
   RB3's point-spot-authored venues — or (b) **explicitly decide** that per-pixel Lambert for RB3's
   point-spots is the right native model and the 656→752 struct growth + cube is **not worth it** for
   RB3. This prototype provides the evidence for that call: on current data, (b) is defensible.
3. Normalize the base-fill; validate the per-vertex sample weighting vs Dolphin (R1, now low-priority).

### DC3 blast radius
The struct growth **656→752 crosses the shared `UniformStructs.h` + `standard_wgsl.inc` + the DC3
`static_assert(656)`** — this is a **non-zero-blast** change (unlike W3.1a's fog fill). In the prototype
(worktree, rb3-only build) DC3 is untouched. For the Wave-7 LAND, bumping DC3's engine pin to that HEAD
would fail `static_assert(sizeof(SceneUniforms)==656)` → DC3's `UniformStructs.h` + WGSL struct MUST
mirror the +96 field in lockstep. DC3's `WgpuRnd::WriteSceneUniforms` need NOT write `boxAmbient`
(zero-init + `boxAmbientEnabled=0` → shader `select()` keeps its legacy scalar path, zero behavior
change), so DC3 gets the field for free — but the assert + WGSL must move. **Coordinator-sequenced with
the DC3 owner.** (If Wave-7 decision (b) wins, the struct never grows and DC3 blast is zero.)

**Artifacts:**
- engine branch `wave6-boxmap-proto` @ `5efa3219` (worktree `/tmp/wave6-boxmap-wt`, scratch build `/tmp/wave6-boxmap-build`)
- `scripts/native/_w32-boxambient-ab.py` (before/after harness + numeric wash/luma scorer)
- `docs/native/engine-arch-review-2026-07-05/execution/W3.2/captures/` (4 PNGs + montage + `scores.json`)
- `RB3_BOX_AMBIENT` row in mainline `NativeCompatFlags.classification.json` (uncommitted → coordinator regen)
- checkpoint `/tmp/wave6-checkpoints/B-S2.json`

## D.S1 (W3.2b point-light escalate-or-drop DECISION MEMO, Opus, plan-only) — 2026-07-06 — done

**Deliverable:** `DECISION.md` (this dir) — decision from EXISTING evidence only (WAVE7_REVIEW A9;
no Dolphin harness built).

**RECOMMENDATION: (iii) DROP.** Do not land `RB3_BOX_AMBIENT` (option i), do not redesign around a
per-object point-light cube (option ii). Keep per-pixel Lambert as the native model; preserve the
`wave6-boxmap-proto` branch + registered flag on the shelf; do NOT grow `SceneUniforms` 656→752
(DC3 blast stays zero). Re-open only on a human-captured, point-spot-dominant venue that measurably
beats Lambert.

**Evidence chain (all pre-existing):**
- E1 — 23-environ probe: 27 point + 1 fakespot + ZERO directional → the prototype's directional cube
  collects near-zero energy (near-no-op). SYS-4 "flat-grey ambient" premise refuted.
- E2 — A/B montage: ON = warm base-fill over-brightening (1.0–1.73×), a deviation not a fidelity
  gain; luma inside OFF A/A band but chroma warms.
- E3 — Dolphin gp_00/gp_b07: RB3 venue art is deliberately dim/high-contrast silhouette; per-pixel
  Lambert (montage OFF) already matches it; the cube's base-fill would wash *away* from ground truth.
- E4 — band members lit by their OWN char rig (mLightsReal), not venue mLightsApprox → cube barely
  touches the user-visible characters.
- E5 — mechanism: Wii point→cube is a per-object single-sample ambient approximation; per-pixel
  Lambert is strictly higher spatial fidelity → per-pixel Lambert is arguably *more* accurate.
- E6 — struct growth 656→752 crosses the shared `UniformStructs.h` DC3 `static_assert` → option (i)
  is NOT harmless plumbing (non-zero DC3 blast + the E2 behavior change).

**Option (ii) blast radius (documented for the record, if ever escalated):** per-draw box eval in
`DrawMesh` (obj.world via W1.6 RB3DrawContext) + struct/WGSL growth + DC3 lockstep + unvalidated
per-vertex sample weighting (R1, no Dolphin harness). Large multi-repo change to replace a
higher-fidelity path — NO-GO now.

**Human-capture request filed in DECISION.md §5** (only if escalating): 3 matched-frame Dolphin
venue captures (warm-spot theater / cool-spot arena / mixed club) at band-visible framing + matched
native Lambert frame + RB3_VENUE_PROBE dump each. Existing shots (one venue family, char-contact
framing) are insufficient.

**Constraints honored:** plan-only, no engine edits, no flag flips, no pin bump. Re-baseline note
(WAVE7_REVIEW A8) acknowledged but no new captures taken (decision made from existing evidence).

**Artifacts:**
- `docs/native/engine-arch-review-2026-07-05/execution/W3.2/DECISION.md`
- checkpoint `/tmp/wave7-checkpoints/D-S1.json`
