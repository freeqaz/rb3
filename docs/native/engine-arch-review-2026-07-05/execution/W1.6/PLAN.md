# W1.6 — Immutable scene bind group + `DrawContext` (kill the SYS-3 state-leak class)

**Lane A, item 2 (engine).** Planner: Opus. Status: PLAN written, implementation pending.
Parent: `REFACTOR_PLAN.md` W1.6 (:70) · `ARCHITECTURE_REVIEW.md` SYS-3 (:85) · lane doc
`01-renderer-core.md` §2b/§2f · `WAVE3_KICKOFF.md` COORDINATOR ACCEPTANCE + `WAVE3_REVIEW.md`
R-C/R-D. **The riskiest item of Wave 3.**

Gate precondition (re-confirmed): W0.3c reached **Exit B** — the `--canonical-order` multiset
comparator is landed (`drawlog-golden.py`, rb3 `5d254e00`) and catches every W1.6 defect class
deterministically. **CAVEAT carried from `W0.3c/STATUS.md` ("Implication for W1.6"):** the gate reads
*probabilistically RED* (~96% in the verifier's regime) because a **pre-existing CharEyes/CharLookAt
world-jitter residual** exceeds the frozen `eps=3.0` — a bug unrelated to W1.6, filed as W0.3d. This
PLAN therefore runs the draw-log gate as an **A/B differential with a residual-name filter** (S4),
and leans on the regime-*stable* nets (2-scene screenshot hash, `rb3-tests DrawLogGolden.*`, lineup,
`milo-engine-tests`) for the hard signal. See §"Gate protocol".

---

## Objective

Replace the mutable `mSceneBindGroup` member — a single field **re-created an unbounded number of
times per frame** by `WriteSceneUniforms`, where every draw implicitly consumes "whatever the last
write left" (order-dependent global state, SYS-3) — with:

1. **`WriteSceneUniforms` RETURNS an immutable per-write scene binding** (`RB3SceneBinding` =
   `{ wgpu::BindGroup group; uint32_t offset; }`) instead of side-effecting a mutable member that
   later reads implicitly depend on.
2. **One well-defined active-binding member `mActiveScene`** (value-typed `RB3SceneBinding`),
   assigned *only* at the explicit write/bind points — never mutated in place.
3. **A `RB3DrawContext` value object** (scene binding + world xfm + material/obj/bone bind-group
   handles + pipeline) built at draw time and threaded explicitly into a single `SubmitDraw` helper,
   so each draw's scene-binding dependency is a **visible parameter**, not a hidden member read.

**Output MUST be BYTE-IDENTICAL.** This is a rename+repackage: the sequence of `CreateBindGroup`
calls, the `SceneUniforms` bytes written into `mSceneRing`, the per-draw `SetBindGroup` handles, and
the submission order are all unchanged. Every commit is therefore **MOVE-class** (behavior-preserving,
hard rule 1) — no default-OFF flag is warranted because there is no behavior change to gate. The
staging discipline (small, individually-gated MOVE commits) exists so a *silent* regression (the
`a0f98ad` bind-group-collapse class R6 warns about) is caught at the smallest step.

## Faithful-reference citations (CURRENT code — re-grepped at engine HEAD `5cee522`)

`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (4775 lines) unless noted.

- **`WriteSceneUniforms(RndCam*)`** — defn `:1160`; fills `SceneUniforms s`, writes into
  `mSceneRing` (`mSceneOffset = mSceneRing.Write(...)` `:1433`), pins that offset into the bind-group
  descriptor (`e[0].offset = mSceneOffset` `:1436`), **creates the member**
  `mSceneBindGroup = ...CreateBindGroup(&bd)` `:1444`, then latches `mLastSceneCam`/`mLastSceneCamPose`
  `:1445-1450`. Declared `Rnd_Wgpu_RB3.h:179`.
- **Call sites of `WriteSceneUniforms`** (return value currently discarded): BeginFrame `:1552`;
  DrawMesh mid-frame re-pose `:2186` (V2/V13 cam re-pose) and `:2202` (per-environ).
- **Group-0 bind sites reading the member** (`mPass.SetBindGroup(0, mSceneBindGroup, 0, nullptr)`):
  BeginFrame open `:1590`; `BeginDrawTarget` `:1901`; `EndDrawTarget` `:1956`; `ClearDepthForOverlay`
  `:2003`; DrawMesh mid-frame re-binds `:2187`, `:2203`; DrawMesh **main draw** `:4149`; DrawParticles
  `:4757` + restore `:4767`.
- **Halo capture** `:4144` — `mHaloDraws.push_back({pipe, mSceneBindGroup, matBG, objBG, boneBG, ...})`
  stores the **live** scene handle (comment `:4128-4143` explains why the HANDLE not the offset). The
  replay in `RB3HaloPass.cpp:261-270` reads only `d.scene` (the captured field) — **RB3HaloPass.cpp
  needs NO edit** as long as the `HaloDraw.scene` field stays `wgpu::BindGroup` (`Rnd_Wgpu_RB3.h:416`).
- **Draw-log record** `:4167` — `RecordDrawLog(key, obj.world, mSceneBindGroup.Get(), matBG.Get(), ...)`
  logs the scene handle as an opaque identity token (the a0f98ad-collapse net).
- **`mSceneOffset` reads** (besides `:1436`): the CompositeHaloBloom debug fprintf `:2473`.
- **Members** (`Rnd_Wgpu_RB3.h`): `wgpu::BindGroup mSceneBindGroup` `:279`, `uint32_t mSceneOffset`
  `:280`, `RndCam* mLastSceneCam` `:281`, `float mLastSceneCamPose[6]` `:288`, `void* mLastSceneEnv`
  `:296`, `struct HaloDraw {...}` `:416`, `std::vector<HaloDraw> mHaloDraws` `:422`.
- **`mLastSceneCam*/mLastSceneEnv` dirty-tracking** (SYS-3 §2f-2, out of W1.6 scope — leave as-is):
  `:1445-1450`, `:2177-2205`. W1.6 does not touch the *heuristic*; it only removes the mutable-binding
  half of the leak.

### In-lane overlap to AVOID (W0.3c, already committed on this HEAD)
- W0.3c's `RB3_DRAWORDER_TRACE` probe lives at DrawMesh **entry** `:2076-2102`
  (`RB3DrawOrderTraceOn()`). It is upstream of every line W1.6 edits — **do not touch it** (hard
  rule 8: never revert/relocate a sibling/prior-lane commit's lines).
- W0.3b's frozen-clock seam is in `src/App.cpp` — **NOT edited** (no App.cpp edits, per brief).

---

## Design (end state)

```cpp
// Rnd_Wgpu_RB3.h — near the scene members (~:279)
struct RB3SceneBinding {          // immutable value returned per WriteSceneUniforms
    wgpu::BindGroup group;
    uint32_t        offset = 0;
};
struct RB3DrawContext {           // per-draw value threaded to SubmitDraw
    RB3SceneBinding      scene;   // the explicit scene dependency (was the leaked member)
    const float*         world;   // obj.world (column-major) — for capture/log
    wgpu::RenderPipeline pipe;
    wgpu::BindGroup      mat, obj, bone;
    wgpu::Buffer         vbuf, ibuf;
    uint32_t             indexCount;
};
// method: RB3SceneBinding WriteSceneUniforms(RndCam* cam);
// member: RB3SceneBinding mActiveScene;   // replaces mSceneBindGroup + mSceneOffset
```

`mActiveScene` is the **one** field that names "the scene binding currently bound at group 0". It is
assigned *only* `= WriteSceneUniforms(...)` (3 sites) and read by the group-0 bind points and the
per-draw context — it is never mutated field-by-field, so the "last-write-wins implicit dependency"
is gone by construction: each draw's binding is an explicit value it was handed.

Note: the bind points are *separate engine entry callbacks* (BeginFrame, Begin/EndDrawTarget,
ClearDepthForOverlay, DrawMesh, DrawParticles) into one long-lived pass — a "current active binding"
member is intrinsic to that shape and cannot be a pure local. The SYS-3 fix is that this member is
now an **immutable value assigned only at writes** and **consumed via an explicit `DrawContext`**,
not a mutable handle poked in place mid-`DrawMesh`.

---

## Subtasks

### W1.6.S1 — `WriteSceneUniforms` returns `RB3SceneBinding`; introduce `mActiveScene` mirror
- **model:** opus
- **files:** `milo-native-engine/src/platform/Rnd_Wgpu_RB3.h`, `.../Rnd_Wgpu_RB3.cpp`
- **goal:** Change the signature to return the freshly-created binding, add the value-typed
  `mActiveScene` member, and have callers capture it — while the legacy `mSceneBindGroup`/
  `mSceneOffset` members **stay written exactly as today** (transitional mirror). Bind sites still
  read the legacy members. Pure additive plumbing ⇒ byte-identical by construction.
- **steps:**
  1. `Rnd_Wgpu_RB3.h`: add `struct RB3SceneBinding` (group+offset) near `:279`; add member
     `RB3SceneBinding mActiveScene;`; change decl `:179` to
     `RB3SceneBinding WriteSceneUniforms(RndCam* cam);`. Do NOT remove `mSceneBindGroup`/`mSceneOffset`
     yet.
  2. `.cpp` `WriteSceneUniforms`: keep the body verbatim through `:1444`
     (`mSceneBindGroup = ...CreateBindGroup`); at the end `return RB3SceneBinding{ mSceneBindGroup, mSceneOffset };`.
  3. Update the 3 call sites to assign: `mActiveScene = WriteSceneUniforms(cam);` at `:1552`, and
     `mActiveScene = WriteSceneUniforms(RndCam::sCurrent);` at `:2186` and `:2202`.
  4. Leave all `SetBindGroup(0, mSceneBindGroup, ...)`, the halo capture, the drawlog, and the
     `:2473` offset read UNCHANGED.
- **verification:** build `rb3-native` + `rb3-tests` in `native/build-agent-W1.6`; run the full
  §"Gate protocol" (screenshot hashes at splash+song_select **byte-identical** vs the pre-S1 build is
  the primary proof; drawlog A/B differential shows no new class; `rb3-tests` DrawLogGolden.* green;
  `milo-engine-tests` 198/0/2; lineup PASS). Commit `W1.6:` (MOVE).

### W1.6.S2 — repoint all group-0 reads to `mActiveScene`; delete the legacy members
- **model:** opus
- **files:** `.../Rnd_Wgpu_RB3.cpp`, `.../Rnd_Wgpu_RB3.h`
- **goal:** Collapse the two mirrors into the single immutable-value member. Replace every read of
  `mSceneBindGroup` with `mActiveScene.group` and every read of `mSceneOffset` with
  `mActiveScene.offset`, then delete `mSceneBindGroup`/`mSceneOffset` from the header and their
  assignments inside `WriteSceneUniforms`. Byte-identical (same handle, same offset, accessed via the
  value).
- **steps:**
  1. In `WriteSceneUniforms`: build `RB3SceneBinding sb; sb.offset = mSceneRing.Write(...)` at `:1433`;
     use `sb.offset` in the descriptor at `:1436`; `sb.group = ...CreateBindGroup(&bd)` at `:1444`;
     set `mActiveScene = sb;` and `return sb;`. Remove the now-dead `mSceneBindGroup =`/`mSceneOffset =`
     stores.
  2. Repoint the 9 group-0 bind sites (`:1590, :1901, :1956, :2003, :2187, :2203, :4149, :4757, :4767`)
     to `mActiveScene.group`.
  3. Repoint halo capture `:4144` → `mActiveScene.group`; drawlog `:4167` → `mActiveScene.group.Get()`;
     `:2473` fprintf → `mActiveScene.offset`.
  4. Delete `mSceneBindGroup` / `mSceneOffset` decls (`Rnd_Wgpu_RB3.h:279-280`); also fix the
     `:970` reset (`mSceneBindGroup = nullptr;` → `mActiveScene = {};`).
  5. Confirm `grep -n mSceneBindGroup\|mSceneOffset` over both TUs = 0 hits (only comments may remain;
     update stale comments referencing the old member name).
- **verification:** full §"Gate protocol"; the DrawLogGolden.CatchesBindGroupCollapse deterministic
  net must stay green (it proves the scene handle per draw is unchanged). Commit `W1.6:` (MOVE).

### W1.6.S3 — `RB3DrawContext` + `SubmitDraw`; thread the scene binding explicitly + gate fail-red
- **model:** opus
- **files:** `.../Rnd_Wgpu_RB3.cpp`, `.../Rnd_Wgpu_RB3.h`
- **goal:** Deliver the DrawContext half of the brief. Introduce `struct RB3DrawContext` and a private
  `void SubmitDraw(const RB3DrawContext& ctx);` that issues `SetPipeline` + the 4 `SetBindGroup`s +
  VB/IB + `DrawIndexed`. Convert DrawMesh's final draw block (`:4144-4174`) to build a `ctx` (with
  `ctx.scene = mActiveScene`) and call `SubmitDraw(ctx)`; the halo capture and `RecordDrawLog` read
  from `ctx`. Do the same for DrawParticles (it binds group-0 scene + group-1 tex — thread
  `ctx.scene` into its submit path; its group-1 differs, so a lighter particle-context or a direct
  `SubmitDraw`-with-override is acceptable as long as `ctx.scene` is explicit). Byte-identical.
- **steps:**
  1. `Rnd_Wgpu_RB3.h`: add `struct RB3DrawContext`; add `void SubmitDraw(const RB3DrawContext&);`.
  2. `.cpp`: implement `SubmitDraw` = the exact `SetPipeline/SetBindGroup(0..3)/SetVertexBuffer/
     SetIndexBuffer/DrawIndexed` sequence at `:4147-4155`.
  3. DrawMesh: build `RB3DrawContext ctx{ mActiveScene, obj.world, pipe, matBG, objBG, boneBG, vbuf,
     ibuf, cachedIndexCount };` before the halo-capture branch; change capture push to use `ctx.*`
     fields; change `SubmitDraw(ctx);`; `RecordDrawLog(key, ctx.world, ctx.scene.group.Get(), ...)`.
  4. DrawParticles: build a ctx (scene from `mActiveScene`) and route its bind/draw through the
     explicit ctx.scene so no group-0 read touches a member directly.
  5. **Fail-red demonstration (the a0f98ad-class net, per brief R6):** on a scratch branch/uncommitted
     diff, deliberately mis-thread one draw's `ctx.scene` (e.g. capture `mActiveScene` into a local
     BEFORE a mid-frame `WriteSceneUniforms` and submit the stale binding for the scrolled-gem draws),
     rebuild, and show the drawlog gate goes **RED** on a **non-residual** world-xfm / bind-group-token
     divergence (not an eye-jitter residual). Record the exact rc + the diff line in STATUS. **Revert
     the perturbation** before committing S3 clean. This proves the gate would catch a real W1.6
     mis-thread despite the eps caveat.
- **verification:** full §"Gate protocol" GREEN on the clean build; fail-red RED recorded. Commit
  `W1.6:` (MOVE) — the fail-red perturbation is NOT committed.

### W1.6.S4 — consolidated verification sweep + comment cleanup + STATUS
- **model:** sonnet
- **files:** `.../Rnd_Wgpu_RB3.cpp` (comment-only), `docs/.../W1.6/STATUS.md`
- **goal:** Run the full determinism + screenshot + lineup + dual-suite sweep one last time against
  the S3 HEAD; refresh comments that still name `mSceneBindGroup`/"mutable member" to describe the
  immutable binding + DrawContext; append the STATUS sections with commit SHAs, the A/B differential
  numbers, the screenshot hashes, and the S3 fail-red evidence.
- **steps:**
  1. §"Gate protocol" end-to-end; capture the 15-run drawlog A/B classification table + the 2 scene
     screenshot md5s (baseline `41b9e3a`-behavior vs W1.6 HEAD).
  2. `grep -n "mutable\|mSceneBindGroup\|state.leak" Rnd_Wgpu_RB3.{cpp,h}` — reword stale comments
     (comment-only MOVE commit).
  3. Append `## W1.6.S1..S4 — done` to STATUS under `flock /tmp/rb3-docs.lock` with SHAs + evidence.
  4. (optional) `/refactor-staff` pass on the new `SubmitDraw`/context code — match% N/A (native),
     readability only, must not change emitted bytes (re-gate).
- **verification:** all gates green; STATUS complete. Commit `W1.6:` (comment/docs).

---

## Gate protocol (run on EVERY commit)

**Build (own dir):**
```
cmake -B native/build-agent-W1.6 -S native \
  -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
cmake --build native/build-agent-W1.6 --target rb3-native rb3-tests -j
```
(Pin-mismatch message is expected — W0.3c already advanced engine HEAD to `5cee522` past the
`41b9e3a` pin; **do NOT bump the pin**, hard rule 3.)

1. **2-scene screenshot hash (PRIMARY — regime-stable, byte-comparable).** Headless
   `RB3_HTTP=1 rb3-native`; capture `/api/screenshot` at **splash** and **song_select**; md5 must
   equal the pre-commit (baseline) build's md5 at the same two scenes. A byte-identical refactor of a
   static menu MUST match here — this is the hardest single signal and is immune to the eye-jitter
   caveat. (Use the `scripts/native/song-select-capture.py` harness pattern.)
2. **Draw-log gate — A/B differential with residual-name filter** (works around the W0.3c eps caveat):
   ```
   python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order \
       --determinism-check 15 --fail-red-audit --scene splash_screen
   ```
   Classify every FAIL. **GREEN for W1.6 iff:** draw **count** identical; **no** bind-group-collapse;
   **no** mesh-identity swap beyond baseline; **no** NEW *non-residual* world-xfm divergence — i.e.
   every residual FAIL's mesh-name-hash ∈ the committed 26-draw sidecar set
   (`native/tests/goldens/drawlog/splash_screen.fixedclock-residual.json`, 7 distinct hashes) AND the
   same FAIL-class distribution appears when the SAME sweep is run on the pre-commit build. A new FAIL
   class on the W1.6 build (absent from baseline) = RED = real regression. (Rationale + the "treat
   residual-name-only world failures as the known non-blocking eye jitter" instruction: `W0.3c/STATUS.md`
   §"Implication for W1.6".) **Never widen the residual sidecar** (W0.3c verifier's explicit warning).
3. **`rb3-tests DrawLogGolden.*`** (regime-independent unit nets): `ctest -R DrawLogGolden` in
   `native/build-agent-W1.6` — all green, especially `CatchesBindGroupCollapse`, `CatchesCoLocation`,
   `CatchesDroppedDraw`, `CatchesPipelineChange`, `PopulatesFromRealDrawMesh`.
4. **Lineup gate:** `scripts/native/lineup-gate.py` PASS against `goldens/w0.5-lineup/` (img/segA/
   ratioB/countC/pin layers — the patch-shard net). No shards, no ratio regression.
5. **Invariance nets — engine suite stays green:**
   ```
   DC3_DATA=/home/free/code/milohax/dc3-decomp/orig-assets \
   MILO_LIB=/home/free/code/milohax/dc3-decomp/orig-assets/extracted \
   ctest -j1   # in the milo-engine-tests build → 198 pass / 0 fail / 2 skip
   ```
   (SkinGolden.*/ClipPoseFixture.* are DC3-context invariance nets — a RED there means W1.6 leaked
   into shared draw/skin code = auto-stop.)

A **red gate is trusted over reasoning** (the a0f98ad multi-draw uniform-collapse regression is
exactly this item's failure mode).

---

## Exit criteria (measurable)

- **E1 — byte-identical output.** Splash + song_select `/api/screenshot` md5s on the final W1.6 HEAD
  equal the pre-W1.6 (`5cee522`-behavior) build's md5s. (Primary.)
- **E2 — no new draw-log defect class.** `drawlog-golden.py --fixed-clock --canonical-order
  --determinism-check 15 --fail-red-audit` on the W1.6 build produces the SAME FAIL-class distribution
  as the baseline build (all FAILs residual-name-only world jitter; zero count / bind-group-collapse /
  mesh-identity / non-residual-world divergences introduced by W1.6).
- **E3 — the gate can still see a W1.6 regression.** S3 fail-red: a deliberately mis-threaded
  `ctx.scene` drives the drawlog gate RED on a non-residual bind-group/world divergence (rc + evidence
  in STATUS); reverted before the clean commit.
- **E4 — structural fix landed.** `grep -c mSceneBindGroup Rnd_Wgpu_RB3.{cpp,h}` = 0 (member gone);
  `WriteSceneUniforms` returns `RB3SceneBinding`; `mActiveScene` is assigned only at the 3 write sites;
  each draw consumes its scene binding via `RB3DrawContext`/`SubmitDraw`. SYS-3 §2f-1 eliminated by
  construction.
- **E5 — nets green throughout.** `rb3-tests DrawLogGolden.*` green, `lineup-gate.py` PASS,
  `milo-engine-tests` 198/0/2, on every commit.

## Files touched (exact repo-relative — coordinator cross-diffs lanes)

Engine repo (`/home/free/code/milohax/milo-native-engine`), all MOVE-class:
- `src/platform/Rnd_Wgpu_RB3.cpp`
- `src/platform/Rnd_Wgpu_RB3.h`

rb3 repo (`/home/free/code/milohax/rb3`), docs only:
- `docs/native/engine-arch-review-2026-07-05/execution/W1.6/PLAN.md`
- `docs/native/engine-arch-review-2026-07-05/execution/W1.6/STATUS.md`

**NOT touched:** `src/platform/RB3HaloPass.cpp` (reads the captured `HaloDraw.scene` field only — no
member dependency); `src/App.cpp` (brief: no App.cpp edits); any `scripts/native/*.py` gate (Exit-B
comparator already landed by W0.3c — W1.6 is a consumer); `MILO_ENGINE_PIN` (coordinator bumps).
Stage ONLY these paths (`git add <path>` — never `-A`/`-a`); the engine tree carries a sibling's
uncommitted `FxSendNative.cpp` — leave it untouched (rule 8).

## Risks / conflicts

- **Lane A sequential (W0.3c → W1.6) on `Rnd_Wgpu_RB3.cpp`.** W0.3c is DONE (engine HEAD `5cee522`);
  W1.6 builds on top. The only W0.3c lines inside the file are the `RB3_DRAWORDER_TRACE` probe at
  `:2076-2102` (DrawMesh entry) — **upstream of every W1.6 edit; do not touch** (rule 8).
- **W2.2 (Lane B) is engine READ-ONLY** (rb3-only edits + existing `mNativeBonesRebound`/
  `RB3_GUARD_EXEMPT_REBOUND` seams per WAVE3_REVIEW D1) — **zero engine-file overlap** with W1.6.
- **W2.5 (Lane C) is rb3 game-side** (`BandConfiguration.cpp`) — no overlap.
- **W3.1 was DEFERRED to Wave 4** precisely because its edit surface (fog + 4→8 light arrays →
  `SceneUniforms` struct size, inside `WriteSceneUniforms`) is the exact region W1.6 rewrites
  (WAVE3_REVIEW E1/R-D hotspot 1). Confirm W3.1 is NOT running concurrently; if it appears, it must
  rebase onto W1.6's `RB3SceneBinding` return shape.
- **The draw-log gate is probabilistically RED** (eps under-calibration, W0.3d). Mitigated by the A/B
  differential + residual-name filter (§"Gate protocol" step 2) and by leaning on the regime-stable
  screenshot/unit/lineup nets. Do **not** "fix" it by widening the residual sidecar (forbidden) or by
  touching CharEyes/CharLookAt (engine, out of scope, would collide with this region).
- **Byte-identity of a hand-threaded refactor is fragile.** Mitigation = the MOVE-per-step staging
  (S1 signature+mirror → S2 collapse+delete → S3 context) so any divergence is bisected to one small
  commit, plus the mandatory per-commit gate.
