# Engine work-stream plan: drawrect + postproc-rtt + smear-removal St.3

**Authored:** 2026-06-01 (Opus staff-eng planner). **Companion to:**
[`RTT_HACK_UNWIND_ROADMAP.md`](RTT_HACK_UNWIND_ROADMAP.md). **Engine pin:** `070562a`.

Build-ready, sequenced plan for landing two WebGPU renderer features in the shared
`milo-native-engine` plus the dependent glue cleanup. ONE engine branch, three
sequential stages (all edit `src/platform/Rnd_Wgpu_RB3.cpp/.h` → strictly serial), plus
the rb3-glue deletion in Stage 3.

---

## 1. Confirmed facts (open questions resolved)

**1.1 TU exclusion for `MILO_ENGINE_GPU_BACKEND=rb3`** (engine `CMakeLists.txt:248-387`):
- Tier-2 rndobj-coupled gfx (dc3-only, EXCLUDED for rb3): `DrawRect2D.cpp`, `DofPass.cpp`,
  `VertexFormats.cpp`, `ShadowPass.cpp`, **`PostProcPass.cpp`**, `TextureConvert.cpp`.
- dc3 platform backends (EXCLUDED): `MaterialSetup/MeshGpuCache/Mesh_Wgpu/Part_Wgpu/
  RndTex_Native/`**`Rnd_Wgpu.cpp`**`/Tex_Wgpu/TransparentQueue`.
- rb3 backend (INCLUDED): only `src/platform/Rnd_Wgpu_RB3.cpp`.
- **`BloomPass.cpp` IS linked for rb3** (Tier-1 rndobj-free gfx, `MILO_ENGINE_GFX_SOURCES`).
- ⇒ BOTH features need **self-contained** implementations in `Rnd_Wgpu_RB3.cpp`.
  `DrawRect2D.cpp`/`PostProcPass.cpp`/`Rnd_Wgpu.cpp` are read-only references. Only
  `BloomPass` is reusable from the existing rb3 link.

**1.2 Pin check is a WARNING** (`rb3/native/CMakeLists.txt:76-86`), not FATAL — a worktree
at a different engine SHA still builds. Pin bumped once, at the end.

**1.3 `B+W_film02.pp` (dumped live via `/api/dta/eval`):** pure color-grade
(saturation **−40**, contrast **+10**, brightness 0, out_lo ≈0.047 black-lift) + bloom
(intensity 1.0, color warm-white) + vignette (1.02) + noise (0.5). **NO** refract /
chromatic / DOF / posterize / flicker. ⇒ a single-pass grade fragment (+optional bloom)
suffices. Port `PostProcPass.cpp:35-166` grade WGSL verbatim.

**1.4 OutfitConfig::Compose reachability (drawrect verify):** `Compose`
(`OutfitConfig.cpp:103`) ← `DrawPreClear()` (`:908`, calls `:969/:975`). Takes the
DrawRect branch only when the diffuse is an RTT outfit tex (`kRenderedNoZ`, `:109`).
Selects `sCam` with `SetTargetTex(diffTex)` before DrawRect (`:133/:143`) ⇒ at DrawRect
time `RndCam::sCurrent->TargetTex()==diffTex` and **no DrawMesh has run** → DrawRect must
fire the RTT begin-hook itself. Reachable via gameplay band-characters with RTT outfits;
fallback verify = `RB3_DRAWRECT_DBG` one-shot log proving the path fires with non-flat
tint + the existing `CHAR_DBG` diffuse-resolution log.

**1.5 Misc:** `Rnd::DrawRect` empty inline `Rnd.h:80`; `mColorModFlags` is **public**
(`Mat.h:353`, no header edit needed); `Blend` 0-7 ↔ `WgpuBlend` 0-7; `mTargetFmt` =
RGBA8 headless / `SurfaceFormat()` (BGRA8) windowed (`Rnd_Wgpu_RB3.cpp:652-653`) — the
intermediate + composite MUST use `mTargetFmt`, never hardcoded RGBA8; `mIntermediateTex`
does not exist yet (add to BandRnd, not GpuDevice). Stage-3 deletions:
`RB3SongSelectHideAlbumSmear` `rb3_game_input.cpp:1019-1046` + call `:1185`; `App.cpp:528-532`.

---

## 2. Isolation & landing mechanics

**Hazard:** `tools/setup-worktree.sh:192-203` symlinks `.claude/worktrees/milo-native-engine`
→ the REAL shared engine, so every rb3 worktree builds the shared checkout. Editing the
shared checkout perturbs concurrent `web-chars` worktrees + DC3.

**Mechanic: isolated engine git-worktree + `-DMILO_ENGINE_PATH`** (shared checkout
untouched until the final merge):
```bash
git -C /home/free/code/milohax/milo-native-engine worktree add \
    /home/free/code/milohax/milo-native-engine-wt-rb3rtt \
    -b rb3-drawrect-postproc 070562adf17732ed6fffd1b4f1e451f0ce8b0752
cd /home/free/code/milohax/rb3
cmake -S native -B native/build-rtt \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DMILO_ENGINE_PATH=/home/free/code/milohax/milo-native-engine-wt-rb3rtt
cmake --build native/build-rtt --target rb3-native -j   # per-stage (warm after first)
```
Do NOT use `setup-worktree.sh` for engine work (its shared symlink defeats isolation).
rb3-side edits stay in the main repo (Stage-3 deletions are small/surgical/`#ifdef HX_NATIVE`).

**Pin-bump:** once, at the end, after merging the engine branch to shared master.
**Final merge (after Stage 3 verified on web):** merge engine branch → engine master;
bump `MILO_ENGINE_PIN` to the merged SHA in `rb3/native/CMakeLists.txt:74`; rebuild
`native/build-native` + `scripts/web/build.sh`; remove the engine worktree; commit rb3
(pin bump + Stage-3 deletions) and merge to rb3 master.

---

## 3. Shared quad infrastructure (Stage 1 builds, Stage 2 reuses)

**Members (`Rnd_Wgpu_RB3.h` private):** `mQuadShader`, `mQuadRectBGL`, `mQuadPostBGL`,
`mQuadRectPL`, `mQuadPostPL`, `mQuadVertexBuffer`, `mRectUB`(32B), `mPostProcUB`(160B),
`mQuadReady`; `void EnsureQuadPipeline();`. Public override
`void DrawRect(const Hmx::Rect&, const Hmx::Color&, RndMat*, const Hmx::Color*, const Hmx::Color*) override;`.
Null all in `Shutdown()`.

**WGSL (one module, multiple entries):**
- `vs_rect` (explicit 6-vert NDC quad; pos mapped CPU-side) + `fs_rect` (tex*mod*vtxColor;
  `colorMod==2` → alpha-as-grayscale mask approx) + `fs_rect_notex` (mod*vtxColor; base
  layer null-diffuse).
- `vs_fullscreen` (vertex-index fullscreen triangle, no vbuf) + `fs_postproc` (PORT
  `PostProcPass.cpp:35-166` grade verbatim; PostProcUB 160B).

**`EnsureQuadPipeline()`:** compile shader once; build `mQuadRectBGL`(tex@0,samp@1,RectUB@2)
+ `mQuadPostBGL`(sceneTex@0,samp@1,PostProcUB@2 minBindingSize=160,bloomTex@3); pipeline
layouts; vbuf (6×32B) + UBs. RenderPipelines cached in a small map keyed on
`(format<<8|blend<<2|hasDepth<<1|isPost)` (avoids per-frame `CreateRenderPipeline` on the
composite).

**Reuse:** DrawRect → `vs_rect`/`fs_rect[_notex]`, format `mRtActiveTex?mRtFmt:mTargetFmt`,
blend `MapBlend(mat->GetBlend())`, depth per pass. Composite → `vs_fullscreen`/`fs_postproc`,
format `mTargetFmt`, no blend/depth, `Draw(3)`.

---

## 4. Stages (all edit `Rnd_Wgpu_RB3.cpp` → strictly sequential)

Build: `cmake --build native/build-rtt --target rb3-native -j`. **Universal canary every
stage:** a non-postproc, non-outfit gameplay/main_hub frame pixel-identical to pre-change;
`BandRnd: frame drawn — N meshes` unregressed (both new paths default-inactive).

**Stage 1 — drawrect:** implement `EnsureQuadPipeline()` + `BandRnd::DrawRect`:
guard → replicate RTT begin-hook (`if(!RB3RttDisabled()&&RndCam::sCurrent){RndTex* tt=
RndCam::sCurrent->TargetTex(); if(tt&&tt!=mRtActiveTex) BeginDrawTarget(tt);}`) →
`EnsureQuadPipeline()` → rect→NDC via `TheRnd->Width()/Height()` → `RectUB.mod =
mat->GetColor()*paramColor` (the key DC3 divergence — Compose sets tint via SetColor),
`colorMod=mat->mColorModFlags` → diffuse `GetRB3TexView` / `fs_rect_notex` → blend
`MapBlend(mat->GetBlend())` → format/depth per pass → `Draw(6)` → **restore
`mSceneBindGroup` at group 0**. Optional `RB3_DRAWRECT_DBG`. Verify: outfit composes
tinted (or DBG log + `RB3_RTT_OFF=1` no-crash). Commit engine branch (no pin bump).

**Stage 2 — postproc-rtt:** add `mIntermediateTex/View/W/H`, `mPostProcFlushed`,
`MainColorTarget()` (intermediate when `RndPostProc::Current()` else `mFrameView`),
`EnsureIntermediate(W,H)` (format **`mTargetFmt`**), `RunPostProcComposite(dst)` (the §3
composite reading the §1.3 grade via `RndPostProc::Current()` HX_NATIVE accessors).
- `BeginFrame:873`: `bool hasPP=RndPostProc::Current()!=nullptr; mPostProcFlushed=false;
  if(hasPP){EnsureIntermediate(W,H); colorAtt.view=mIntermediateView;} else
  colorAtt.view=mFrameView;`
- `EndDrawTarget:1151`: `colorAtt.view = MainColorTarget();` (mid-frame RTT resumes into
  the intermediate under a postproc screen).
- `EndFrame:913`: after `mPass.End()` before `mEncoder.Finish()`: `if(mIntermediateView &&
  RndPostProc::Current() && !mPostProcFlushed){RunPostProcComposite(mFrameView);
  mPostProcFlushed=true;}`.
- Bloom optional (bind `mBlackView`@3 to skip in v1; or reuse linked `BloomPass`). No
  CMake change. Verify native: song_select frame graded (desaturated) headless; composite
  log fires only on postproc screens; clouds RTT still renders. Commit engine branch.

**Stage 3 — smear-removal St.3 (rb3, after Stage 2 verified on web):** delete
`RB3SongSelectHideAlbumSmear` (`rb3_game_input.cpp:1019-1046`) + call (`:1185`) +
`App.cpp:528-532`; bump pin. Verify **web (authoritative):** `album-art-check.mjs` cover
present + no smear, etched graded. Native: etched graded not blanked; art-less song shows
placeholder. Commit rb3.

---

## 5. Workflow structure

Sequential pipeline; coordinator (me) owns the engine worktree setup (§2) + final merge
(§2). Stage-agents implement one-at-a-time on the engine branch (Stages 1-2 native-verify
via `build-rtt`), gate-on-success, abort-on-failure. Each returns PASS/FAIL + commit SHA +
verify artifacts + canary diff. Stage 2 starts only if Stage 1 PASS; Stage 3 only after
Stage 2 PASS + web grade verified. No parallelism (shared file).

---

## 6. Risks & mitigations (summary)

Format mismatch (use `mTargetFmt` not RGBA8 for intermediate/composite — hard Dawn abort,
surfaces immediately); depth/sampleCount (rb3 is 1×, depth-disabled D24S8 main / none RT /
none composite); **group-0 restore** after every DrawRect; mid-frame RTT resume via
`MainColorTarget()`; default-path canary; `kColorModAlphaUnpackModulate` mask = v1 approx
(alpha-as-grayscale); concurrent-engine coordination (isolated worktree until merge);
web BGRA8 + composite uses LoadOp::Clear (full-screen overwrite, no Load reliance); bloom
is v2-optional (don't block St.3 on bloom — block on "smear absent, region graded").

See the full planner output in the session transcript for exhaustive file:line detail.
</content>
