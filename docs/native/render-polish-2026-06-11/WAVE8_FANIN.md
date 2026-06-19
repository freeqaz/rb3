# Render-Polish Wave 8 — FAN-IN / LANDING PLAN (2026-06-19)

Wrap-up wave that closes the four residuals left open at campaign close: web full-song
load, venue lighting polish, pose-footwear false-drop shards, and crowd/venue variety.
All four items are **plan-tractable and verified**; three landed code, one landed Fix C
and deferred Fix B as a future-wave hand-off. **Wii decomp build is byte-identical for
all four** (every code change is engine-only `Rnd_Wgpu_RB3.cpp`, an `#ifdef HX_NATIVE`
`src/` block, or a non-compiled test artifact).

Base state the orchestrator lands onto:
- rb3 master HEAD `72e32812`; rb3 worktree bases `1c46a70e`.
- engine master pin `15ce606d` (`native/CMakeLists.txt:74`); both engine worktrees built on `15ce606`.

---

## 1. Verdict matrix

| Item | Plan | Impl status | Review verdict | Land? |
|---|---|---|---|---|
| **web-songload** | tractable, no engine | done, verified | CONFIRM | **YES** (rb3-only, 1 new test file) |
| **lighting-polish** | partial, needs engine | done, verified | CONFIRM_WITH_RESIDUALS | **YES** (engine + pin bump) |
| **pose-footwear-shard** | tractable, needs engine | done, verified | CONFIRM | **YES** (engine + pin bump) |
| **crowd-venues** | partial, needs engine | **partial** (Fix C done, Fix B plan-only) | CONFIRM_WITH_RESIDUALS | **YES, Fix C only** (rb3-only). Fix B = future-wave hand-off, NOT landed. |

No item is blocked; no verdict is REJECT. The two CONFIRM_WITH_RESIDUALS verdicts carry
only out-of-scope/taste residuals (see §3), not regressions.

---

## 2. Landing plan (for the orchestrator)

### 2a. rb3 commits to cherry-pick onto master

| Order | rb3 commit | Branch | Files | Notes |
|---|---|---|---|---|
| any | `5015eb0d` (full: `5015eb0d6fffdfe84327d672a6c5f36f3c672341`) | `wt-task-web-songload` | `scripts/web/_w8-songload-verify.mjs` (NEW, +222) | Test artifact only; not compiled. Zero conflict. |
| any | `1859def9` | `wt-task-crowd-venues` | `src/system/bandobj/BandDirector.cpp` | Fix C only. All changes inside the existing `#ifdef HX_NATIVE`. |
| after engine landed | `f35c58d9` | `wt-task-lighting-polish` | `native/CMakeLists.txt` (pin bump ONLY) | DO NOT cherry-pick as-is — see pin note 2c. |
| after engine landed | `3c89d897` | `wt-task-pose-footwear-shard` | `native/CMakeLists.txt` (pin bump ONLY, placeholder) | DO NOT cherry-pick as-is — see pin note 2c. |

`f35c58d9` and `3c89d897` are each a one-line `MILO_ENGINE_PIN` bump pointing at their
OWN worktree engine SHA. They will collide on the same line and neither final SHA is
correct individually. **Do not cherry-pick them.** Instead land the engine commits first
(2b) then write ONE pin bump by hand (2c).

### 2b. Engine commits to cherry-pick onto engine-main (in ORDER)

Both engine commits are single commits on top of pin `15ce606`. They touch the SAME file
(`src/platform/Rnd_Wgpu_RB3.cpp`) but **DISJOINT functions/regions** — sequential
cherry-pick is clean either order. Recommended order (lighting first, then shard):

| Order | engine commit | Branch | File / region |
|---|---|---|---|
| 1 | `03695e312db4210d7efcdb919ee3026817a77166` (lighting-polish) | `wt-task-lighting-polish` | `Rnd_Wgpu_RB3.cpp` → `BandRnd::WriteSceneUniforms` ONLY |
| 2 | `5962059a6ed5775bffe2e92242cb16d95b865fef` (pose-footwear-shard) | `wt-task-pose-footwear-shard` | `Rnd_Wgpu_RB3.cpp` → `BandRnd::DrawMesh` ONLY |

crowd-venues + web-songload have **NO engine commit**.

### 2c. ONE MILO_ENGINE_PIN bump covers BOTH engine commits

After landing both engine cherry-picks on engine-main, take the resulting engine-main HEAD
SHA and write `native/CMakeLists.txt:74` `MILO_ENGINE_PIN` = that final SHA, in a SINGLE rb3
commit. This supersedes BOTH `f35c58d9` and `3c89d897` (both of which pin only their own
intermediate worktree SHA and would be wrong/conflicting). One pin bump, one engine-main
state, covers all engine work in this wave.

### 2d. Exact conflict surfaces in the shared engine file

Both engine commits edit `Rnd_Wgpu_RB3.cpp`. Their regions do not overlap:

**`03695e3` (lighting) — function `BandRnd::WriteSceneUniforms`, the `world.cam` block (+33/−7):**
- New getters `sVenuePointExposure()` / `sVenueDirExposure()` + comment, appended AFTER the
  existing `sVenueGreyKey()` getter, just before `void BandRnd::WriteSceneUniforms` (~L1116).
- Dir-light color lines, `ty==1` branch (~L1290–1296): `* de` on the three `std::min(lc.<c>,1.5f)`.
- Point-light color lines, `ty==0` branch (~L1298–1304): `* pe` on the three `std::min(lc.<c>,1.8f)`.
- No-light grey-key fallback, `dl==0 && pl==0` (~L1317): `sVenueGreyKey() * sVenueDirExposure()`.

**`5962059` (pose-shard) — function `BandRnd::DrawMesh`, the V24 `degenerate` guard (~L4912–4946):**
- Was the single line `bool degenerate = (wext > 15.f) && (lext > 0.001f) && (wext > 2.0f * lext);`
  at ~L4915; now a ~50-line band-membership scan + 3-cap band branch + unchanged 2.0x `else`.
- Plus a 1-line `[SHARD_RATIO]` fprintf tweak (band/other label) immediately below. Render-inert.

These are in **different functions** (~L1116–1317 vs ~L4912–4946), ~3,600 lines apart. They
also do NOT touch any prior render-polish engine region: `DrawParticles`, `fs_postproc`
(wave-5 first-frame-flash `ppCeil/ppKnee`), `standard_wgsl.inc` (wave-4 `softClipLighting`),
the wave-5 WorldXfm recompose (~L4104–4156), the rebake (~L4015–4100; pose-shard only READS the
`skeleton_unshared.milo` detector at L4042, no edit), the per-bone finite guard (~L4203–4236),
bloom/halo composite, or the IK/C8 diagnostics. The only shared line with PRIOR wave work is
lighting's grey-key line (wave-5 menu-contrast `facaa6a` introduced it as `sVenueGreyKey();`;
lighting appends `* sVenueDirExposure()`) — already on engine-main, applies cleanly; if a future
sibling re-touches it the one-line factor is a trivial reroll.

### 2e. Plan-only / future-wave hand-off (NOT landed)

- **crowd-venues Fix B (2D bowl-imposter crowd)** — PLAN-ONLY by design (attempt-gated). Fix C
  makes the arena 2D-imposter path reachable + proven live (8 archetypes × ~87–88 instances/frame),
  and confirms the broken stub (`WiiRnd::GetSharedTex` returns null). NEW de-risking finding: the
  engine's skinned-RTT pipeline variant is ALREADY handled at pin `15ce606`
  (`Rnd_Wgpu_RB3.cpp:5384-5410`). Remaining work for a FUTURE wave (opt-in `RB3_CROWD_IMPOSTER=1`,
  default OFF): new native strong `WiiRnd::GetSharedTex` (`native/src/rb3_crowd_imposter_native.cpp`)
  + remove the weak stub at `native/src/band3_link_stubs.s:667-668` + an HX_NATIVE billboard branch
  in `src/system/rndobj/MultiMesh.cpp:162` + verify the 8-archetype shared-tex mid-frame RT
  close-cycle + capture a retail arena/festival reference (none exists in `images/retail-screenshots/`).
  Hand off to a future render-polish wave.

### 2f. Post-land deploy action (NOT a code landing, but required for the live web fix)

- The web-songload fix is a **rebuild**, not a source change: the SongParser underflow fix
  (`e83e2c79`, already on master since 2026-06-16, `#ifdef HX_NATIVE` at
  `src/system/beatmatch/SongParser.cpp:1095`) is in source but the DEPLOYED
  `native/web/build/{release,debug}/rb3-web.wasm` is the stale 2026-06-11 build (artifacts are
  gitignored, not committed). **After landing, run `scripts/web/build.sh` on the deploy host** so
  the live web deploy reflects the fix. Until then, in-browser tests against the as-deployed wasm
  reproduce the pre-fix crash (false negative). The verify harness `_w8-songload-verify.mjs` proved
  load+play on a freshly-built wasm.

---

## 3. Cross-item interaction risk on the COMPOSED master build

**Two engine commits compose in the same file. Check after the composed build:**

1. **Lighting × pose-shard (same file, different functions):** disjoint, no logic interaction.
   `WriteSceneUniforms` writes scene uniform colors; `DrawMesh` shard guard decides whether to
   draw a band garment. Independent. Verify the composed build compiles (one `Rnd_Wgpu_RB3.cpp`
   with both hunks) and `sizeof(SceneUniforms)` is unchanged (lighting added no struct field —
   only value scales on existing fields).

2. **Lighting changes COMPOSING with the EXISTING render-polish lighting stack — PRIMARY watch.**
   The wave-8 venue exposure scale (point ×0.70, dir ×0.80, on the per-light colors BEFORE the
   per-channel clamp) sits in a multi-layer lighting pipeline already on master:
   wave-4 `softClipLighting`, wave-5 `fs_postproc` `ppCeil/ppKnee` clip, wave-5 ambient floor/clamp,
   wave-5 menu-contrast grey-key. The wave-8 change is value-only and is designed as identity at
   exposure=1.0 (`RB3_VENUE_POINT_EXPOSURE=1 RB3_VENUE_DIR_EXPOSURE=1` = clean full revert, no
   rebuild). On the composed build confirm: (a) menu hub still readable, not over-crushed (impl
   measured 4.39:1 single-frame, dark-cell darker, no neon dimming); (b) the gameplay song-start
   reveal reads as a venue with form, not a pink flood, and is NOT dimmed vs revert at steady state;
   (c) the soft-clip still backstops (composite clipW% stayed 0.00 both ways in the A/B); (d) no
   regression on song_select (backdrop is near-black + UI prelit → exposure scale is a no-op there)
   or the game.cam highway (untouched — scale is inside the `world.cam` venue branch only).
   The two env knobs tune the reveal with no rebuild if art review wants it brighter/darker.

3. **pose-shard guard relaxation — confirm the negative control holds on the composed build.**
   The band branch (4.0x ratio cap / 110u world cap / 40u world floor) fires only when a bone
   resolves to `skeleton_unshared.milo`; crowd/extras/instrument/UI keep the proven 2.0x verbatim.
   Confirm on the composed build that genuine band flings still drop (impl negative control:
   192,639 drops) and crowd/UI/instrument drops are unchanged.

4. **crowd-venues Fix C — default path byte-unchanged.** With no override, the handler returns
   the `no_venue_override` sentinel → fallback → original small_club_01 path. objdiff
   `EnterVenue__12BandDirectorFv` = 100.0% / 520==520 / diff 0. No interaction with lighting/shard
   (different repo + different file). NOTE the known venue-specific native gaps surfaced by Fix C
   making non-small_club venues reachable: `festival_01` override SIGSEGVs during song-load
   (festival-venue native asset/render gap, NOT a Fix C logic bug — `arena_06` is reproducibly
   clean across 2 runs); `big_club` untested. Default (small_club) is unaffected. Track festival
   for the future Fix-B/venue-variety wave.

---

## 4. Close-out paragraph for CAMPAIGN_SUMMARY.md

> **Wave 8 (wrap-up, 2026-06-19) — the four campaign-close residuals closed.** Web full-song
> load+play in the BROWSER was confirmed (it was a stale deployed wasm predating the SongParser
> underflow fix, not a delivery defect — remedy is a `scripts/web/build.sh` rebuild on the deploy
> host; a new per-song Playwright harness `scripts/web/_w8-songload-verify.mjs` proves load+play
> on a fresh build). Venue lighting polish landed an env-tunable per-light exposure scale (point
> ×0.70 / dir ×0.80) in `WriteSceneUniforms`'s `world.cam` block that darkens the over-bright
> song-start reveal, the menu point-light mid-bleed, and the endgame disco peak at once, with the
> wave-4/5 soft-clip + postproc clip kept as backstops and identity at exposure=1.0. The
> pose-fling footwear/glove false-drop shard residual was fixed at the engine V24 ratio guard —
> band-class garments (bone owning `skeleton_unshared.milo`) get a 4.0x ratio cap + 110u world cap
> + 40u world floor while crowd/extras/instrument/UI keep the proven 2.0x, dropping band false-drops
> 12,597→0 with the genuine-fling negative control still catching 192,639 tears. Crowd/venue variety
> landed Fix C (the native venue bridge now honors `{meta_performer set_venue_override}`, so arenas
> load instead of always small_club_01, default byte-unchanged); Fix B (2D bowl-imposter crowd)
> stays plan-only and is handed to a future wave with the skinned-RTT pipeline de-risked. Wii build
> byte-identical across all four. Two engine commits (`03695e3` lighting, `5962059` pose-shard) land
> disjoint regions of `Rnd_Wgpu_RB3.cpp` under ONE `MILO_ENGINE_PIN` bump; two rb3-only commits
> (`5015eb0d` web test, `1859def9` Fix C) land independently.

---

## Appendix — quick orchestrator checklist

1. Cherry-pick engine `03695e3` then `5962059` onto engine-main (disjoint, clean).
2. Cherry-pick rb3 `5015eb0d` (web test) and `1859def9` (Fix C) onto master (independent, clean).
3. Hand-write ONE `MILO_ENGINE_PIN` bump → final engine-main HEAD SHA (supersedes `f35c58d9` + `3c89d897`; do NOT cherry-pick those two).
4. Build the composed master + run §3 checks (compose lighting on the existing stack; pose-shard negative control; Fix C default byte-unchanged).
5. Run `scripts/web/build.sh` on the deploy host so the live web reflects the SongParser fix.
6. Fix B (crowd imposters) + festival-venue native crash → future-wave hand-off.
