# W2.3 — Kill shared-GeomOwner skeleton aliasing for crowd + props (SYS-1 bone-source half)

**Planner:** Opus. **Wave 4, Lane A item 2 (engine).** Sequential AFTER W2.1 (both edit
`Rnd_Wgpu_RB3.cpp` `DrawMesh`; W2.1 landed on engine `6852caa` and defines the placement contract
W2.3 layers on). Engine pin `6221a56` in rb3 (do NOT bump); engine working-tree HEAD is `6852caa`
(post-W2.1). Status: PLAN (not yet implemented).

## Objective

Fix the SYS-1 **bone-source** half for crowd (and skinned bone-attached props): `DrawMesh` builds
the GPU bone palette from `owner = mesh->GeomOwner()` — `owner->NumBones()` /
`owner->BoneTransAt(b)` / `owner->BoneOffsetAt(b)` — **never from the drawn instance's own bones**.
`mBones` (and therefore `NumBones`/`BoneTransAt`/`BoneOffsetAt`) is a **per-mesh** member
(`RndMesh::mBones`, `src/system/rndobj/Mesh.h:359`, `:240/:256/:257`), so an instance that shares
GEOMETRY through `mGeomOwner` but carries its own bones has those bones ignored. When two co-resident
worlds (gameplay venue + tv3 intro vignette) hold same-named crowd archetypes drawing through a
**shared** GeomOwner master mesh with a single `mBones`, every instance reads the last-posed owner's
bones → the crowd renders bunched / sharded. Make the draw read **the drawn mesh's own bones when it
has them**, so the latched draw-time `RebindCrowdCharBonesToOwnSkeleton` (which today papers over the
seam by mutating the owner) becomes unnecessary.

**Scope discipline (A1 — R5 pattern, BINDING):** `RebindCrowdCharBonesToOwnSkeleton`
(`rb3/src/system/world/Crowd.cpp:929`, impl `:929–1064`, called at `Draw3DChars` `:409`) is
**RETAINED this wave** — default-REPLACED, not removed. The engine change lands behind its own
default-OFF flag; the rebind is proven a no-op under flag-ON as an exit measurement, and deleted only
in a LATER wave once its opt-out `RB3_NO_CROWD_REBIND` is proven inert. **No `Crowd.cpp` source edit
this wave** (it is toggled only via its existing `RB3_NO_CROWD_REBIND` env for the negative control).

### What W2.1 already did, and what is LEFT for W2.3 (the layering — VERIFIED)

- **W2.1 (landed `6852caa`, default-OFF `RB3_PLACEMENT_CONTRACT`) fixed PLACEMENT:** for skinned
  meshes `obj.world = mesh->WorldXfm()` (Half A, `Rnd_Wgpu_RB3.cpp:2896`) + a bind-relative palette
  (Half B, `:3181`). Its verifier MEASURED (W2.1/STATUS.md) that under flag-ON the crowd mesh
  `0xc57f0ca822620500` moves `obj.world (0,0,0) → bowl-spread` (draw count 211→115) and the drum kit
  places correctly (two cymbals house-right). **So "crowd/drum all at one POINT" is W2.1's win.**
- **W2.1 did NOT fix the bone aliasing/sharding.** Its verifier explicitly recorded that
  `RB3_NO_CROWD_REBIND` "must be RETAINED at the flip — it fixes genuinely-broken meshes" (W2.1/
  STATUS.md, Deviations #1). The palette is still composed from `owner->BoneTransAt/BoneOffsetAt`
  (`:3193`/`:3249`), and the crowd's correctness still depends on the latched rebind mutating that
  owner. **That residual is W2.3's target.**

### Faithful-reference citations (VERIFIED this planning pass against engine `6852caa`)

- **Bone data is per-mesh, geometry is per-owner** — `rb3/src/system/rndobj/Mesh.h`:
  `int NumBones() const { return mBones.size(); }` (`:240`), `BoneTransAt(idx){return mBones[idx].mBone;}`
  (`:256`), `BoneOffsetAt(idx){return mBones[idx].mOffset;}` (`:257`), `IsSkinned(){return !mBones.empty();}`
  (`:251`); `ObjOwnerPtr<RndMesh> mGeomOwner` (`:357`) shares only `mVerts`/`mFaces`/`mVolume`
  (`Verts()`/`Faces()` at `:247–250`), **not** `mBones` (`:359`).
- **The bug site — palette reads OWNER, not the drawn mesh** — `Rnd_Wgpu_RB3.cpp`:
  `RndMesh* owner = mesh->GeomOwner(); if(!owner) owner = mesh;` (`:2137`); `bool skinned =
  owner->IsSkinned();` (`:2257`); `int numBones = owner->NumBones();` (`:2947`); the REAL compose loop
  `for (int b=0; b<numBones; b++){ RndTransformable* bt = owner->BoneTransAt(b); ... Multiply(
  owner->BoneOffsetAt(b), wt, skin); ... }` (`:3193` bt, `:3249` compose). The V24 shard guard and
  SKIN_CLAMP also read `owner` (`:3317–3385`, `:3844+`).
- **Ground truth that the fix is needed, and its two competing mechanisms** —
  `rb3/src/system/world/Crowd.cpp` `RebindCrowdCharBonesToOwnSkeleton` (`:929`). **The function's own
  comments state TWO CONTRADICTORY root causes** and must be reconciled empirically (see Risks):
  - **Header comment (`:884–905`): SHARED GeomOwner.** "Both archetypes' same-named body mesh
    INSTANCES draw through the SAME GeomOwner master mesh, which has a single shared mBones array …
    the engine reads the OWNER's bones — never the drawn instance's … whichever world posed the
    shared owner last wins." Under this theory, reading the drawn mesh's own bones **is** the fix.
  - **Inline comment (`:1032–1048`): SELF-owned, OFFSET poison.** "For these crowd meshes
    GeomOwner()==self (verified — NOT shared … despite the same archetype names), and the bones
    already ARE this archetype's own … The poison is the OFFSET: each mesh's authored inverse-bind
    BoneOffsetAt does NOT match the native skeleton's bind pose." Under this theory, reading own bones
    is a **no-op** and only the offset rebake helps.
  - **The LANDED code** does `owner->SetBone(b, own, /*calcOffset=*/true)` (`:1045`, offset REBAKE)
    latched via `owner->mNativeBonesRebound` (`:1049`), and resolves `own = curChar->Find(bound->Name(),
    false)` (`:1041`) with a `diffInstance` counter (`:1043`) that measures how often `own != bound`
    (the shared-owner signal). `calcOffset=true` being load-bearing is strong evidence the OFFSET
    poison is real; `diffInstance`/the Find-by-name repoint is the shared-owner de-alias. **Both may
    be in play; only a runtime probe (S1) resolves which dominates for the drawn crowd meshes.**
- **Band members are the invariance anchor:** a band character mesh is self-owned
  (`GeomOwner()==self`) so `boneSrc == owner == mesh` regardless of the flag → **byte-identical** for
  band. `SkinGolden.*` + `HandsBindOracle` + `ClipPoseFixture.*` PIN this.
- **2D crowd imposter path is correct and OUT OF SCOPE** — the `RndMultiMesh mMMesh` billboard path
  (`Crowd.cpp:101,125,185,261…`, drawn via the multimesh instance list) never enters the skinned
  `DrawMesh` bone-palette branch. W2.3 touches only the 3D skinned palette → the imposter path is
  byte-identical by construction (ARCHITECTURE_REVIEW: "2D crowd imposter path — correct").
- **Flag registry** — `src/platform/NativeCompatFlags.classification.json` (top-level keyed by flag
  name); `RB3_PLACEMENT_CONTRACT` template at `:90`. Read idiom = cached-static `getenv`
  (release-safe, NOT `ProbeActive`). Census `scripts/analysis/native_compat_census.py check` scans
  engine `getenv` sites → must exit 0.

## Design of the bone-source de-alias (S2, conditional on S1)

Introduce ONE engine flag **`RB3_CROWD_OWN_BONES`** (class:feature, default:off). When ON, route the
REAL palette compose through the drawn mesh's own bones when it has them:

```cpp
// boneSrc: with the flag ON, a drawn instance that carries its own bones supplies the palette;
// flag-OFF (and any mesh that has no own bones) → owner, i.e. today's behavior verbatim.
RndMesh* boneSrc = (sCrowdOwnBones && mesh->NumBones() > 0) ? mesh : owner;
```

and replace `owner->NumBones()/BoneTransAt(b)/BoneOffsetAt(b)` with `boneSrc->…` in **the compose
loop only** (`:2947`, `:3193`, `:3249`, and the V24/SKIN_CLAMP measure that gates the SAME composed
skin, `:3317–3385`, `:3844+`). The owner-mutating heuristic pre-passes (SKEL_REBAKE `:3029+`) keep
reading `owner` (they only matter when `boneSrc==owner`); when `boneSrc==mesh` those owner mutations
no longer feed the palette — which is exactly the intended "rebind becomes unnecessary" direction.

**flag-OFF ⇒ `boneSrc==owner` ⇒ byte-identical.** Band ⇒ `owner==mesh` ⇒ byte-identical both states.
The EXACT set of read sites to reroute (compose vs. every diagnostic) is finalized in S2 against S1's
probe + the invariance nets — diagnostics may stay on `owner`; only the palette-feeding reads must
move. **This is a CHANGE commit (rule 1); no MOVE mixed in; both the compose reroute and the flag land
together, no intermediate half-state.**

**Critical dependency on S1 (the thesis may be refuted):** if S1 proves crowd `GeomOwner()==self`
(`owner==mesh` pointer-identical) **and** the drawn mesh's own bones are offset-poisoned (own-bone
mesh-local skin extent still > the SKIN_CLAMP threshold), then `boneSrc==mesh==owner` is a literal
no-op and the sharding is a pure OFFSET-rebake problem that "read own bones" cannot touch. In that
case S2 does NOT land a no-op flag: it STOPS and hands the coordinator the finding that W2.3's
bone-source thesis is refuted for crowd, with the offset-rebake generalization scoped as a separate
future item (the rebind is retained as still-load-bearing). This honest branch is an accepted outcome.

## Subtasks

### W2.3.S1 — Characterize the crowd bone-source seam + build the W2.3 gate; prove fail-red  · model: opus
**Goal:** empirically DECIDE whether reading the drawn mesh's own bones can make the rebind
unnecessary, and stand up the negative-control gate that SEES crowd sharding — proven RED on the
current (unchanged) build. **No behavior change in this subtask.**

**Files (create/extend):**
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` — an env-gated (`RB3_CROWD_BONE_PROBE`),
  diagnosis-only probe in the skinned branch of `DrawMesh` (its own commit, no behavior change): for
  each skinned crowd/extras mesh dump `mesh` ptr, `owner=GeomOwner()` ptr, `owner==mesh?`,
  `mesh->NumBones()` vs `owner->NumBones()`, per-bone `owner->BoneTransAt(b)==mesh->BoneTransAt(b)?`
  (de-alias delta / `diffInstance`-equivalent), and the worst-bone **mesh-local skin extent computed
  from the DRAWN mesh's OWN `BoneOffsetAt(b)*BoneTransAt(b)->WorldXfm()`** vs the owner's — i.e. would
  the own-bone palette PASS the SKIN_CLAMP (`>12u` = shard) or not.
- `rb3/native/tests/test_placement_oracle.cpp` (extend) or new `rb3/native/tests/test_crowd_bone_oracle.cpp`
  wired into `rb3-tests` — the W2.3 negative-control oracle: assert that in a captured gameplay
  drawlog, crowd instances stay **pairwise-distinct + span the bowl** (inheriting W2.1's placement
  oracle) AND the crowd **shard/fallback-drop signal is not elevated** vs the rebind-ON baseline.
- `rb3/scripts/native/crowd-bone-gate-capture.py` (new, or extend `placement-gate-capture.py`) —
  camera-pinned gameplay capture (nav pattern from `band-closeup-capture.py`/`song-end-test.py`,
  `RB3_FIXED_CLOCK`, `rb3_director_disable`→`rb3_force_shot`) that records the drawlog + the
  `SKIN_CLAMP_PROBE`/`sFallbackBones` crowd shard-drop counts under two conditions.

**Steps:**
1. Build `rb3-native`+`rb3-tests` in `native/build-agent-W2.3`. Capture the crowd probe on the
   current build under **rebind-OFF** (`RB3_NO_CROWD_REBIND=1`) with W2.1 flag both OFF and ON:
   read out **(Q1)** is `owner==mesh` (self) or `owner!=mesh` / `diffInstance>0` (shared)? and
   **(Q2)** does the DRAWN mesh's own-bone palette pass the SKIN_CLAMP (clean) or exceed it (offset
   poison)? Record the raw numbers in STATUS.md.
2. Build the negative-control gate: with the rebind ON (baseline) capture the crowd instances'
   distinct/spread + the shard-drop count; assert the oracle GREEN there.
3. **Fail-red on the CURRENT build (FREE):** run the SAME oracle with `RB3_NO_CROWD_REBIND=1`
   (rebind OFF) on the unchanged engine → the crowd sharding/co-bunching returns (shard-drop count
   spikes and/or bodies drop) → the gate MUST go **RED**. Record the RED output as the fail-red demo.
   (This is the exact condition W2.3-flag-ON must later make GREEN without the rebind.)
4. **Write the DECISION** in STATUS.md: `SHARED` (Q1 shared, own bones clean) → S2 proceeds, bone-
   source de-alias is the fix. `SELF+POISON` (Q1 self, Q2 poisoned) → W2.3 thesis REFUTED for crowd
   → S2 stops, escalate to coordinator (offset-rebake is a separate item; rebind retained). `MIXED`
   → S2 lands the de-alias for the shared subset and documents the residual offset dependence.

**Verification commands:**
- `cmake -B native/build-agent-W2.3 -S native -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ && cmake --build native/build-agent-W2.3 --target rb3-native rb3-tests`
- `RB3_HTTP=1 RB3_FIXED_CLOCK=1 RB3_CROWD_BONE_PROBE=1 RB3_NO_CROWD_REBIND=1 python3 scripts/native/crowd-bone-gate-capture.py` → Q1/Q2 readout.
- `native/build-agent-W2.3/rb3-tests --gtest_filter='CrowdBoneOracle.*'` → **RED with `RB3_NO_CROWD_REBIND=1` on current build** (documented fail-red); GREEN with rebind ON.

**Exit (S1):** probe + oracle wired into `rb3-tests`, fail-red proven RED on unchanged `6852caa`, and
the SHARED / SELF+POISON / MIXED DECISION recorded — S2's design is now determined. No behavior change
committed.

### W2.3.S2 — Engine bone-source de-alias behind `RB3_CROWD_OWN_BONES` (default-OFF)  · model: opus
**Goal:** land the `boneSrc` reroute atomically behind ONE registered default-OFF flag; flag-OFF
byte-identical; flag-ON turns S1's negative-control gate GREEN **without** the rebind, with band/hands
invariance nets still green. **Gated on S1's DECISION ≠ SELF+POISON** (else STOP + escalate).

**Files:**
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` — cached-static `RB3_CROWD_OWN_BONES` read; the
  `boneSrc` selection; reroute the palette-feeding `NumBones/BoneTransAt/BoneOffsetAt` reads
  (`:2947`, `:3193`, `:3249`, V24/SKIN_CLAMP measure) from `owner` to `boneSrc`.
- `milo-native-engine/src/platform/NativeCompatFlags.classification.json` — register
  `RB3_CROWD_OWN_BONES` = `{"class":"feature","read":"presence","default":"off",
  "owner":"render/crowd","faithfulStatus":"not-live: SYS-1 crowd bone-source de-alias, default-OFF
  pending coordinator flip"}`.
- `milo-native-engine/src/platform/NativeCompatFlags.gen.inc` — regenerate via
  `scripts/analysis/native_compat_census.py` (do NOT hand-edit).

**Steps:**
1. Register the flag; regenerate `gen.inc`; `python3 scripts/analysis/native_compat_census.py check`
   → **exit 0**.
2. Add the cached-static flag read + `boneSrc` selection at the top of the skinned palette region
   (mirroring the `sWorldFixOff` idiom `:3141`).
3. Reroute the palette-feeding reads to `boneSrc` in ONE CHANGE commit. flag-OFF keeps `owner`
   verbatim. Verify no intermediate half-state.
4. Iterate against S1's gate + invariance nets until: flag-ON + `RB3_NO_CROWD_REBIND=1` → crowd
   distinct/spread + shard-drop NOT elevated (gate GREEN), AND `SkinGolden.*`/`HandsBindOracle`/
   `ClipPoseFixture.*` green + canonical splash A/B 0-unexpected on BOTH flag states (band
   `owner==mesh` → identical).

**Verification commands:**
- Census: `python3 scripts/analysis/native_compat_census.py check` → exit 0.
- Engine invariance suite: `DC3_DATA=/home/free/code/milohax/dc3-decomp/orig-assets
  MILO_LIB=/home/free/code/milohax/dc3-decomp/orig-assets/extracted ctest -j1` in the engine test
  build → **198/0/2** (bar). `SkinGolden.*` + `ClipPoseFixture.*` green.
- `rb3-tests --gtest_filter='HandsBindOracle.*:DrawLogGolden.*:CrowdBoneOracle.*'` green (last one
  GREEN under flag-ON+rebind-OFF).
- **flag-OFF byte-identical:** `python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order`
  (splash) 0-unexpected; `python3 scripts/native/lineup-gate.py` PASS all layers.
- **flag-ON green:** `RB3_CROWD_OWN_BONES=1 RB3_NO_CROWD_REBIND=1 native/build-agent-W2.3/rb3-tests
  --gtest_filter='CrowdBoneOracle.*'` → GREEN.

**Exit (S2):** flag registered (census 0), flag-OFF byte-identical (splash canonical 0-unexpected +
lineup PASS + rb3-tests + engine 198/0), flag-ON gate GREEN with rebind OFF, invariance nets green.
Committed default-OFF; NO flip. (Or, if S1=SELF+POISON: no commit, escalation recorded.)

### W2.3.S3 — flag-ON verification package + rebind-no-op measurement + negative controls (A1/R5, A/A discipline)  · model: opus
**Goal:** produce the flag-ON exit evidence with the correct A/A verify protocol, prove the rebind is
a no-op under flag-ON (or honestly document the residual), and run the required negative controls.
Engine READ-ONLY (verify only; append STATUS.md + capture outputs under `execution/W2.3/`).

**Steps:**
1. **A1 / R5 — rebind-no-op measurement:** with `RB3_CROWD_OWN_BONES=1`, compare crowd rendering with
   `RB3_NO_CROWD_REBIND=1` (rebind OFF) vs unset (rebind ON): distinct/spread + shard-drop count must
   be equivalent. If equivalent → thesis proven, rebind is a no-op under flag-ON → RETAIN this wave,
   file its removal for a later wave. If NOT equivalent (residual offset dependence) → honest partial:
   de-alias landed, rebind RETAINED as still-needed; document the residual.
2. **A/A discipline (per W2.1's B2):** W2.3 can alter crowd skin-vertex world positions, so run
   baseline-vs-baseline A/A (flag-OFF twice) AND flag-OFF-vs-ON A/B to separate genuine W2.3 effect
   from the pre-existing CharEyes/CharLookAt eye-flake + bloom/exposure wash the W2.1 verifier
   documented. Only crowd/extras skinned meshes may differ A/B.
3. **Negative controls (REQUIRED):**
   - **2D crowd imposter path byte-identical** — the `RndMultiMesh` billboard path never enters the
     skinned palette branch; confirm its draw records are byte-identical flag-OFF vs ON.
   - **crowd/extras SKIN_CLAMP counts unchanged vs baseline** — compare the flag-ON (rebind-ON) clamp/
     `sFallbackBones` counts against the current baseline (capture a fresh baseline; the
     `W2.2/char/parsed-default.json` characterization is the reference format). The clamp must not
     newly fire on any mesh it did not fire on before.
4. Reviewer-judged Dolphin wides (documentation, non-blocking): fresh gameplay `t2` captures +
   `docs/native/engine-arch-review-2026-07-05/c8-ground-truth-2026-07-01/dolphin-shots/gp_*.png`
   (gp_00 crowd spread) + `images/retail-screenshots/` for crowd density/pose.

**Verification commands:**
- `RB3_CROWD_OWN_BONES=1 [RB3_NO_CROWD_REBIND=1] python3 scripts/native/crowd-bone-gate-capture.py`
  (rebind-no-op A/B).
- A/A: two flag-OFF captures + one flag-ON via the same harness; diff with the canonical comparator.
- 2D imposter + SKIN_CLAMP negative controls via the probe/drawlog.

**Exit (S3):** rebind-no-op adjudicated (proven no-op OR residual documented), A/A shows only
crowd/extras skinned meshes differ A/B, 2D imposter byte-identical, SKIN_CLAMP counts unchanged. A
go/no-go package for the coordinator-gated default-ON flip is recorded in STATUS.md. NO flip in this
item.

## Exit criteria (measurable)

- **flag-OFF (`RB3_CROWD_OWN_BONES` unset) byte-identical:** canonical splash drawlog A/B 0-unexpected
  (`drawlog-golden.py --fixed-clock --canonical-order`); `lineup-gate.py` PASS all layers; `rb3-tests`
  green; engine `ctest -j1` 198/0/2.
- **Band/hands invariance:** `SkinGolden.*`, `HandsBindOracle.*`, `ClipPoseFixture.*` green in BOTH
  flag states (band `owner==mesh` ⇒ identical).
- **flag-ON gate GREEN with the rebind OFF:** `RB3_CROWD_OWN_BONES=1 RB3_NO_CROWD_REBIND=1` →
  `CrowdBoneOracle.*` GREEN (crowd instances pairwise-distinct + span the bowl + shard-drop count NOT
  elevated vs the rebind-ON baseline). This is the S1 fail-red condition turned GREEN.
- **Fail-red intact:** the S1 oracle is RED with rebind OFF on the flag-OFF/unchanged build.
- **A1 / R5:** `RebindCrowdCharBonesToOwnSkeleton` RETAINED (no `Crowd.cpp` source edit); its opt-out
  proven a no-op under flag-ON, OR the residual it still fixes explicitly documented.
- **Negative controls:** 2D crowd imposter path byte-identical flag-OFF vs ON; crowd/extras SKIN_CLAMP
  counts unchanged vs baseline.
- **Census:** `native_compat_census.py check` exit 0 (`RB3_CROWD_OWN_BONES` registered at introduction).
- **Honest-refutation branch is a valid exit:** if S1 proves SELF+POISON, W2.3 lands NO engine change;
  the DECISION + escalation to the coordinator (offset-rebake as a separate item, rebind retained) is
  the recorded outcome.

## Files touched

- `docs/native/engine-arch-review-2026-07-05/execution/W2.3/PLAN.md` (this file)
- `docs/native/engine-arch-review-2026-07-05/execution/W2.3/STATUS.md` (append-only log)
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (S1 probe; S2 `boneSrc` reroute + flag read)
- `milo-native-engine/src/platform/NativeCompatFlags.classification.json` (S2 flag registration)
- `milo-native-engine/src/platform/NativeCompatFlags.gen.inc` (S2 regenerate — do NOT hand-edit)
- `rb3/native/tests/test_placement_oracle.cpp` (extend) OR `rb3/native/tests/test_crowd_bone_oracle.cpp` (new)
- `rb3/native/tests/CMakeLists.txt` (only if a new test file is added to `rb3-tests`)
- `rb3/scripts/native/crowd-bone-gate-capture.py` (new) OR extension of `scripts/native/placement-gate-capture.py`
- capture outputs (screenshots / A-B logs) under `docs/native/engine-arch-review-2026-07-05/execution/W2.3/`

**NOT touched (BINDING):** `rb3/src/system/world/Crowd.cpp` (rebind RETAINED, toggled only via its
existing `RB3_NO_CROWD_REBIND` env); `src/App.cpp`; the sibling `FxSendNative.cpp` edit.

## Risks / conflicts

- **THESIS-REFUTATION RISK (highest — the reason S1 is first).** The landed `Crowd.cpp` carries two
  contradictory root-cause comments (shared-GeomOwner `:884–905` vs self-owned offset-poison
  `:1032–1048`) and uses `calcOffset=true` (offset rebake) — strong evidence the OFFSET poison is
  real. If crowd `GeomOwner()==self` AND the drawn mesh's own bones are offset-poisoned, "read own
  bones" is a NO-OP and W2.3's engine change cannot make the rebind unnecessary. S1 MUST resolve this
  empirically BEFORE any behavior change; S2 STOPS and escalates on the SELF+POISON verdict rather
  than land a placebo flag. This is an accepted, honest outcome — do not force a change to fit the
  brief.
- **Lane A sequencing (W2.1 → W2.3 on `Rnd_Wgpu_RB3.cpp`).** W2.1 landed on `6852caa`; W2.3 edits the
  SAME skinned palette region (`:2947`/`:3193`/`:3249`) and the W2.1 contract's bind-relative palette
  (`:3181`). Serial in one lane is correct — never hold the file concurrently with another agent
  (hard rules 2/4/7/8). W2.3's `boneSrc` reroute must compose cleanly with W2.1's `contractReorg`
  path (both read `owner->BoneOffsetAt`; when `boneSrc==mesh` the reorg must use `boneSrc`'s offset
  too — reconcile in S2).
- **W0.3d part-b (async-loader/worker completion-order) is diagnosis-only wrt Lane-A files.** If its
  fix wants `Rnd_Wgpu_RB3.cpp` or the object-list path, it STOPS and is coordinator-sequenced after
  Lane A — no concurrent edit of this file.
- **W2.6 is rb3-only + engine READ-ONLY** (foot/shoe rest-capture) — no collision with W2.3's engine
  edits; but both touch the crowd/extras SKIN_CLAMP story conceptually, so cross-diff the baseline
  clamp-count artifacts at coordinator merge.
- **A/A nondeterminism (per W2.1 B2).** W2.3 alters crowd skin-vertex worlds, so the pre-existing
  CharEyes/CharLookAt eye-flake + bloom/exposure wash will co-occur in A/B. Use A/A controls to
  attribute divergence; do NOT carry the W1.6-era "residual-name world failures = non-blocking" rule
  (W2.3 IS a world-alterer for crowd meshes).
- **Flag hygiene (F2).** `RB3_CROWD_OWN_BONES` is engine-side → census-enforced; register in
  `classification.json` at introduction (exit 0). `RB3_NO_CROWD_REBIND` stays an rb3-side flag (census
  does not scan `rb3/src/system/`) — untouched this wave.
