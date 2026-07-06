# Wave 3 Kickoff — Fable pre-dispatch review

**Reviewer:** Fable. **Date:** 2026-07-06. **Reviewing:** `execution/WAVE3_KICKOFF.md` (draft).
Evidence tags: **MEASURED** = I read the file/line or ran the check myself; **JUDGMENT** = my call
on measured facts.

## VERDICT: dispatch-with-amendments

The lane skeleton (A sequential W0.3c→W1.6; B parallel bind fix; C cheap wins) is right. Three
things must change before dispatch: (1) **W2.2's stated gates do not actually exercise W2.2's
change** — the real gates must be built as its first subtask; (2) **W3.1 collides file-for-file
with W1.6** and violates the parent plan's own Phase-3 dependency — move it behind W1.6;
(3) the R-C fallback as drafted (`setarch -R` interim gate) is **already refuted by the W0.3b
verifier's own data** — replace it with a canonical-order comparator fallback. Plus one
history-critical addition to the W2.2 brief: a prior rebake experiment already failed with a
documented 200-460u smear signature, and the brief must distinguish itself from it.

---

## R-A — Is starting W2.2 before W1.6 a violation of "nets first"? **NO, with a corrected premise.**

**Ruling: proceed in parallel. Not a rule violation — but the kickoff's stated reason is wrong.**

- MEASURED: `REFACTOR_PLAN.md` Phase-2 header (":90") says Phase 2 is "Blocked on Phase 0 nets
  (W0.1, W0.3, W0.4, W0.5)", i.e. a strict reading blocks W2.2 on W0.3 — which is only
  probabilistic (W0.3b VERIFY: ~33% flake). The per-item Dependencies line (REFACTOR_PLAN.md:99-101)
  does *not* list W0.3 for W2.2, and the coordinator's STATUS note (REFACTOR_PLAN.md:28) already
  waived it. JUDGMENT: the waiver is sound — W2.2's failure modes (bad bind/rebake → shards, wrong
  deformation, mis-placement) are caught by vertex/effector/lineup-class gates, not by the draw-log;
  the draw-log's fail-red-audit + count-exact modes still run diagnostically. **Amendment A1:**
  record the W0.3-dependency waiver explicitly in the W2.2 brief so a future audit doesn't read it
  as a silent skip.
- **The corrected premise (this changes the W2.2 brief materially):** the kickoff says W2.2 is
  "gated by the *deterministic* W0.1 skin golden + W0.4 effector golden" and that "a bind change
  makes both goldens move." **Both claims are mechanically wrong.** MEASURED: W0.1's golden runs in
  `milo-engine-tests` compiled against **dc3-decomp** sources and assets (`W0.1/STATUS.md`: asset
  `char/main/gen/main.milo_xbox` "loads as HamCharacter", DC3 headers, `DC3_DATA`/`MILO_LIB`, posed
  via `SetLocalRot`); W0.4 likewise (`W0.4/STATUS.md`: `ClipPoseFixture`, crowd `female_base`
  clip). W2.2's change lives in **rb3** game code (`rb3/src/system/bandobj/BandCharacter.cpp:1062`
  `RebindOutfitBonesToOwnSkeleton` + a new load-time hook) — **none of which is compiled into
  `milo-engine-tests`**. So the engine goldens will neither move nor gate the bind change. Their
  real role is *invariance nets*: they prove the shared skinning **math** (`BoneOffsetAt·boneWorld`
  composition) didn't change. Useful, but they cannot catch a wrong rebake.
- **Amendment A2 (rewrite of KICKOFF Lane B, line 46 + the "CRITICAL nuance" paragraph):**

  > Replace: "Gated by the *deterministic* W0.1 skin golden + W0.4 effector golden"
  > With: "The W0.1/W0.4 engine goldens are **invariance nets only** (they run in the DC3-context
  > engine suite and never execute the band rebind path — they must stay green untouched, and any
  > red there means W2.2 leaked into shared skinning math, an automatic stop). W2.2's *correctness*
  > gates do not exist yet and are built as **W2.2.S1** before any bind change: (i) an rb3-tests
  > bind-pose identity gtest through the real band load path, (ii) a hand-closeup capture harness.
  > See R-B for the full exit."

  > Delete: "A bind change makes both goldens move. So the item MUST first ... re-capture the
  > goldens to corrected values" — this re-capture scenario cannot occur for W0.1/W0.4 (wrong
  > binary); keep only the general never-re-baseline-to-merely-different rule, applied to the NEW
  > rb3-side goldens.

## R-B — W2.2 ground truth. **Dolphin is reachable at screenshot level; per-vertex it is not. The exit below makes a blind revert structurally impossible.**

**This is the highest-stakes adjudication, so it is the most prescriptive.**

- **Is Dolphin reachable? YES, at screenshot level — the harness already exists and already
  captured band closeups.** MEASURED: `docs/native/c8-ground-truth-2026-07-01/t2-dolphin-oracle.md`
  — headless Xvfb + `dolphin-emu-nogui -p x11 -v Vulkan` + pipe-injected guitar input drove the
  real RB3 Wii disc to **live in-song gameplay with the band rendering**, F9 screenshots at
  2436×1368; `dolphin-shots/` contains `face_guitarist_ambient.png`, `face_singer_rimlit.png`, gameplay
  frames. Gameplay cameras cut to guitarist/singer closeups where hands-on-fret are visible. What is
  **NOT** reachable in a wave: a per-vertex or matched-pose-matched-frame numeric A/B (no Wii-side
  vertex dump tooling; pose/camera timing can't be pinned across emulator and port). JUDGMENT: do not
  ask the planner for a per-vertex Dolphin oracle; ask for reviewer-judged hand-closeup screenshot
  comparison against fresh Dolphin captures, which is achievable with the existing t2 recipe.
- **Why "no oracle → characterize only" (the kickoff's fallback) is the wrong frame:** W2.2 has a
  *mathematical* oracle Dolphin can't provide and doesn't need to: by definition of `invBind`, at
  the captured bind pose the composition `offset′ · perMemberBoneBindWorld` must be identity in
  mesh space — i.e. **skinned verts at bind pose must equal the authored (decoded) verts**. That is
  a falsifiable, fail-red-able gtest through the actual band load path, and it is exactly the check
  whose absence let both BandPatchMesh rewrites ship: they were "Wii-correct" by eyeball with no
  numeric invariant. (ARCHITECTURE_REVIEW.md:150, SYS-7 :123-130.)
- **The known-failure tripwire the kickoff omits (history the planner MUST have):** MEASURED —
  `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:3714-3725`: a prior experiment
  (`RB3_GUARD_EXEMPT_REBOUND=1` + `RB3_BOUND_REBAKE=1`, render-polish 2026-06-11) already paired a
  full rebind with a **rest-pose** rebake and FAILED: "translation anchors (skin-to-bone deltas
  ≤92u...) but the native rotation-basis divergence remains — far-from-bone verts smear by
  R·sin(θ) to persistent **200-460u extents** (gloves/fingernails/jackets vs ~70u character) and
  exempt meshes drew as full-screen slabs." W2.2's thesis is different — rebake against the
  **bind pose captured at skeleton-load/first-pose** (before any clip plays), the untried path from
  `CHAR_SKINNING_DEFORM_INVESTIGATION.md` (wave-08 OPEN item: "capture the animated per-member
  skeleton's BIND-pose orientation ... would need a hook at per-member skeleton load/first-pose") —
  but the brief must (a) cite the failed experiment so the implementer doesn't re-run it, and
  (b) use its failure signature (skinpos > ~92u, shard-ratio > 2×, 200-460u extents) as the numeric
  tripwire that means "you have reproduced the failure, stop."
- **Amendment B1 — W2.2 exit (replaces the kickoff's ground-truth sentence), four layers + flag
  staging:**
  1. **W2.2.S1 (gate construction, lands first):** (i) new `rb3-tests` gtest through the real
     band-member load path: at the captured bind frame, for every rebound bone,
     `offset′ · perMemberBoneBindWorld ≈ identity` and skinned verts ≈ authored decoded verts
     (eps), **fail-red proven** by perturbing the captured bind pose; (ii) a hand-closeup capture
     script (song-select-capture.py pattern) producing framed hand/finger shots, including the
     **count-in/walkon window** (known thin-geo shard window — memory: walkon count-in pose,
     "thin-geo count-in shards = pose-independent skinning residual").
  2. **Numeric draw-time gate:** `REBIND_DRAW_SKINPOS` ≤ ~65u on ALL rebound meshes *including
     hands/fingers/hair* (the established clean-band metric, CHAR_SKINNING doc wave-08 VERIFY);
     `REBIND_DRAW_FLING` (>120u) count = 0 over a full captured gameplay run; shard-guard ratio
     ≤ 2× (`SHARD_RATIO_DBG=1` with the guard still ON) — i.e. explicitly NOT the 200-460u failure
     signature. Torso metrics must not regress vs the torso-only baseline run.
  3. **Visual gate:** W0.5 patch-bearing lineup (numeric bbox layers) green **plus reviewer-judged
     wide AND hand-closeup frames** compared against fresh Dolphin t2 captures +
     `images/retail-screenshots/` gameplay shots. Reviewer-judged is mandatory — the BandPatchMesh
     lesson (execution/README.md hard-won: image gates were blind to shard corruption).
  4. **Flag staging (the structural anti-revert):** the full-body rebind+rebake lands
     **default-OFF** behind a new registered flag (W0.6 registry, `class: feature`). The default
     flip is a **separate, one-line commit** gated on layers 1-3 all green. The torso-only
     `RebindOutfitBonesToOwnSkeleton` path stays intact as the opt-back
     (do NOT delete it this wave — "supersedes" in the kickoff must mean *default-replaces*, not
     *removes*; deletion follows the R5 pattern: only after the A/B opt-out is proven a no-op in a
     later wave). Worst case under this staging is an unflipped flag + a characterization writeup —
     a BandPatchMesh-style blind revert is impossible because nothing behavior-changing lands
     un-gated or default-on unproven.
- **If layer-1's bind-frame capture turns out unreachable at the load seam** (the wave-08 doc's
  stated blocker: "the skeleton is already animating when reachable"), THEN and only then does
  W2.2 stop at "characterize + propose oracle" — that decision point should be an explicit S1 exit
  branch, not the item's default posture.

## R-C — W0.3c scope. **YES, timebox it — but the drafted fallback is refuted; replace it.**

- **Timebox: yes.** MEASURED: the W0.3b verifier explicitly assessed the order flake as "a genuine
  engine determinism investigation (likely more than one call site), not a bounded diff"
  (`W0.3b/STATUS.md:201-207`), and ruled out `sMeshGpu` as the direct source (it is point-queried
  only, never iterated for submission — STATUS:190-199; confirmed in
  `RB3MeshCache.h:119,142-145`). Give W0.3c a diagnosis budget (suggest 2 agent-sessions) with two
  exits.
- **The kickoff's proposed interim gate (`setarch -R`) is DEAD ON ARRIVAL.** MEASURED:
  `W0.3b/STATUS.md:184-188` — the verifier's 15-run sweep ran through `drawlog-golden.py
  --fixed-clock`, which **already wraps in `setarch -R` by default** (STATUS:81), and byte-identical
  binaries still disagreed run-to-run: "run-to-run process nondeterminism that `setarch -R` + the
  frozen clock + the fixed RNG seed + forced loader drain do NOT fully pin." Strike the `setarch -R`
  option from R-C.
- **Amendment C1 — rewrite the W0.3c item (KICKOFF lines 33-36):**

  > **W0.3c** — root-cause the draw-submission-order nondeterminism, timeboxed to ~2 agent-sessions
  > of diagnosis. Leads (in priority order): the flake signature is **mesh-identity swaps**
  > (`skinned/pipe/idx/tris/verts` all differing — W0.3b VERIFY), i.e. the *sequence* of submissions
  > changes, so hunt for (a) iteration over pointer-keyed/unordered containers anywhere in the
  > poll→draw-list path (the verifier flagged the pattern, not the instance; `sMeshGpu` itself is
  > exonerated), (b) allocation-order divergence feeding pointer values — since ASLR is off, heap
  > addresses only vary if the *allocation sequence* varies, which points at **async loader / worker
  > thread interleaving** upstream of the frozen-clock seams (Loader drain forces completion, not
  > completion *order*), (c) the "skin/pose iteration" pointer-order site W0.3b.S2 identified
  > (STATUS:57). Diagnostic tool: dump submission order at 2-3 pipeline stages across ≥15 runs and
  > bisect where order first diverges; log mesh pointer values to confirm/refute allocation-order
  > divergence.
  > **Exit A (fixed):** `--fixed-clock` draw-log golden green across **≥15** fresh boots (the
  > verifier proved 3 is statistically worthless at a ~33% flake rate), `--fail-red-audit` still
  > red, residual sidecar unchanged or shrunk.
  > **Exit B (timebox expired):** land a **canonical-order comparator mode** in
  > `drawlog-golden.py`: compare the draw log as an order-insensitive multiset keyed by
  > (mesh name, pipeline, blend, counts, world-xfm), keeping the exact-count gate, the two-draw
  > bind-group-collapse check, and fail-red-audit. This retains detection of every defect class
  > W1.6 risks (R6: bind-group collapse, world-xfm change, count change, `a0f98ad`-class) while
  > being insensitive to the one already-known order bug; it is deterministic, not probabilistic, so
  > it does not violate the "riskiest item must not land on a probabilistic gate" rule
  > (README Wave-2 W1.6 row). File the order root-cause as W0.3d for Wave 4. Do NOT widen the
  > residual sidecar to swallow order swaps (verifier's explicit warning, STATUS:232-234).
- **W1.6 launch condition (KICKOFF line 62):** amend "only launches if W0.3c verified green" to
  "only launches if W0.3c reached Exit A **or** Exit B (canonical comparator green 15/15 + fail-red
  proven)". JUDGMENT: under Exit B, W1.6 additionally keeps its lineup + 2-scene screenshot gates
  (already in the draft) — with those, launching W1.6 on the canonical comparator is sound; pure
  submission-*order* regressions are the only thing it can't see, and order is precisely the
  property that is currently nondeterministic and therefore un-gateable either way.
- One scope note: the kickoff's second W0.3c lead ("uninitialized `CharEyes`/`CharLookAt` look-at
  state") conflates the **old, bounded, sidecar'd world-jitter residual** (26 draws, eps 3.0 —
  W0.3b.S3) with the **new order flake** (336/354 *unexpected* mesh-identity swaps). They are
  different bugs; the CharEyes fix is a nice-to-have stretch (deletes the sidecar) but must not
  gate W0.3c's exit. State this in the brief.

## R-D — Concurrent-edit safety. **Mostly disjoint, with two real hotspots — one of them is W3.1, not Lane B.**

- **Factual correction to the kickoff (line 79):** "Load path = `Skeleton_Native.cpp`" is wrong.
  MEASURED: `milo-native-engine/src/platform/Skeleton_Native.cpp:19-33` is the **Kinect/COCO
  skeleton-tracking provider** (`NativeSkeletonProvider`, COCO keypoints) — DC3 dance input, nothing
  to do with character skeleton binding. Remove it from the lane description.
- **Actual file surfaces (MEASURED):**
  | Lane | Repo | Files |
  |---|---|---|
  | A · W0.3c | engine | `Rnd_Wgpu_RB3.cpp`, `RB3MeshCache.{h,cpp}`, possibly a poll/draw-list site TBD by diagnosis; rb3 `scripts/native/drawlog-golden.py` (Exit B) |
  | A · W1.6 | engine | `Rnd_Wgpu_RB3.cpp` (`WriteSceneUniforms` :1160-1444, `mSceneBindGroup` creation :1444, all `SetBindGroup(0,…)` sites :1590-2176), `Rnd_Wgpu_RB3.h`, extracted TUs that replay the scene bind group (RB3HaloPass) |
  | B · W2.2 | **rb3** | `src/system/bandobj/BandCharacter.{cpp,h}` (:570, :1062), likely `src/system/char/Character.cpp`/`CharBonesMeshes.cpp` (load/first-pose hook), new `rb3-tests` test, capture script. **Engine: ZERO files** (see below) |
  | C · W2.5 | rb3 | `src/system/bandobj/BandConfiguration.cpp` (+ `BandWardrobe.cpp` if the resolve is there) |
  | C · W3.1 | engine | `Rnd_Wgpu_RB3.cpp` `WriteSceneUniforms`/`SceneUniforms` struct (light arrays :1373-1379, fog), `.wgsl.inc` shaders — **the same function region W1.6 rewrites** |
- **Hotspot 1 — W3.1 ⟂ W1.6 is a direct collision.** MEASURED: W3.1's whole edit surface (fog
  wiring, 4→8 light arrays → `SceneUniforms` struct size → the `e[0].size = sizeof(SceneUniforms)`
  binding at :1436) sits inside `WriteSceneUniforms`/`mSceneBindGroup`, which is the exact code
  W1.6 replaces. See R-E for the resolution (move W3.1 behind W1.6).
- **Hotspot 2 — Lane B's temptation to edit `DrawMesh` — pre-empt it with existing seams.**
  MEASURED: the clamp and rebake already auto-skip rebound meshes via `mesh->mNativeBonesRebound`
  (`Rnd_Wgpu_RB3.cpp:2920-2921, :3190, :3226`), and the shard guard already has an **opt-in
  exemption for rebound meshes**: `RB3_GUARD_EXEMPT_REBOUND=1` (:3726-3731). So W2.2 can run its
  entire experiment loop with zero engine edits: set `mNativeBonesRebound` on rebound meshes
  (the existing rebind already does) + run captures with `RB3_GUARD_EXEMPT_REBOUND=1`. **Amendment
  D1 (add to the W2.2 brief):** "Engine repo is READ-ONLY for this item. The shard-guard default
  flip (making the rebound exemption default-on once metrics are clean) is a Wave-4 follow-up after
  W1.6, or a coordinator-executed one-liner — never a Lane-B edit to `DrawMesh` (hard rule 8
  analog)." Without this line, the first time a rebound hand mesh gets guard-dropped, a Lane-B
  implementer will "fix" `Rnd_Wgpu_RB3.cpp:3712` mid-wave and collide with Lane A.
- **Same-directory adjacency (low risk, note it):** Lane A's optional CharEyes stretch edits rb3
  `src/system/char/CharEyes.cpp`/`CharLookAt.cpp`; Lane B may edit `src/system/char/Character.cpp`/
  `CharBonesMeshes.cpp`; W2.5 edits `src/system/bandobj/BandConfiguration.cpp` next to Lane B's
  `BandCharacter.cpp`. Different files throughout — no blocker — but **Amendment D2:** every lane's
  PLAN.md must enumerate exact files, and the coordinator diffs the three lists for overlap before
  dispatch (the Wave-1 git-reset incident and hard rules 4/7/8 remain in force; also per memory,
  `App.cpp` is entangled with another agent's uncommitted work — no Wave-3 item should touch
  `src/App.cpp`, and W0.3b already has seams there; W0.3c must reuse them, not re-edit).

## R-E — Over-parallelism. **Width is fine; the composition of Lane C is not.**

- **Ruling: keep three lanes, but move W3.1 out of Wave-3 parallel execution.** Two independent
  grounds: (1) the file collision with W1.6 (R-D hotspot 1, MEASURED); (2) the parent plan itself:
  Phase 3 is "Blocked on W0.6 flag registry ... **and W0.3 per-draw golden (to prove no bind-group
  collapse)**" (REFACTOR_PLAN.md:123, :131) — W0.3's golden is exactly what is NOT green yet. The
  kickoff schedules W3.1 in parallel anyway, violating its own parent plan. **Amendment E1:**
  either (a) append W3.1 to Lane A's tail (W0.3c → W1.6 → W3.1) to run only if the wave has budget
  left after W1.6 lands and its golden is green, or (b) defer W3.1 to Wave 4. I recommend (b): the
  wave's review budget should concentrate on W0.3c and W2.2 (the kickoff's own instinct in R-E),
  and a 5-hour wave rarely reaches a lane-A third item.
- **W2.5: keep.** Game-side, one file, disjoint (R-D table), independent data-gap fix
  (ARCHITECTURE_REVIEW.md:144 — "independent game-side data gap, NOT the transform engine").
  **Amendment E2:** add the standard fail-red requirement to its exit: demonstrate the assert/log
  fires on a synthetic unresolvable `targName` (temporarily rename one waypoint), not just that it
  stays silent on good data.
- Resulting wave shape: **Lane A** W0.3c→W1.6 (sequential, engine), **Lane B** W2.2 (rb3-only,
  staged S1 gates → S2 flag-off change → S3 measure → S4 default flip), **Lane C** W2.5 (tiny).
  JUDGMENT: this is the right concentration — two hard items get the verifier depth that caught
  W0.3b's flake in the first place.

## Missing items / gates (bug-class audit vs SYS-1..7 + two waves of history)

1. **[covered above, restated as a gate]** W2.2 without the bind-pose identity gtest is the exact
   SYS-7 hole that shipped both BandPatchMesh reverts. Amendment B1.1 closes it. This is the
   single most important amendment in this review.
2. **Count-in/walkon window coverage** — the known thin-geo shard window is the count-in
   (pose-independent skinning residual; the W0.5 lineup gate does not frame it). B1.1(ii) adds it
   to the hand-closeup capture. Without it, W2.2 can pass every gate and still shard at song start
   — the user-visible moment.
3. **Crowd/extras negative control for W2.2** — the rebind is band-scoped via the `mFilter` seam
   (CHAR_SKINNING doc), but the rebake hook at skeleton load is NEW code near shared char paths.
   Add to W2.2's exit: crowd/extras clamp behavior byte-identical (SKIN_CLAMP probe counts
   unchanged vs baseline), per the wave-08 regression method.
4. **W0.6 registry hygiene** — W2.2's new flag(s) and any W0.3c flag must be registered in
   `NativeCompatFlags.classification.json` at introduction (`census check` exit 0), per the
   W0.3b.S1 precedent. One sentence in each brief.
5. **Wave gate unchanged** — engine-test suite 198/0 + lineup-gate PASS + coordinator-only pin
   bump remain the wave exit (README protocol); the W0.1/W0.4 goldens staying green is now
   *load-bearing as an invariance net for W2.2* (see R-A), so a red there is an automatic Lane-B
   stop, not a re-baseline candidate.

## Summary of amendments (in kickoff order)

| # | Where | Change |
|---|---|---|
| A1 | Lane B brief | Record the W0.3-dependency waiver for W2.2 explicitly |
| A2 | KICKOFF :46-53 | Reword gates: W0.1/W0.4 are invariance nets (DC3-context suite, never execute the band rebind); real gates built as W2.2.S1; delete the "goldens move / re-capture" instruction |
| B1 | Lane B exit | Four-layer exit: bind-pose identity gtest (fail-red) → numeric draw-time (≤65u incl. hands, 0 flings, ratio ≤2×) → W0.5 lineup + reviewer-judged wide+hand-closeup vs Dolphin t2 captures → land default-OFF, flip default in a separate gated commit; torso-only path retained as opt-back |
| B2 | Lane B brief | Cite the FAILED `RB3_BOUND_REBAKE` rest-rebake experiment (`Rnd_Wgpu_RB3.cpp:3714-3725`) + its 200-460u signature as the stop-tripwire; distinguish bind-pose-capture thesis from it |
| C1 | KICKOFF :33-36 | Timebox W0.3c (~2 sessions diagnosis); STRIKE the `setarch -R` fallback (refuted, W0.3b STATUS:184-188); Exit B = canonical-order (multiset) comparator, 15-run bar, fail-red kept |
| C2 | KICKOFF :62 | W1.6 launches on Exit A **or** Exit B; CharEyes determinism is stretch, not exit |
| D1 | Lane B brief | Engine repo READ-ONLY for W2.2; use `mNativeBonesRebound` + `RB3_GUARD_EXEMPT_REBOUND=1` seams; guard default-flip deferred |
| D2 | All lanes | Correct `Skeleton_Native.cpp` misattribution (Kinect provider); PLAN.md per-lane exact-file lists, coordinator cross-diff before dispatch; nobody touches `src/App.cpp` |
| E1 | Lane C | Remove W3.1 from parallel Lane C (collides with W1.6 at `WriteSceneUniforms` :1160-1444; Phase 3 blocked on W0.3 per REFACTOR_PLAN:123) — defer to Wave 4 (preferred) or Lane-A tail |
| E2 | W2.5 | Add fail-red demo (synthetic unresolvable waypoint) |
