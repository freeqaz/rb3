# Wave 3 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE3_REVIEW.md`) — **all 9 amendments adopted**; dispatched with the corrected shape below.

## COORDINATOR ACCEPTANCE (2026-07-06) — final dispatched shape

Fable review `WAVE3_REVIEW.md` returned **dispatch-with-amendments**; all adopted. Net changes vs the draft below:

- **W2.2 gate premise was wrong (A2):** W0.1 skin golden + W0.4 effector golden run in the **DC3-context** `milo-engine-tests` and never execute rb3's band rebind path — they are **invariance nets only** (a red there = W2.2 leaked into shared skinning math → auto-stop), NOT W2.2's correctness gate. W2.2 therefore **builds its own gates first (S1)**.
- **W2.2 four-layer anti-revert exit (B1/B2):** (1) new `rb3-tests` **bind-pose identity gtest** through the real band load path — at the captured bind frame `offset′·perMemberBoneBindWorld ≈ identity` and skinned verts ≈ authored decoded verts, **fail-red proven** (the numeric invariant BandPatchMesh never had) + a hand-closeup capture harness **including the count-in/walkon window**; (2) numeric draw-time: `SKINPOS ≤ ~65u` on ALL rebound meshes *incl. hands/fingers/hair*, `FLING(>120u)=0`, shard-ratio `≤2×`, crowd/extras clamp byte-identical; (3) W0.5 lineup + **reviewer-judged wide AND hand-closeup** frames vs fresh Dolphin t2 captures + `images/retail-screenshots/`; (4) land **default-OFF** behind a new registered flag, **flip default in a separate one-line commit** gated on layers 1-3 — torso-only `RebindOutfitBonesToOwnSkeleton` retained as opt-back (default-*replaces*, not removes). Worst case = an unflipped flag, never a blind revert. **Tripwire:** the prior `RB3_BOUND_REBAKE` rest-pose rebake already FAILED with a 200-460u smear (`Rnd_Wgpu_RB3.cpp:3714-3725`); W2.2's thesis is the untried **bind-pose-captured-at-load** rebake — if that capture seam is unreachable, S1 stops at "characterize + propose oracle," does not land a blind bind change.
- **W2.2 engine READ-ONLY (D1):** use existing `mNativeBonesRebound` skip-seams + `RB3_GUARD_EXEMPT_REBOUND=1`; no `DrawMesh` edits (would collide with Lane A / W1.6). Shard-guard default-flip is a Wave-4 coordinator one-liner.
- **W0.3c timebox + Exit B (C1/C2):** ~2 sessions diagnosis. **STRIKE `setarch -R` fallback** (already refuted — the 15-run flake sweep ran under it). **Exit A** = root-cause fixed, golden green **15/15** boots. **Exit B** (timebox expired) = land an **order-insensitive canonical multiset comparator** in `drawlog-golden.py` (keyed by mesh-name/pipeline/blend/counts/world-xfm; keeps exact-count, bind-group-collapse, and fail-red checks) — deterministic, so W1.6 may launch on **A or B**. File the order root-cause as W0.3d. CharEyes/CharLookAt determinism is a stretch (deletes the old sidecar), **not** W0.3c's exit.
- **W3.1 DEFERRED to Wave 4 (E1):** its edit surface (fog + 4→8 light arrays → `SceneUniforms` size) is *inside* `WriteSceneUniforms`/`mSceneBindGroup` — the exact region W1.6 rewrites — and Phase 3 is blocked on the W0.3 golden per `REFACTOR_PLAN.md:123`. Concentrate wave review budget on the two hard items.
- **W2.5 fail-red (E2):** demonstrate the assert/log fires on a synthetic unresolvable waypoint `targName`, not just silence on good data.
- **Hygiene (D2):** every lane PLAN.md enumerates exact files; coordinator cross-diffs before dispatch; **no Wave-3 item touches `src/App.cpp`** (entangled with a sibling agent + W0.3b seams); new flags registered in the W0.6 registry at introduction (`census check` exit 0). Correct the draft's `Skeleton_Native.cpp` misattribution — it's the Kinect/COCO provider, not the char-load path.

**Final dispatched shape:** Lane A (engine, sequential) `W0.3c → W1.6`; Lane B (rb3-only, staged S1→S4) `W2.2`; Lane C (tiny, rb3) `W2.5`. W1.6 launches iff W0.3c reached Exit A or Exit B.

---

_(Original draft below, retained for provenance; superseded where the acceptance above differs.)_

**Draft status:** DRAFT — under Fable review, not yet dispatched.
Parent: `REFACTOR_PLAN.md` (Phases 2–3), `ARCHITECTURE_REVIEW.md` (SYS-1..7), `execution/README.md`
(Wave 1+2 results, hard rules 1–8).

## Where we are (entering Wave 3)

- Phase 0 nets green (skin golden W0.1, effector golden W0.4, non-blind lineup W0.5, loud stubs,
  flag registry). Phase 1 monolith decomposition **done** (`Rnd_Wgpu_RB3.cpp` 7,017→4,747; six TUs;
  all 13 asset-name branches relocated to the game hook — SYS-2 structurally addressed). Engine-test
  suite 198/0. Engine pin `41b9e3a`.
- **Open blockers:** W0.3b is partial — draw *count* is deterministic (888×) but draw *submission
  order* is ~33% flaky run-to-run (mesh-identity swaps; byte-identical binaries disagree). This is a
  live SYS-3 instance. It blocks the draw-log golden from being a hard gate, which blocks **W1.6**
  (the DrawContext state-leak fix), which the plan wanted before Phase-2 placement work.

## The core sequencing question this wave answers

Two headline bug families the user most wants fixed: **(1) meshes deforming wrong (hands/fingers)**
and **(2) wrong placement (crowd/drum all at one point)**. Per the bug→cause map:
- Hands/fingers = SYS-1 **bind** fault (wrong skeleton + wrong-basis `invBind`). Lives in the
  **char/skeleton load path**, gated by the *deterministic* skin + effector goldens.
- Crowd/drum placement = SYS-1 **placement** fault (`obj.world=identity` for skinned meshes). Lives
  in **`DrawMesh`**, which W1.6 is about to restructure.

So (1) is unblocked NOW (its gates are green and it doesn't touch `DrawMesh`); (2) should wait for
W1.6 (don't fight the DrawContext refactor over the same code). That gives the wave shape below.

## Proposed Wave 3 lanes

**Lane A — DrawMesh chain (sequential, engine backend):**
- **W0.3c** — root-cause + fix the draw-submission-order nondeterminism. Leads: pointer-keyed
  `std::unordered_map<RndMesh*,...> sMeshGpu`/`sGeomSyncGen` iteration order; uninitialized
  `CharEyes`/`CharLookAt` look-at state. Exit: `--fixed-clock` draw-log golden green across ≥15
  fresh boots (not 3), `--fail-red-audit` still red. Turns the draw-log into a real gate.
- **W1.6** — replace mutable `mSceneBindGroup` with an immutable-per-write scene binding threaded via
  a `DrawContext` value object. Gated by the now-green draw-log golden + lineup + screenshot at 2
  scenes. Byte-identical. This is the SYS-3 fix and the enabler for clean Phase-2 placement work.

**Lane B — hands/fingers bind fix (parallel, load path):**
- **W2.2** — rebind outfit + appendage skin meshes to the per-member animated `skeleton_unshared`
  AND rebake `invBind` against the per-member bind pose (`offset' = meshBindWorld ·
  inverse(perMemberBoneBindWorld)`), captured at skeleton-load/first-pose. Removes the H2 basis
  mismatch, unlocks full-body (hands/fingers) rebind — supersedes the torso-only
  `RebindOutfitBonesToOwnSkeleton` workaround.
- **CRITICAL nuance for the planner:** the W0.1 skin golden proves *faithful-to-decomp-reference*,
  and the W0.4 effector golden asserts against values *captured from the current (buggy) build*. A
  bind change makes both goldens move. So the item MUST first establish **independent ground truth**
  that the corrected deformation is *right* (Dolphin capture and/or `images/retail-screenshots/`
  hand poses), then re-capture the goldens to corrected values *with that justification recorded* —
  never re-baseline to merely "different." The W0.5 patch-bearing lineup (no shards) is the visual
  backstop.

**Lane C — cheap independent wins (parallel):**
- **W3.1** — faithful lighting fills (additive, extension not redesign): wire fog from `RndEnviron`
  (`FogEnable/GetFogStart/End/FogColor`), bump directional+point light arrays 4→8 (GX cap), populate
  `projLight` from environ fakespots. No hack deleted; fills real gaps. Gated: lineup + no-regression.
- **W2.5** — game-side `BandConfiguration::SyncPlayMode` assert that every waypoint `targName`
  resolves to a `BandCharacter`; log misses. Fixes "only some members placed" (independent data gap).

**W1.6 gate:** only launches if W0.3c verified green AND Lane A reached it. Else defer to Wave 4
(same as W1.6 deferred this wave).

## Risks / open questions for the reviewer

- **R-A: Is starting W2.2 (Phase-2 correctness) before W1.6 lands a violation of "nets first, then
  legible code, then correctness"?** My read: no — the *nets* (the thing the rule protects) are
  green, and W2.2 is in the load path, not the un-refactored `DrawMesh`. But I want this challenged.
- **R-B: W2.2 ground-truth.** Is Dolphin/retail ground truth actually reachable for a
  hand-deformation A/B, or will the planner hit the same "no oracle" wall that sank BandPatchMesh? If
  unreachable, W2.2 should stop at "characterize + propose oracle," not land a bind change blind.
- **R-C: W0.3c scope.** The order-nondeterminism root cause is unknown (verifier ruled out `sMeshGpu`
  as the *direct* cause). If it's a deep multi-site hunt, should W0.3c be timeboxed to
  "root-cause + minimal fix OR interim `setarch -R` gate + filed follow-up," so it can't stall the
  wave?
- **R-D: Concurrent-edit safety.** Lane A (engine `Rnd_Wgpu_RB3.cpp`) and Lane B (engine char/skeleton
  load) are both in the engine repo. Are they file-disjoint enough to avoid the git-reset/clobber
  incidents from Waves 1–2? (Load path = `Skeleton_Native.cpp`, char `Poll`, `BandCharacter`; DrawMesh
  path = `Rnd_Wgpu_RB3.cpp`. Believed disjoint — confirm.)
- **R-E: Over-parallelism.** Is three lanes the right width, or should W3.1/W2.5 defer so the fleet
  concentrates review budget on the two hard items (W0.3c, W2.2)?

## What I want from the Fable review

A decisive verdict on the sequencing (esp. R-A and R-C), the W2.2 ground-truth gate (R-B — this is
the one most likely to reproduce a BandPatchMesh-style revert if wrong), and any lane I've mis-scoped
or mis-parallelized. Concrete amendments, not just concerns.
