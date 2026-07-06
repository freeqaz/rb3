# Wave 4 Kickoff — Fable pre-dispatch review

**Reviewer:** Fable. **Date:** 2026-07-06. **Reviewing:** `execution/WAVE4_KICKOFF.md` (draft).
Evidence tags: **MEASURED** = I read the file/line or ran the check myself; **JUDGMENT** = my call
on measured facts.

## VERDICT: dispatch-with-amendments

The Lane A/B skeleton is right and W2.1's sequencing behind W1.6 is correct. Three things must
change before dispatch: (1) **Lane C is built on a mechanically wrong flag and a refuted premise**
— `RB3_SKEL_REBIND_FULL` is the *known-broken* full-body rebind (flipping it default-ON ships the
sharding path the Phase-0 fail-red deliberately uses), the flag W2.2 actually landed is
`RB3_HANDS_BIND_FIX`, and W2.2's own S3 measurement + verifier already adjudicated **NO FLIP**
(zero measured benefit); Lane C must be rewritten around the foot/shoe coverage gap instead.
(2) **The kickoff's R-B premise is scene-hollow**: the canonical draw-log golden only exists for
`splash_screen`, which contains no crowd or drum kit — as committed, the gate cannot see the
crowd co-location fix *at all*; W2.1 needs a placement-specific gate built first (specified below).
(3) **W3.1 must defer to Wave 5** — it is not the "cheap fill" the brief prices: `SceneUniforms`
is the shared cross-backend WGSL contract, so 4→8 light arrays is a DC3-blast-radius change with
no DC3 gate in the brief, and the parent plan's Phase-3 blocker (a clean W0.3 golden) is exactly
what W0.3d is only now fixing.

---

## R-A — Lane A concurrency/ordering. **Sequential is right; order W2.1→W2.3 is right; W3.1 leaves the lane entirely (see R-E).**

**MEASURED line ranges in `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` @ `6221a56` (4,805
lines):**

| Function / region | Lines | Owner item |
|---|---|---|
| `WriteSceneUniforms` | **:1160–1458** (light fills :1355–1379, `s.fogEnabled = 0` :1429, `e[0].size = sizeof(SceneUniforms)` :1437) | W3.1 |
| `SubmitDraw` | :2110–2120 | — |
| `DrawMesh` | **:2121–4193** (skinned `obj.world` selection block :2807–2850 incl. hub-bar/scrollbar injections; bone-palette / `GeomOwner` region :2882–~3060; `SubmitDraw(ctx)` :4177) | W2.1 + W2.3 |

- **Post-W1.6 the two functions are genuinely disjoint**: `DrawMesh` consumes the scene binding
  opaquely (`RB3DrawContext ctx{ mActiveScene, ... }` → `SubmitDraw`), never constructs or resizes
  it; `sizeof(SceneUniforms)` appears exactly once in the RB3 TU, inside `WriteSceneUniforms`
  (:1437). So the W3.1⟂W1.6-class *semantic* collision no longer exists. **BUT** W3.1's real edit
  surface is NOT confined to that function — MEASURED: `struct SceneUniforms` lives in
  `src/gfx/UniformStructs.h:18–56` with `static_assert(sizeof(SceneUniforms) == 656)` and the
  header's own contract comment ("Shared between every gfx backend... if a backend wants to extend
  a struct, it must extend it HERE so all backends and the WGSL stay in lockstep",
  `UniformStructs.h:4–12`); the DC3 backend binds the same struct at `Rnd_Wgpu.cpp:1644`; the light
  loops live in the shared `standard_wgsl.inc`. That makes W3.1 a **cross-backend contract change**,
  not an `Rnd_Wgpu_RB3.cpp` edit — see R-E.
- **Even function-disjoint items should not run concurrently in the same file**: agents edit the
  one shared engine working tree (hard rules 2/4/7/8 exist because of it), so two agents holding
  `Rnd_Wgpu_RB3.cpp` open simultaneously can lose edits regardless of function boundaries.
  JUDGMENT: keep Lane A sequential.
- **Order W2.1→W2.3: correct.** Both edit the SAME `DrawMesh` region (:2807–3060) — W2.1 defines
  the placement contract (what `obj.world` and the palette basis each encode); W2.3's
  "read the drawn mesh's own bones" only has a defined correctness contract once that is settled.
  **Amendment A1 (W2.3 brief):** "becomes unnecessary" must mean *default-replaces, not removes* —
  the draw-time `RebindCrowdCharBonesToOwnSkeleton` call (rb3 `src/system/world/Crowd.cpp:409`,
  impl :906–1040) stays intact this wave; delete only in a later wave after its opt-out is proven
  a no-op (the plan's own R5 pattern).

## R-B — Does W2.1 need W0.3d first? **No — but the kickoff's confidence in the canonical gate is misplaced for a worse reason than eye-flake: the gate cannot see crowd/drum at all. A placement-specific gate is REQUIRED, built as W2.1.S1.**

The kickoff (:32–33, :62–66) says "the residual-filtered canonical draw-log (co-location detection
is exactly what it catches)". Three grounded corrections:

1. **Scene coverage — the decisive one.** MEASURED: `native/tests/goldens/drawlog/` contains
   exactly `splash_screen.json`, `splash_screen.fixedclock-residual.json`, and
   `synthetic_scene.json`; `drawlog-golden.py --scene` defaults to `splash_screen` (:682) and
   W1.6's verifier already flagged "harness golden is splash-only" (`W1.6/STATUS.md:396-397`).
   Crowd and drum kit draw only in gameplay (`WorldCrowd::Draw3DChars`, `Crowd.cpp:404-417`;
   bone-attached instrument props). **A crowd co-location fix — and any regression of it — is
   simply outside the golden'd scene: the gate stays green on a broken crowd because the crowd
   never appears in it.** The comparator's fail-red `CatchesCoLocation` proves the *mechanism*
   detects co-location in a scene it observes; it says nothing about a scene it never observes.
2. **The residual-name filter is a real (bounded) swallow channel for W2.1 specifically.**
   MEASURED: `compare_canonical` (`drawlog-golden.py:428-580`) tolerates any world divergence
   ≤ eps on a draw whose name-hash is in the sidecar's known-name set. The sidecar names are
   CharEyes/CharLookAt **skinned character eye/face meshes** — exactly the mesh class whose
   `obj.world` W2.1 changes. A ≤3.0u W2.1 error on those meshes is silently "expected". Worse, the
   W1.6 verification protocol ("classify residual-name-only world failures as non-blocking",
   `W0.3c/STATUS.md:352-356`) was sound for W1.6 only because W1.6 *mechanically could not alter
   any `obj.world`* (proven by A/A controls). **W2.1 is precisely a world-alterer — that protocol
   must NOT be carried into W2.1's verification.** Amendment B2 below.
3. **During W2.1's own A/B, the world axis saturates with intended signal** (every skinned draw's
   world changes by design under flag-ON), so the canonical comparator degrades to its
   count / scalar-multiset / bind-group-collapse / mesh-identity axes for the transition. Those
   axes are still valuable nets (they catch the a0f98ad class and dropped draws) — keep the gate —
   but it cannot adjudicate "placement is right".

**Ruling on the question as asked:** W2.1 does NOT need W0.3d first (agree with the kickoff — and
because W2.1 lands default-OFF per R-C, the committed splash golden stays valid, so Lane A and
Lane B never contend over golden files). But the canonical gate is a *secondary regression net*
for W2.1, not its correctness gate.

**Amendment B1 — the placement gate (replaces the kickoff's Lane-A gate sentence, built as
W2.1.S1 BEFORE any behavior change, fail-red proven on the CURRENT build):**

> **W2.1.S1 (gate construction, lands first):**
> (i) **Gameplay draw-log golden**: extend the drawlog capture to a gameplay scene (nav harness
> pattern from `song-end-test.py` / `hand-closeup-capture.py`, `RB3_FIXED_CLOCK`, camera pinned
> via `rb3_director_disable`/`rb3_force_shot` as in `band-closeup-capture.py`) with crowd + drum
> kit in frame.
> (ii) **The "right, not just different" oracle — assert the renderer honors the faithful
> game-side transform it currently discards.** For crowd: `WorldCrowd::Draw3DChars` poses each 3D
> instance via `curChar->SetWorldXfm(spXfm)` (`Crowd.cpp:404`) where `spXfm` comes from the
> faithful placement-mesh path — so ground truth is already computed CPU-side by decomp code.
> The gate asserts each crowd instance's drawn `obj.world` translation matches the `spXfm.v` it
> was posed with (probe at the SetWorldXfm site vs the drawlog record), instances are
> pairwise-distinct, and their positions span the bowl (not all-equal — REFACTOR_PLAN Phase-2
> exit). **Fail-red is free**: on the current build this asserts non-identity worlds and MUST go
> RED (today every skinned instance logs identity) — that red is the proof the gate sees the bug.
> For drum kit: drawn prop-mesh world ≠ origin AND consistent with the drummer's waypoint/bone
> world (W0.4-effector-style, through the faithful `BandCharacter` prop-attach path).
> (iii) **Reviewer-judged Dolphin wides**: fresh t2 captures + the existing
> `c8-ground-truth-2026-07-01/dolphin-shots/gp_*.png` (gp_00 already shows the crowd spatially
> spread house-left) + `images/retail-screenshots/fandom_gameplay_*.png` for drum-kit position.
> The canonical splash gate + rb3-tests `DrawLogGolden.*` + W0.5 lineup run as regression nets on
> the flag-OFF path (byte-identical required) and as count/bind-group/identity nets on flag-ON.

**Amendment B2 (verification protocol):** the W2.1 verifier must treat residual-name world
divergences as **in-scope signal, not the W1.6-era non-blocking eye jitter** — W2.1 can
mechanically alter those meshes' worlds. The A/A-control technique (baseline-vs-baseline) from the
W1.6 verify is the right way to re-separate genuine eye-flake from W2.1 effects.

## R-C — Default-OFF staging + ground truth for W2.1. **YES — stage default-OFF behind ONE flag gating BOTH halves atomically. Ground truth = the faithful game-side transforms (B1.ii) + Dolphin wides.**

- **Blast radius (MEASURED):** the edit replaces the `} else if (skinned) { identity }` arm at
  `Rnd_Wgpu_RB3.cpp:2848-2849` — the path taken by **every skinned draw in the game**: band
  members, hair, crowd 3D chars, and the skinned UI meshes (hub bar, scrollbar thumb). This is
  the largest behavior surface any item has touched, on the just-refactored `DrawMesh`.
- **The in-file comments document the exact failure mode of a naive version:** the hub-bar block
  warns "the FULL meshWorld would double-apply the model→world rotation the palette already
  encoded and SKEW the bar" (:2797-2800), and the scrollbar/hub comments state character bones
  "already hold WORLD coords" (:2791-2792). So `obj.world = WorldXfm()` is only correct **jointly
  with** the bind-relative palette-basis change — a coupled two-part change with a documented
  double-transform trap between the parts. **Amendment C1:** one registered flag (suggest
  `RB3_PLACEMENT_CONTRACT`, class:feature, engine-side so the census enforces registration) gates
  BOTH halves; there must be no intermediate commit where one half is live without the other.
- **History says stage it:** two BandPatchMesh reverts + the `RB3_BOUND_REBAKE` 200-460u failed
  experiment are all this bug family; W2.2's default-OFF staging just demonstrated the worst case
  can be "an unflipped flag", never a blind revert. JUDGMENT: W2.1 (and W2.3, its own flag)
  land default-OFF; flips are separate one-line coordinator-gated commits — same B1-layer pattern
  as W2.2.
- **Flag-OFF exit requirement:** flag-OFF must be **byte-identical** (canonical splash A/B
  0-unexpected, lineup PASS, rb3-tests green) — this also keeps Lane B's golden work uncontended.
- **Flag-ON exit:** B1 placement gate green (crowd spread matching spXfm, drum ≠ origin) + the
  name-scoped placement hacks' opt-outs (`RB3_NO_HUB_BAR_PLACEMENT_FIX`,
  `RB3_SCROLLBAR_THUMB_FIX_OFF`, `RB3_NO_CROWD_REBIND`) proven **no-ops** (REFACTOR_PLAN Phase-2
  exit) + a song_select hub-bar/scrollbar screenshot A/B (those injections live inside the edited
  block at :2807-2850 — a placement-contract regression there is invisible to every gameplay gate)
  + hands/skinning nets green (`HandsBindOracle`, W0.5 lineup, hand-closeup harness) + crowd
  SKIN_CLAMP negative control vs `W2.2/char/parsed-default.json`.

## R-D — W2.2-flip. **The split ("agent packages, coordinator adjudicates") is right in principle — but the item as drafted must not dispatch: wrong flag, refuted premise.**

- **Wrong flag (MEASURED, would ship a known-broken path):** the kickoff (:10, :53-54) and the
  README Wave-3 table/backlog (:131, :154) say W2.2 landed behind **`RB3_SKEL_REBIND_FULL`** and
  Wave 4 flips it default-ON. `RB3_SKEL_REBIND_FULL` is the *pre-existing* full-body-rebind
  toggle at `src/system/bandobj/BandCharacter.cpp:1084`, whose own comments read "it animates the
  whole body but **shards thin geo**" (:1060) / "rebinds everything under the rotation-basis
  mismatch" (:1081) — it is the **deliberately-broken skin** the Phase-0 W0.1 exit gate uses for
  fail-red (REFACTOR_PLAN:59). Flipping it default-ON ships the sharding path. The flag W2.2.S2
  actually landed is **`RB3_HANDS_BIND_FIX`** (`BandCharacter.cpp:1385`, commit `32746985`).
  **Amendment D1:** correct the flag name in WAVE4_KICKOFF and in both README occurrences before
  any brief is cut from them.
- **Refuted premise (MEASURED):** "one adjudicated flag-flip from shipping" is not what W2.2
  concluded. S3 measured `RB3_HANDS_BIND_FIX=ON` yields **no improvement on its own scope**
  (head graze 69.5u↔69.5u, hands identical; `W2.2/STATUS.md` S3 gate 1), and S3 + the adversarial
  verifier both recommend **NO FLIP**: the hands/head fix that matters
  (`RebindHeadHandsAtRest`) is **already default-ON** (`BandCharacter.cpp:522`, opt-out
  `RB3_NO_HEAD_REBIND`) and is a proven net win (rebake-OFF head guard-DROPs at 9.59×); the ~69u
  head graze is **already adjudicated structural** (identical flag-OFF/ON; non-rebound crowd
  bodies show the same 63-64u extent); the one HARD residual is the **foot/shoe lower-body path**
  (`saddleshoe_skin.2` 4.73× guard-DROP), which goes through `RebindOutfitBonesToOwnSkeleton`,
  not the hands path, and which the flag cannot clear.
- **Amendment D2 — rewrite Lane C:**
  1. **W2.2-close (coordinator memo, no fleet time):** sign off the head-graze adjudication from
     the S1a/S3 record (structural, crowd-baseline-matched); record the no-flip decision for
     `RB3_HANDS_BIND_FIX`; append its `classification.json` entry (S2's outstanding handoff —
     MEASURED still absent from `NativeCompatFlags.classification.json`); correct the README flag
     name.
  2. **NEW item W2.6 — foot/shoe rest-capture coverage** (the actual residual W2.2 filed):
     extend the load-time rest capture to the lower-body/outfit meshes that still guard-DROP.
     rb3-only, `BandCharacter.cpp`, engine READ-ONLY (same seams as W2.2), default-OFF behind its
     own flag, gated by the existing `hands_bind_characterize.py` A/B (the drop must clear:
     ratio ≤2×, no DROP on foot meshes, FLING=0, crowd clamp byte-identical) + `HandsBindOracle`
     + W0.5 lineup. This inherits W2.2's four-layer anti-revert staging wholesale.
  3. Reviewer-judged Dolphin hand-closeup A/B remains worthwhile as *documentation* of the
     already-shipped `RebindHeadHandsAtRest` (S1c's harness + `handcloseup_walkon.png` are ready)
     — but it gates nothing this wave, so make it optional tail work, not a lane exit.
- On the general question: yes — "subagent produces the A/B package, coordinator (human-eyes)
  adjudicates any default flip in a separate one-line commit" is the correct standing split; no
  full-wave hold for human sign-off is needed **because** flips are structurally separated.

## R-E — Over-width / W3.1. **Defer W3.1 to Wave 5. Three grounds, one of them new.**

1. **It is not reachable anyway:** as the third serial item in Lane A behind the biggest
   correctness change of the campaign, a ~5-hour wave will not get there (the Wave-3 review's own
   observation, borne out — Wave 3's Lane A consumed the wave on W0.3c+W1.6).
2. **NEW — W3.1 is not RB3-local, and the brief prices it as if it were.** MEASURED:
   4→8 light arrays change `SceneUniforms` in `src/gfx/UniformStructs.h` (static_assert 656 — the
   cross-backend WGSL layout contract, consumed by DC3's `Rnd_Wgpu.cpp:1644` and by the shared
   `standard_wgsl.inc`). That is a **DC3 blast radius** with zero DC3 gate in the kickoff (and a
   documented history of engine-ahead-of-DC3 breakage — the dc3 masking incident). When W3.1 runs
   (Wave 5), its brief needs: extend struct + WGSL in lockstep, `milo-engine-tests` 198/0
   (DC3-context, incl. WgslValidation) as a hard gate, and a DC3-side visual smoke.
3. **Parent-plan consistency:** Phase 3 is "Blocked on W0.6 ... and W0.3 per-draw golden"
   (REFACTOR_PLAN:135); the golden becomes a clean gate exactly when Lane B's W0.3d lands. Running
   lighting one wave later on a clean gate is the plan working as designed, not a delay.

Resulting wave shape: **Lane A** W2.1→W2.3 (engine, sequential, each default-OFF-staged);
**Lane B** W0.3d (rb3 + diagnosis); **Lane C** W2.2-close memo + W2.6 foot/shoe (rb3-only).
JUDGMENT: that is the right concentration — the two hard items (W2.1 placement, W0.3d determinism)
get the verifier depth.

## Missing items / gates / collisions (SYS-1..7 audit)

1. **W0.3d(b) collision constraint (Lane B ⟂ Lane A).** The mechanism-2 root cause is
   "async-loader/worker completion-order feeding object-list insertion order"
   (`W0.3c/STATUS.md:65-70`) — a load-path/engine hunt whose fix could want to touch
   `Rnd_Wgpu_RB3.cpp` or shared loader code. **Amendment F1 (W0.3d brief):** part (b) is
   *diagnosis-only* with respect to any file in Lane A's list; if the fix lands in
   `Rnd_Wgpu_RB3.cpp` or the object-list path Lane A is editing, STOP and hand the patch to the
   coordinator for post-Lane-A sequencing (the exact discipline W0.3c.S2 honored). Part (a)
   (CharEyes/CharLookAt freeze under `RB3_FIXED_CLOCK`, eps recalibration from a large sample)
   is rb3 `src/system/char/` + sidecar regen — disjoint from every other lane. Do NOT satisfy (a)
   by widening the global eps (the W0.3b verifier's explicit warning): per-name eps derived from
   an N≥30-boot sample, or a jitter-source fix, only.
2. **Hub-bar/scrollbar UI regression net for W2.1** — covered as Amendment C1's song_select A/B;
   without it, a placement-contract bug in the :2807-2850 block regresses SYS-1's UI family
   (hub bar / scrollbar / HUD placement) invisibly to all gameplay gates.
3. **W2.3 negative controls:** crowd 2D **imposter** path byte-identical (it is correct today —
   ARCHITECTURE_REVIEW "2D crowd imposter path — correct"); crowd/extras `SKIN_CLAMP` counts vs
   `W2.2/char/parsed-default.json` baseline.
4. **Flag registry hygiene:** W2.1/W2.3's engine-side flags trip the census scanner (engine src IS
   a scan root, unlike rb3/src) — register in `NativeCompatFlags.classification.json` at
   introduction, `census check` exit 0 in each exit gate. Lane C's rb3-side flags follow the W2.2
   precedent (coordinator appends at pin-bump).
5. **Standing hygiene unchanged:** nobody touches `src/App.cpp`; the engine tree still carries a
   sibling agent's uncommitted `FxSendNative.cpp` — leave it; per-lane PLAN.md exact-file lists +
   coordinator cross-diff before dispatch; wave exit remains engine-tests 198/0 + lineup PASS +
   coordinator-only pin bump; hard rules 1–8 in force.

## Summary of amendments (in kickoff order)

| # | Where | Change |
|---|---|---|
| A1 | Lane A / W2.3 brief | `RebindCrowdCharBonesToOwnSkeleton` retained this wave (default-replaces, not removes; delete only after opt-out proven no-op, R5 pattern) |
| B1 | Lane A / W2.1 gates | Placement gate built as W2.1.S1 BEFORE the change: gameplay drawlog golden + spXfm-vs-drawlog crowd oracle (fail-red = RED on current co-located build) + drum-prop ≠ origin + Dolphin/retail wides. Canonical splash gate demoted to regression net (splash contains no crowd/drum — MEASURED goldens dir) |
| B2 | W2.1 verify protocol | The W1.6 "residual-name world failures = non-blocking" rule must NOT carry over; use A/A controls to re-separate eye-flake |
| C1 | Lane A / W2.1 staging | Default-OFF behind ONE flag gating obj.world + palette-basis atomically; flag-OFF byte-identical; flag-ON exit incl. hub-bar/scrollbar song_select A/B + opt-out-no-op proofs; flip = separate coordinator-gated commit |
| D1 | Kickoff :10/:53-54 + README :131/:154 | Correct the flag name: W2.2 landed `RB3_HANDS_BIND_FIX`, NOT `RB3_SKEL_REBIND_FULL` (the latter = the known-broken full-body rebind used as Phase-0 fail-red — flipping it ships shards) |
| D2 | Lane C | Rewrite: no flip exists to ship (S3 measured no benefit; `RebindHeadHandsAtRest` already default-ON; head graze already adjudicated structural). Lane C = coordinator close-out memo + classification.json entry + NEW W2.6 foot/shoe rest-capture coverage (default-OFF, W2.2-pattern gates); Dolphin A/B optional documentation |
| E1 | Lane A tail | Defer W3.1 to Wave 5; its Wave-5 brief must treat `SceneUniforms`/WGSL as the shared cross-backend contract (DC3 gates: engine-tests hard, DC3 visual smoke) |
| F1 | Lane B / W0.3d | Part (b) diagnosis-only wrt Lane-A files; engine fixes coordinator-sequenced after Lane A. Part (a): no global-eps widening — per-name eps from ≥30-boot sample or jitter-source fix |
| F2 | All lanes | Census registration for engine-side flags at introduction; PLAN.md exact-file lists + coordinator cross-diff; `App.cpp` off-limits; `FxSendNative.cpp` untouched |
