# W2.1 — Skinned-placement contract (SYS-1 placement half): crowd/drum-kit at one point

**Planner:** Opus. **Wave 4, Lane A item 1 (engine).** Sequential BEFORE W2.3 (both edit
`Rnd_Wgpu_RB3.cpp` `DrawMesh` `:2807–3316`). Engine pin `6221a56` (do NOT bump).
Status: PLAN (not yet implemented).

## Objective

Fix the SYS-1 **placement** half: skinned meshes whose placement arrives via `SetWorldXfm` on a
parent `Dir` (crowd 3D chars, bone-attached drum-kit/instrument props) collapse to the origin
because `DrawMesh` forces `obj.world = identity` for every skinned draw and derives placement
entirely from the bone palette. Adopt Wii semantics: `obj.world = mesh->WorldXfm()` for skinned
meshes too, **jointly with** a **bind-relative bone-palette basis** so the model→world placement is
applied exactly once. This is a **coupled two-part change** — the in-file comments at
`Rnd_Wgpu_RB3.cpp:2797–2800` document that `obj.world = WorldXfm()` WITHOUT the bind-relative
palette double-applies the model→world rotation and SKEWS the mesh. **Both halves land TOGETHER
behind ONE engine-registered flag `RB3_PLACEMENT_CONTRACT` (default-OFF).** The default-ON flip is a
SEPARATE, coordinator-gated one-line commit — NOT part of this item.

### Faithful-reference citations (VERIFIED this planning pass, against `6221a56` HEAD)

- **The bug site** — `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:2847–2848`:
  ```cpp
  } else if (skinned) {
      for (int i = 0; i < 16; i++) obj.world[i] = (i % 5 == 0) ? 1.f : 0.f;   // IDENTITY
  } else {
      MiloXfmToColMajor(mesh->WorldXfm(), obj.world);                          // static path
  }
  ```
  This `else if (skinned)` arm is taken by **every skinned draw in the game** (band members, hair,
  crowd 3D chars, skinned UI: hub bar `:2842`, scrollbar thumb `:2840`).
- **The palette composition** — `:3159–3160` then `:3315`:
  ```cpp
  Transform skin; Multiply(owner->BoneOffsetAt(b), wt, skin);   // skin = BoneOffset(b) * boneWorld  (WORLD space)
  ... MiloXfmToColMajor(skin, dst);                             // bones.bones[b]
  ```
  GPU applies `worldPos = obj.world * (Σ w_b · boneMat_b · v)` — the comment at `:2760–2764` states
  the palette "already composes the bone's world transform, so the blended vertex is in world space;
  the object world matrix must be IDENTITY to avoid double-transforming."
- **The double-transform trap** — `:2793–2800` (hub-bar block): "the FULL meshWorld would
  double-apply the model→world rotation the palette already encoded and SKEW the bar"; `:2791–2792`
  (character bones "already hold WORLD coords"). This is the exact hazard the bind-relative half
  neutralizes.
- **Ground truth for crowd placement** — `rb3/src/system/world/Crowd.cpp:337–403`: each 3D instance
  is placed via `curChar->SetWorldXfm(spXfm)` (`:403`), where `spXfm` is the faithful per-instance
  bowl placement (`.v` from `m3DChars[i].unk0` + height/rotate/focus billboard math), computed
  CPU-side by decomp code. `RebindCrowdCharBonesToOwnSkeleton(curChar)` (`:409`, native-only) runs
  right after. **The renderer currently DISCARDS `spXfm` for the drawn skinned mesh** (obj.world
  forced identity) → all instances co-locate. `spXfm` is the "right, not just different" oracle.
- **Drum kit / instrument props** — attached to specific hand/prop bones (`BandCharacter.cpp:775–776`,
  `:583–584`); their prop-mesh world must be `≠ origin` AND consistent with the drummer's
  bone/waypoint (W0.4-effector-style).
- **Band members are a no-op under the contract** (the invariance that pins the algebra): a skinned
  character mesh's own `WorldXfm` is identity here (`:3245–3246`: "A skinned mesh's own WorldXfm is
  identity here … would just read back the character's far-from-origin world position") and its
  placement lives in `boneWorld`. So for band/hands `obj.world = WorldXfm() = I` and the palette is
  unchanged → **byte-identical**. `SkinGolden.*` + `HandsBindOracle` + `ClipPoseFixture.*` PIN this.
- **Flag registry** — `src/platform/NativeCompatFlags.classification.json` (top-level keyed by flag
  name); template entry `RB3_FIXED_CLOCK` = `{"class":"feature","read":"presence","default":"off",
  "owner":...}`. Read idiom in `DrawMesh`: cached-static `getenv` (e.g. `:3083–3084`
  `static int sWorldFixOff=-1; if(sWorldFixOff<0) sWorldFixOff=getenv(...)?1:0;`) — release-safe
  (NOT `ProbeActive`, which compiles out in release). The census (`native_compat_census.py`) scans
  engine `getenv` sites and requires each be registered → `check` exit 0.

## Design of the placement contract (the coupled change, S2)

Two halves, gated atomically by `RB3_PLACEMENT_CONTRACT`:

- **Half A (obj.world):** replace the `else if (skinned) { identity }` arm (`:2847–2848`) with
  `MiloXfmToColMajor(mesh->WorldXfm(), obj.world)` **when the flag is ON**. Flag-OFF keeps the
  identity arm verbatim.
- **Half B (bind-relative palette):** when the flag is ON, compose the palette so the bone matrix is
  expressed **relative to the mesh** (strips whatever world the bone carries so `obj.world=meshWorld`
  supplies placement exactly once), instead of the current world-space `skin = BoneOffset·boneWorld`.

**The EXACT operand order (which factor of `mesh->WorldXfm()`/its inverse, on which side) is NOT
asserted in this plan — it is determined empirically in S2 against the S1 oracle + the invariance
nets**, because the row-vector/column-major conventions here make a paper derivation error-prone and
the safety nets bound it precisely on both sides:
  - **Band/hands byte-identical** (`SkinGolden.*`, `HandsBindOracle`, `ClipPoseFixture.*` stay green,
    and canonical splash A/B 0-unexpected on the flag-OFF path AND — since band meshWorld≈I — on the
    flag-ON path for band) pins the `meshWorld≈I` case.
  - **Crowd drawn `obj.world` translation == the `spXfm` it was posed with** (S1 oracle) pins the
    placement case.
  - **The double-transform trap comment (`:2797–2800`) is the guardrail** — a naive Half-A-only build
    MUST skew, which is the negative signal proving Half B is required and correctly formed.

Under flag-ON the general contract should subsume the name-scoped hub-bar (`:2842–2846`) and
scrollbar-thumb (`:2833–2841`) injections — hence the exit requirement that
`RB3_NO_HUB_BAR_PLACEMENT_FIX` / `RB3_SCROLLBAR_THUMB_FIX_OFF` become **no-ops** under flag-ON.
Keep those name-scoped arms structurally intact this wave (delete only after their opt-outs are
proven no-ops — R5 pattern); flag-OFF they behave exactly as today.

## Subtasks

### W2.1.S1 — Build the placement gate FIRST (BEFORE any behavior change)  · model: opus
**Goal:** stand up the gate that can SEE the crowd/drum co-location bug, and prove it goes RED on the
CURRENT (unchanged) build. The committed splash golden has NO crowd/drum, so it CANNOT gate
placement — this subtask is the real correctness gate for W2.1.

**Files (create/extend):**
- `rb3/scripts/native/placement-gate-capture.py` (new) — gameplay drawlog capture, camera-pinned.
- `rb3/native/tests/goldens/drawlog/gameplay_crowd.json` (+ `.fixedclock-residual.json`) — committed
  golden (regression net) once the contract is ON; on the current build it is captured only as the
  RED reference.
- `rb3/native/tests/test_placement_oracle.cpp` (new gtest in `rb3-tests`) OR an extension of
  `native/tests/test_draw_log_golden.cpp` — the spXfm-vs-drawlog oracle.
- `rb3/src/system/world/Crowd.cpp` — an **`HX_NATIVE`-guarded, env-gated (`RB3_PLACEMENT_PROBE`),
  default-OFF diagnostic** `fprintf` at the `SetWorldXfm(spXfm)` site (`:403`) dumping
  `instanceIndex, spXfm.v.{x,y,z}` (and the drummer prop bone/waypoint world for the drum case).
  This is diagnosis-only, no behavior change, Wii-compile-inert (`HX_NATIVE` undefined under MWCC).

**Steps:**
1. Gameplay drawlog golden: extend the `RB3_DRAWLOG`/`/api/drawlog` capture to a **gameplay scene
   with crowd + drum in frame**. Reuse the nav+camera-pin pattern from `band-closeup-capture.py`
   (`rb3_director_disable` FIRST, then `rb3_force_shot "<name>"`) and `song-end-test.py`, under
   `RB3_FIXED_CLOCK` (deterministic sim clock, W0.3b) for a stable draw multiset. Capture via
   `RB3_DRAWLOG_DUMP` → JSON (`DumpDrawLog`, `:4258`).
2. The **"right, not just different" oracle** (`test_placement_oracle.cpp`): read the crowd draw
   records (`skinned` flag set, crowd mesh-name-hashes) via `RB3DebugGetDrawLog` (`RB3DrawLogDebug.h`)
   and assert, against the `RB3_PLACEMENT_PROBE` spXfm log:
   - each crowd instance's drawn `obj.world` **translation** matches the `spXfm.v` it was posed with
     (per-instance, within eps);
   - crowd instances are **pairwise-distinct** and **span the bowl** (bbox extent >> 0, not
     all-equal — REFACTOR_PLAN Phase-2 exit);
   - the **drum prop-mesh world ≠ origin** AND consistent with the drummer's bone/waypoint world
     (W0.4-effector-style).
3. **Fail-red on the CURRENT build (FREE):** run S1's oracle against the UNCHANGED engine. It MUST go
   **RED** — today every skinned instance logs `obj.world = identity` (translation 0), so the
   distinct/spans-bowl and translation-match asserts fail. **That red is the proof the gate sees the
   bug.** Record the RED output in STATUS.md as the fail-red demo.
4. Reviewer-judged wides (documentation, not a blocking assert): fresh Dolphin gameplay `t2` captures
   + existing `docs/native/engine-arch-review-2026-07-05/c8-ground-truth-2026-07-01/dolphin-shots/gp_*.png`
   (gp_00 shows the crowd spread house-left) + `images/retail-screenshots/` gameplay shots for the
   drum-kit position.

**Verification commands:**
- `cmake -B native/build-agent-W2.1 -S native -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ && cmake --build native/build-agent-W2.1 --target rb3-native rb3-tests`
- `RB3_HTTP=1 RB3_FIXED_CLOCK=1 python3 scripts/native/placement-gate-capture.py` → produces the
  gameplay drawlog + spXfm probe log.
- `native/build-agent-W2.1/rb3-tests --gtest_filter='PlacementOracle.*'` → **RED on current build**
  (documented), the gate's fail-red.

**Exit (S1):** oracle gtest exists, is wired into `rb3-tests`, and is proven RED on the unchanged
`6221a56` build. No behavior change committed in this subtask.

### W2.1.S2 — The coupled placement contract behind `RB3_PLACEMENT_CONTRACT` (default-OFF)  · model: opus
**Goal:** land Half A + Half B atomically behind ONE registered default-OFF flag; flag-OFF
byte-identical; flag-ON turns S1's oracle GREEN with band/hands invariance nets still green.

**Files:**
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` — `DrawMesh` obj.world arm (`:2847–2848`) +
  palette compose (`:3159–3160`/`:3315`), gated by a cached-static `RB3_PLACEMENT_CONTRACT` read.
- `milo-native-engine/src/platform/NativeCompatFlags.classification.json` — register
  `RB3_PLACEMENT_CONTRACT` = `{"class":"feature","read":"presence","default":"off",
  "owner":"render/placement","faithfulStatus":"not-live: SYS-1 placement contract, default-OFF pending
  coordinator flip"}`.
- `milo-native-engine/src/platform/NativeCompatFlags.gen.inc` — regenerate via
  `scripts/analysis/native_compat_census.py` (do NOT hand-edit).

**Steps:**
1. Register the flag in `classification.json`; regenerate `gen.inc`; run
   `python3 scripts/analysis/native_compat_census.py check` → **exit 0** (registry ⊇ every getenv).
2. Add the cached-static flag read at the top of the obj.world/palette region (mirroring `:3083–3084`).
3. **Half A + Half B in ONE commit** (MOVE-xor-CHANGE rule 1 → this is a CHANGE commit; no MOVE mixed
   in): under flag-ON, `obj.world = MiloXfmToColMajor(mesh->WorldXfm())` for the skinned arm AND the
   palette composed bind-relative; flag-OFF path is the verbatim current code. **No intermediate
   commit may have one half live without the other.**
4. Iterate the exact palette operand order against S1's oracle + the invariance nets until: crowd
   drawn-world == spXfm, drum ≠ origin, AND `SkinGolden.*`/`HandsBindOracle`/`ClipPoseFixture.*`
   stay green and the canonical splash A/B is 0-unexpected on BOTH flag states (band meshWorld≈I).
   Use the `:2797–2800` skew comment as the guardrail (a Half-A-only build must skew — confirm, then
   add Half B).

**Verification commands:**
- Census: `python3 scripts/analysis/native_compat_census.py check` → exit 0.
- Engine invariance suite: `DC3_DATA=/home/free/code/milohax/dc3-decomp/orig-assets
  MILO_LIB=/home/free/code/milohax/dc3-decomp/orig-assets/extracted ctest -j1` in the engine test
  build → **198/0/2** (bar). `SkinGolden.*` + `ClipPoseFixture.*` green.
- `rb3-tests --gtest_filter='HandsBindOracle.*:DrawLogGolden.*'` green.
- **flag-OFF byte-identical:** `python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order`
  (splash) 0-unexpected; `python3 scripts/native/lineup-gate.py` PASS all layers.
- **flag-ON green:** `RB3_PLACEMENT_CONTRACT=1 native/build-agent-W2.1/rb3-tests
  --gtest_filter='PlacementOracle.*'` → GREEN (crowd spread == spXfm, drum ≠ origin).

**Exit (S2):** flag registered (census 0), flag-OFF byte-identical (splash canonical 0-unexpected +
lineup PASS + rb3-tests + engine 198/0), flag-ON oracle GREEN, invariance nets green. Committed
default-OFF; NO flip.

### W2.1.S3 — flag-ON verification package + exit-gate evidence (A/A discipline)  · model: opus
**Goal:** produce the full flag-ON exit evidence with the correct verify protocol (B2) and the
name-scoped-hack no-op proofs, so the coordinator can gate the separate flip.

**Files:** none in engine source (READ-ONLY verify) except appending STATUS.md; may add capture
outputs under `execution/W2.1/` (screenshots, A/B logs).

**Steps:**
1. **B2 verify protocol (CRITICAL — do NOT carry the W1.6-era rule):** W2.1 IS a world-alterer and
   the CharEyes/CharLookAt residual meshes are exactly the class it changes. Do NOT classify
   residual-name world divergences as non-blocking. Run **A/A controls** (baseline-vs-baseline, flag
   OFF vs OFF; then flag ON vs ON) to re-separate genuine eye-flake (pre-existing, W0.3d residual)
   from W2.1 effects. Only A/A-invariant eye divergences may be attributed to the residual; any
   NEW world divergence under flag-ON on a non-crowd/non-drum mesh is a W2.1 bug to investigate.
2. **Name-scoped placement-hack opt-outs proven NO-OPS under flag-ON:** run flag-ON with each of
   `RB3_NO_HUB_BAR_PLACEMENT_FIX=1`, `RB3_SCROLLBAR_THUMB_FIX_OFF=1`, `RB3_NO_CROWD_REBIND=1`
   (if defined) and show byte-identical output vs flag-ON-without-them (the contract subsumes them).
3. **song_select hub-bar / scrollbar A/B** (those injections live INSIDE the edited `:2833–2851`
   block — invisible to every gameplay gate): capture `/api/screenshot` at song_select, flag-OFF vs
   flag-ON, and confirm the hub highlight bar sits behind the focused item + scrollbar thumb tracks
   the list (retail parity, no skew/origin-collapse regression).
4. **Skinning/hands nets green:** `HandsBindOracle`, W0.5 lineup, hand-closeup harness — flag-ON.
5. **Crowd SKIN_CLAMP negative control:** `RB3_NO_SKIN_CLAMP` / clamp-count vs `W2.2/char/
   parsed-default.json` baseline unchanged under flag-ON (the placement change must not alter the
   crowd/extras clamp population).
6. **Canonical splash regression net:** flag-ON splash canonical comparator = count/bind-group/
   mesh-identity nets intact (world axis saturates by design — use the other axes).

**Verification commands:** (all against the S2 build)
- `RB3_PLACEMENT_CONTRACT=1 python3 scripts/native/song-select-capture.py` (or `/api/screenshot`) →
  hub-bar/scrollbar A/B PNGs under `execution/W2.1/`.
- A/A: two flag-OFF captures + two flag-ON captures via `drawlog-golden.py --determinism-check`.
- `RB3_PLACEMENT_CONTRACT=1 RB3_NO_HUB_BAR_PLACEMENT_FIX=1 …` diff vs `RB3_PLACEMENT_CONTRACT=1`.

**Exit (S3):** full evidence package in STATUS.md + `execution/W2.1/`: A/A-separated eye residual,
three opt-outs proven no-ops, song_select A/B parity, hands/lineup nets green, SKIN_CLAMP control
unchanged. Coordinator-gated flip left for a separate one-line commit (NOT done here).

## Exit criteria (item-level, measurable)

**flag-OFF (`RB3_PLACEMENT_CONTRACT` unset) — byte-identical:**
- Canonical splash drawlog A/B (`drawlog-golden.py --fixed-clock --canonical-order`): **0 unexpected**.
- W0.5 lineup-gate: **PASS all layers** (img/segA/ratioB/countC/pin).
- `rb3-tests` green; engine `milo-engine-tests` **198 pass / 0 fail / 2 skip** (`ctest -j1`,
  `DC3_DATA`+`MILO_LIB`).
- `native_compat_census.py check`: **exit 0** (flag registered).

**flag-ON (`RB3_PLACEMENT_CONTRACT=1`) — placement correct:**
- S1 placement oracle GREEN: each crowd instance's drawn `obj.world` translation **== its `spXfm.v`**
  (within eps); instances pairwise-distinct and **span the bowl**; drum prop-mesh world **≠ origin**
  and consistent with the drummer bone/waypoint.
- The gate was proven **RED on the pre-change build** (fail-red is free).
- `SkinGolden.*` + `HandsBindOracle` + `ClipPoseFixture.*` green (band/hands invariance held).
- `RB3_NO_HUB_BAR_PLACEMENT_FIX` / `RB3_SCROLLBAR_THUMB_FIX_OFF` / `RB3_NO_CROWD_REBIND` proven
  **no-ops** under flag-ON.
- song_select hub-bar/scrollbar screenshot A/B: retail parity (bar behind focus, thumb on track).
- Crowd SKIN_CLAMP negative control unchanged.
- Reviewer-judged Dolphin `gp_*` / retail gameplay wides consistent with crowd spread + drum position.

**Staging:** ONE registered engine-side flag gating BOTH halves atomically; no intermediate commit
with one half live; the default-ON flip is a SEPARATE coordinator-gated commit (NOT in this item).

## Files touched (exact repo-relative paths; coordinator cross-diffs lanes)

**Engine (`milo-native-engine`):**
- `src/platform/Rnd_Wgpu_RB3.cpp` — `DrawMesh` obj.world skinned arm (`:2847–2848`) + bone-palette
  compose (`:3159–3160`, `:3315`) + cached-static flag read. **Shared with W2.3 → sequential.**
- `src/platform/NativeCompatFlags.classification.json` — register `RB3_PLACEMENT_CONTRACT`.
- `src/platform/NativeCompatFlags.gen.inc` — regenerated (not hand-edited).

**rb3 (`rb3`):**
- `scripts/native/placement-gate-capture.py` — NEW gameplay drawlog capture harness.
- `native/tests/test_placement_oracle.cpp` — NEW (or extend `native/tests/test_draw_log_golden.cpp`).
- `native/tests/goldens/drawlog/gameplay_crowd.json` (+ `.fixedclock-residual.json`) — NEW golden.
- `native/tests/CMakeLists.txt` — wire the new test/golden if a new file is added.
- `src/system/world/Crowd.cpp` — `HX_NATIVE`+`RB3_PLACEMENT_PROBE`-gated diagnostic fprintf at
  `:403` (spXfm dump). Diagnosis-only, default-OFF, Wii-compile-inert.
- `docs/native/engine-arch-review-2026-07-05/execution/W2.1/{PLAN,STATUS}.md` + captured A/B outputs.

**Off-limits:** `src/App.cpp` (rule); the sibling uncommitted `FxSendNative.cpp` engine edit (leave
untouched).

## Risks / conflicts

- **Lane A W2.1 → W2.3 sequential (HARD).** Both edit the same `DrawMesh` region (`:2807–3316`).
  W2.1 defines the placement contract (what `obj.world` and the palette basis each encode); W2.3's
  "read the drawn mesh's own bones" only has a defined correctness contract once W2.1 settles it. Do
  NOT run concurrently — two agents holding `Rnd_Wgpu_RB3.cpp` open lose edits (rules 2/4/7/8).
- **Biggest behavior surface of the campaign.** The edited arm is every skinned draw. Mitigation:
  default-OFF flag + band/hands invariance nets forcing meshWorld≈I meshes byte-identical + the S1
  oracle proving crowd/drum correctness. History (2× BandPatchMesh reverts, `RB3_BOUND_REBAKE`
  200–460u) is this bug family — the flag makes the worst case "an unflipped flag," never a blind
  revert.
- **Coupled-half trap.** obj.world=WorldXfm() WITHOUT bind-relative palette SKEWS (`:2797–2800`).
  Atomic single-commit gating both halves; no one-half-live intermediate.
- **Verify-protocol regression.** The W1.6-era "residual-name world failures = non-blocking" rule
  MUST NOT carry over (W2.1 alters those meshes' worlds) — S3 uses A/A controls (B2).
- **UI regression invisible to gameplay gates.** Hub-bar/scrollbar injections live in the edited
  block → S3's song_select A/B is mandatory.
- **W0.3d (Lane B, parallel) is diagnosis-only wrt Lane-A files** — if its async-loader fix wants
  `Rnd_Wgpu_RB3.cpp`/the object-list path, it STOPs and hands to the coordinator for post-Lane-A
  sequencing (F1). No contention on golden files: W2.1 lands default-OFF so the committed splash
  golden stays valid.
- **W2.6 (Lane C) is rb3-only, engine READ-ONLY** — no collision with W2.1's engine edits.
- **Flag hygiene (F2):** engine-side flag registered at introduction (census exit 0); no `App.cpp`;
  `FxSendNative.cpp` untouched; per-lane exact-file list above for coordinator cross-diff.
