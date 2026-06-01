# Offscreen render-to-texture (RTT) on native — current state + hack audit

**Authored:** 2026-06-01 (Opus, investigation thread continuing the song_select
album-art smear work). **Engine pin:** `070562a` (= `MILO_ENGINE_PIN` in
`native/CMakeLists.txt`; HEAD of `../milo-native-engine`). **Build:**
`native/build-native/rb3-native`, captured headless via
`scripts/native/song-select-capture.py`.

**TL;DR.** Offscreen-RTT is *partially* implemented on native and is **no longer a
blanket no-op** — the diagnosis in [`CHAR_OUTFIT_DIAGNOSIS.md`](CHAR_OUTFIT_DIAGNOSIS.md)
(2026-05-28) is now **stale on two of its three points**. The **mesh** RTT path
(`RndCam::SetTargetTex` → redirect draws into a per-`RndTex` GPU target) works, and
**char-bone skinning works**. What is *still* missing: `BandRnd::DrawRect` (the
rect-composite layers paint nothing) and any **PostProc / Overlay / TexRenderer**
RTT wiring. Empirically, with our song_select workaround **disabled**
(`RB3_NO_ETCHED_ART_FIX=1`) the screen now renders **clean — no center smear** — and
our native-only album-cover *hide* is now actively **blanking correctly-rendered art**.
That workaround is the prime "hack to unwind." See the audit table below.

---

## 1. What offscreen-RTT looks like on native TODAY (verify before trusting old docs)

The engine grew real RTT after the CHAR_OUTFIT diagnosis was written. Relevant
engine history (`../milo-native-engine`, `git log -- src/platform/Rnd_Wgpu_RB3.cpp`):

- `9f635b7` **engine: implement RndTex render-to-texture (MakeDrawTarget/FinishDrawTarget)**
- `e6c8f86` engine(rb3): InvalidateGpuMesh for SetGeomOwner hot-swap (W7-HUD)
- `8397fa6` text meshes — useAlphaAsRGB + zMode disable
- `15011bd` / `070562a` skinned-char vertex-colour fixes (magenta cast, white band)

All of these are **ancestors of the pin the previous session already used**
(`4ad3a5f`/`0c06ab1`), i.e. RTT + skinning were live during that session too.

### 1a. Mesh-RTT — IMPLEMENTED, works

`src/platform/Rnd_Wgpu_RB3.cpp`:

- `BandRnd::BeginDrawTarget(RndTex*)` (`:1086`) — lazily creates an RGBA8
  `RenderAttachment|TextureBinding` target once, keyed in the same `sTexGpu`
  side-table the diffuse-bind path reads (`:1098-1117`), suspends the main pass, and
  opens a fresh transparent-clear pass into the target view (`:1119-1137`).
- `BandRnd::EndDrawTarget()` (`:1140`) — closes the RT pass, re-opens the main pass
  with `LoadOp::Load` to preserve mid-frame contents (`:1147-1178`). (Note `:1158-1163`:
  must set `depthClearValue=1.0` even with `LoadOp::Load` or Dawn validation aborts —
  that was `5fda7f0`.)
- **Begin hook is lazy, driven from `DrawMesh`** (`:1188-1191`): when
  `RndCam::sCurrent->TargetTex()` is non-null and we haven't redirected yet, call
  `BeginDrawTarget`. The shared `rndobj/Cam.cpp` only fires the END side
  (`FinishDrawTarget`), so the begin had to be hooked here.
- `RndTex::FinishDrawTarget()` (`:1996`) → `EndDrawTarget()` if this tex is the
  active target. `RndTex::MakeDrawTarget()` (`:1995`) is intentionally `{}` (begin is
  hooked in `DrawMesh` instead).
- Disable with `RB3_RTT_OFF=1` (`:1080`). Used in production by the sky-dome
  `clouds_rnd.tex` path.

Constraints: **nested RT is unsupported** (`:1091` bails if `mRtActiveTex` already
set) and only `RGBA8Unorm`, no depth.

### 1b. Char-bone skinning — IMPLEMENTED, works

`DrawMesh` "V14 skinned-mesh detection" (`:1277+`): `owner->IsSkinned()` selects the
88-byte `GpuVertexSkinned` layout, fills the bone palette, and runs `vs_skinned`.
Confirmed empirically — see §2: `album.mesh` (the bone-driven album-cover surface)
renders **correctly positioned in the top-right panel** on native, not
screen-spanning. **This contradicts the "skinning is a native no-op" model** carried
in the workaround comment and in `project_native_visual_repro_loop` memory.

### 1c. What is STILL missing (the real "offscreen-RTT not on native" gaps)

1. **`BandRnd::DrawRect` does not exist.** `Rnd::DrawRect` is the empty inline `{}`
   at `src/system/rndobj/Rnd.h:80`; there is **no** `BandRnd` override (grep-confirmed).
   So every rect-composite layer paints **nothing**. This breaks `OutfitConfig`'s
   base + two-color diffuse/interp/mask tint passes
   (`src/system/bandobj/OutfitConfig.cpp:160/169/175/181`) — the *patch meshes* in the
   same compose now redirect fine via mesh-RTT (1a), but the tint *rects* under them
   are blank. This is the remaining half of the character-outfit RTT gap.

2. **No PostProc / Overlay / TexRenderer RTT wiring.** The engine RB3 backend honors
   **only** `RndCam::sCurrent->TargetTex()`. It has no handling for
   `RndPostProc` (`src/system/rndobj/PostProc.cpp`), `RndOverlay` (`Overlay.cpp`),
   `RndTexRenderer::DrawToTexture` (`TexRenderer.cpp:251`), `HiResScreen.cpp`, or
   `ScreenMask.cpp`. Any effect composited through those — rather than through a plain
   `cam->SetTargetTex` — never opens an RT pass.

3. **Nested RT + non-RGBA8 / depth targets** are unsupported (§1a).

---

## 2. Empirical re-test (2026-06-01, engine `070562a`)

`scripts/native/song-select-capture.py --depths 0,6,12`, headless, no real per-song
covers present (`orig-assets/extracted/ui/image/` has only `blank_album_art_keep` —
**no per-song art ships in `extracted`**, so the cover the prior session's *web* test
loaded cannot load in this native run).

| depth | fix OFF (`RB3_NO_ETCHED_ART_FIX=1`) | fix ON (default) |
|---|---|---|
| 0 (SETLISTS) | clean list; top-right teal "?" placeholder + contained etched cloud | clean list; top-right empty grey |
| 6 (song row) | clean list; **top-right shows ROCKBAND generic cover, correctly placed** | clean list; **top-right empty grey (art hidden)** |
| 12 (song row) | clean list; top-right grey (no cover loaded) | clean list; top-right grey |

**No center smear at any depth with the fix OFF.** The etched_art group renders
**contained in the top-right panel**, not leaking center-left. The album cover mesh
renders **correctly positioned**. The *only* visible effect of our fix today is to
**blank the top-right album panel on native** (depth 6: ROCKBAND cover → empty grey).

> Caveat — do not over-generalize. This native run cannot load real per-song covers
> (none in `extracted`), and headless `/api/screenshot` has the documented
> readback-retention quirk. The previous session confirmed a real smear on the **web**
> swapchain (where Marilyn Manson's cover *does* load). So: re-verify on a web build
> (and ideally a native run with real covers wired in) **before** ripping the
> workaround out. But the native evidence that skinning+mesh-RTT have caught up is
> strong, and the native-only hide is now demonstrably a regression.

---

## 3. `etched_art` — what it actually is

From `strings orig-assets/extracted/ui/song_select/gen/song_select.milo_xbox`: the
"etched glass" album reflection is a group of **meshes** (`header_song_etched_art.mesh`,
`…01.mesh`, `…_color.mesh`) with its own environment + two lights
(`etched_art.env` / `etched_art.lit` / `etched_art01.lit`), normal-mapped materials
(`header_song_bg_etched_art.mat`, normal/spec textures `etched_cool_cloud_*`,
`etched_tiger_head_*`), **plus a `PostProc` / `postprocess` object**.

> **CORRECTION (2026-06-01, after deeper scoping — see
> [`RTT_HACK_UNWIND_ROADMAP.md`](RTT_HACK_UNWIND_ROADMAP.md)).** The earlier framing
> below ("render the etched surface into an offscreen target") is **wrong**. The
> `etched_art.grp` meshes are plain `Group`s that draw **directly** to the framebuffer
> (no `TexRenderer`, no `cam->SetTargetTex` — confirmed zero TexRenderer tokens in the
> song_select milos). The "etched glass" look on Wii comes from a **full-frame
> `RndPostProc` color-grade** (`B+W_film02.pp`, Selected each Poll by
> `MetaPanel::UpdatePostProc`). The native gap is that **`BandRnd` never honors
> `RndPostProc::Current()` at all** — the whole-frame grade is simply never applied, so
> the etched meshes draw raw/ungraded. The fix is to render the main frame into an
> offscreen intermediate and run the engine's existing `PostProcPass` (exactly what
> DC3's `WgpuRnd` does), NOT to RTT the etched meshes. This is the **postproc-rtt**
> feature in the roadmap.

The (incorrect) original framing: ~~render the etched surface into an offscreen target
and composite it as a subtle reflection — a PostProc-driven RTT~~. Today the etched
meshes happen to land **contained in the top-right panel** (§2), so they no longer read
as a smear — but they are drawn **raw/ungraded** (no B+W film grade) rather than
composited.

---

## 4. Hack audit — fixes from this work-thread

Legend: **PERMANENT** = correct port behavior, keep. **GLUE** = legitimate
port-layer compensation, keep until/unless the engine subsumes it. **HACK** =
workaround to unwind once porting catches up (with the trigger).

| Fix (commit) | File | Class | Unwind trigger |
|---|---|---|---|
| `RB3SongSelectHideAlbumSmear` **— etched-group hide** (both targets) | `native/src/rb3_game_input.cpp:1086-1095` | **HACK** | PostProc-driven RTT wired (§1c-2). **Likely already inert** on native (§2 shows no etched leak). Re-verify web, then drop. |
| `RB3SongSelectHideAlbumSmear` **— native-only cover hide + `SetHookTex(false)`** | `rb3_game_input.cpp:1097-1127` (`#ifndef __EMSCRIPTEN__`) | **HACK (now a regression)** | Premised on "skinning is a native no-op" — **false now** (§1b/§2). It blanks correct top-right art. Unwind after a web re-verify + a native run with real covers. |
| `App::RunOneFrame` second per-frame `RB3SongSelectHideAlbumSmear` call | `src/App.cpp` (inside `#ifdef HX_NATIVE`) | **HACK** | Belt-and-suspenders for the hide above; remove together with it. |
| **Select-highlighted-song on confirm** (`tgt[64]=""`, was `"20thcenturyboy"`) | `rb3_game_input.cpp` (`__EMSCRIPTEN__` Confirm handler) | **PERMANENT** | Real bug fix — never re-pin a hardcoded song. Keep. |
| **Clear stale row labels** `label->SetTextToken(gNullStr)` | `src/band3/meta_band/MusicLibrary.cpp:~1048` (`#ifdef HX_NATIVE`) | **GLUE** | Mirrors base `UIListProvider::Text` clear. The 360-ARK layout draws unused slots the Wii layout hid. Correct for the loaded layout; keep unless a layout/PropAnim fix hides those slots. |
| **No-op `InvalidateGpuMesh`** | `native/src/rb3_render_mesh.cpp` | **PERMANENT** | rb3 `BandRnd` re-reads `GeomOwner()` per draw — no cache to invalidate. Correct semantics (the call is only meaningful in DC3's cached `WgpuRnd`). Keep. |
| **details-pane hide** (`song_select_details` showing iff `details_mode`) | `rb3_game_input.cpp:1247+` (opt-out `RB3_NO_DETAILS_FIX`) | **HACK** | PropAnim terminal `showing=FALSE` keyframes (`details_hide.trg`) apply natively. Same PropAnim-keyframe gap class as elsewhere. From [`SONGSELECT_FIX.md`](SONGSELECT_FIX.md). |
| **`ui/image/` data populate** (`cp extracted-xbox-full/ui/image`) | working data (`extracted` is gitignored) | **GLUE** | Fold `ui/image/` into the asset-extraction step so it survives a re-extract. From `SONGSELECT_FIX.md`. |

---

## 5. To actually finish offscreen-RTT on native (engine work, pinned)

In priority order, all in `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`
(engine is soft-pinned; coordinate before editing — concurrent `web-chars` worktrees
have been seen):

1. **Implement `BandRnd::DrawRect`** (override the empty `Rnd.h:80`): a textured/tinted
   full-target quad honoring `RndMat` color/blend/colormod
   (`kColorModModulate` / `kColorModAlphaUnpackModulate`). Unblocks the `OutfitConfig`
   two-color tint layers (and the cheap CPU-composite fallback in CHAR_OUTFIT §3 becomes
   unnecessary). Lowest-effort, highest-coverage gap.
2. **Drive PostProc/Overlay/TexRenderer to set up the RT cam.** Either honor those
   objects' offscreen-target setup, or ensure they route through
   `RndCam::SetTargetTex` so the existing begin-hook (§1a) catches them. Unblocks the
   `etched_art` reflection (§3) and any other PostProc compositing.
3. **Headless readback hygiene** (out of scope of RTT, but adjacent): clear the
   headless readback/accumulation target each frame in `src/gfx/GpuDevice.cpp`
   (`mHeadlessTex`/`mIntermediateTex`) so transient draws don't persist across
   `/api/screenshot`. This is the source of the capture quirk noted in §2.

Regression canary for any RTT change: the main scene must be pixel-unchanged when no
`TargetTex` is active (RT path only activates on non-null target). Diff a gameplay
highway frame; confirm `BandRnd: frame drawn — N meshes` unregressed.

---

## 6. Key files & docs (for the next agent)

- **This doc** — current RTT state + hack audit (supersedes the stale parts of
  CHAR_OUTFIT_DIAGNOSIS §2/§5 re: "RTT/skinning no-op").
- [`CHAR_OUTFIT_DIAGNOSIS.md`](CHAR_OUTFIT_DIAGNOSIS.md) — original 3-layer RTT root
  cause (2026-05-28). **Stale:** MakeDrawTarget/FinishDrawTarget are no longer no-ops;
  skinning works. **Still accurate:** `DrawRect` no-op (gap §1c-1).
- [`SONGSELECT_FIX.md`](SONGSELECT_FIX.md) — album-art grey occluder (data fix) +
  stray SAVE/details pane (PropAnim-keyframe hack). Notes the etched RTT as deferred.
- Engine RTT: `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`
  (`BeginDrawTarget:1086`, `EndDrawTarget:1140`, `DrawMesh` hook `:1188`,
  `FinishDrawTarget:1996`, skinning `:1277+`); header `Rnd_Wgpu_RB3.h:108-162`.
- Matched-fork RTT callers: `OutfitConfig.cpp:103-201` (compose),
  `TexRenderer.cpp:251` (`DrawToTexture`), `Cam.cpp:51-71`
  (`Select`/`SetTargetTex`→`FinishDrawTarget`), `PostProc.cpp`, `Overlay.cpp`,
  `Rnd.h:80` (empty `DrawRect`).
- Our workaround: `native/src/rb3_game_input.cpp:997-1128`
  (`RB3SongSelectHideAlbumSmear` + comment block); `src/App.cpp` (`#ifdef HX_NATIVE`
  per-frame call).
- Repro: `scripts/native/song-select-capture.py` (native headless),
  `scripts/web/album-art-check.mjs` (web swapchain — needed for the unwind re-verify).
</content>
