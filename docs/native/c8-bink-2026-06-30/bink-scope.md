# Frontier 2 — Festival in-world TexMovie / Bink crowd backdrop: implementation scope

**Date:** 2026-06-30 · **Author:** Bink-scoping agent (research only, zero code changes)
**Engine pin at investigation:** `998b8734` (the Option-A screenmask skip)

## TL;DR

The render-side wiring is **bounded and already exists as a working reference in DC3**
(`dc3-decomp/src/system/movie/TexMovie.cpp::DrawToTexture` — native FFmpeg + web `<video>`
decode → `UploadRGBAToRndTex(mTex, …)` directly into the RB3-compatible RT). The
decoders (`FFmpegMovieImpl`, `WebMovieImpl`) and the RT-upload primitive
(`UploadRGBAToRndTex`, in shared `Tex_Wgpu.cpp`) are all present and RB3-safe.

**BUT there are two real blockers that make this LARGER than "wire the decoder":**

1. **The festival crowd movie assets are MISSING and Bink-only.** The `.tmov` objects
   reference external `texture/fest1_mass01.bik … fest1_mass06.bik` (festival_01) /
   `../textures/mass_crowd1.bik …` (festival_02). **None of these `.bik` files exist
   anywhere in `orig-assets/`** (confirmed: `find … -iname 'fest*mass*.bik' -o -iname
   'mass_crowd*.bik'` → 0 hits), and they are **not inline** in the milo (the 5 "BIK"
   string hits in the milo are compressed-texture noise, not `BIKi/BIKb/BIKg` headers).
   So there is nothing to decode today — neither native FFmpeg (Bink decoder exists in
   libavcodec) nor the web transcode-to-`.webm` step has a source. The prior
   `screenmask-impl.md` claim that "the movie bytes are inline in the milo" is
   **inaccurate** — they're external references to absent files.

2. **RB3's `Movie` class is the matched-Wii Bink shape, NOT the DC3 `MovieImpl*` shape.**
   DC3's `TexMovie::DrawToTexture` calls `mMovie.GetImpl()` and `dynamic_cast<FFmpegMovieImpl*>`.
   RB3's `Movie` (`src/system/movie/Movie.h`) has **no `GetImpl()`, no `MovieImpl*` member,
   no `BeginFromFile`** — it has a `Movie::Impl*` that is the Bink/`BINK*` wrapper, and on
   `HX_NATIVE` every public method is routed to `rb3_movie_native.cpp` which only handles
   the **two fullscreen cinematics** and explicitly no-ops in-world TexMovie.

**Verdict:** the GPU/decode plumbing is IMPLEMENT-sized (~1 day if assets existed), but
**asset acquisition (blocker 1) is the gating, unbounded item** and the RB3 `Movie`-shape
adaptation (blocker 2) adds a real engine seam. Net: **larger than it looks** — recommend
keeping GAP 4 deferred unless the crowd `.bik` assets can be located/extracted.

---

## 1. How an in-world TexMovie is *supposed* to paint its RT each frame

`TexMovie` (`src/system/movie/TexMovie.{h,cpp}`) is `RndDrawable + RndPollable`. Its members:
`ObjOwnerPtr<RndTex> mTex` (the render-target texture, e.g. `crowd_mass.tex`), `Movie mMovie`,
and `FilePath unk_0x38` (the `bink_movie_file` path, set via `SetFile`/PropSync).

Per-frame contract (matched fork, Wii path):

- **`TexMovie::Poll()`** → if showing, `mMovie.SetPaused(false)` then `mMovie.Poll()`
  (decodes/advances one Bink frame; `End()`s on EOF).
- **`TexMovie::DrawToTexture()`** (TexMovie.cpp:86) — the RT-paint path:
  ```cpp
  if (!unk_0x38.empty() && mTex && mMovie.Ready() && mMovie.IsOpen()) {
      mTex->MakeDrawTarget();   // bind crowd_mass.tex as the GPU render target
      mMovie.Draw();            // Wii: Bink blits the current frame into the bound RT
      mTex->FinishDrawTarget();
      TheRnd->MakeDrawTarget();
  }
  ```
  Driven from `DrawPreClear()` (TexMovie.cpp:111) via `TheRnd->PreClearDrawAddOrRemove`
  (registered in `UpdatePreClearState`) — i.e. it paints the RT in the pre-clear phase,
  before the venue draws and samples `crowd_mass.tex` as a material diffuse.

**The whole block is `#ifndef HX_WEB`** (and on native the `Movie` methods are routed away),
so on native/web `DrawToTexture` is effectively a no-op — the RT is never painted. That is
the exact gap the Option-A skip works around in `Rnd_Wgpu_RB3.cpp:3346` (skip the
`crowd_mass.tex` quad rather than blit white).

### The intro-movie wiring (the path that DOES work, for contrast)

`rb3_movie_native.cpp` is **fullscreen-only**: `IsFullscreenCinematic()` gates on
basename `rb3_intro_cinematic` / `rb3_end_credits`. On web it spawns a `<video>` **overlay**
above `#canvas-container` (z-index max) — no GPU texture upload at all; the browser
composites the video over the WebGPU canvas. Native desktop instant-skips (or virtual-plays
under `RB3_INTRO_SECS`). `Movie::Begin/Poll/End/SetPaused/Ready` (Movie.cpp, all `#ifdef
HX_NATIVE`) delegate to `RB3MovieNative*`; the Bink `mImpl` path is dead on native.
**This overlay trick cannot paint an in-world RT** (a `<video>` over the canvas ≠ a texture
sampled by venue geometry), so the festival crowd RT needs a genuine decode→upload path.

### The DC3 reference — exactly what RB3 needs (already shipped in DC3)

`dc3-decomp/src/system/movie/TexMovie.cpp::DrawToTexture` (lines 201–232) is the painted-RT
path for native + web:
```cpp
#if defined(HX_NATIVE) && !defined(__EMSCRIPTEN__)
    FFmpegMovieImpl* impl = dynamic_cast<FFmpegMovieImpl*>(mMovie.GetImpl());
    if (impl && impl->HasDecodedFrame())
        UploadRGBAToRndTex(mTex, impl->GetRGBABuffer(),
                           impl->GetDecodedWidth(), impl->GetDecodedHeight());
    mMovie.Draw();          // marks frame consumed
#elif defined(__EMSCRIPTEN__)
    WebMovieImpl* impl = dynamic_cast<WebMovieImpl*>(mMovie.GetImpl());
    if (impl && impl->HasDecodedFrame())
        UploadRGBAToRndTex(mTex, impl->GetRGBABuffer(), …);
    mMovie.Draw();
#else  // Wii
    mTex->MakeDrawTarget(); mMovie.Draw(); mTex->FinishDrawTarget(); TheRnd.MakeDrawTarget();
#endif
```
DC3's `Movie` (`dc3-decomp/src/system/movie/Movie.h`) is the *thin* `MovieImpl*` wrapper:
`MovieImpl *GetImpl() const { return mImpl; }`, `mImpl = TheMovieSys.CreateMovieImpl()`,
and `BinkMovieSys::CreateMovieImpl()` returns `new FFmpegMovieImpl()` (native) /
`new WebMovieImpl()` (web). That factory + `GetImpl()` + the typed accessors
(`HasDecodedFrame/GetRGBABuffer/GetDecodedWidth/Height`) are **what RB3's `Movie` lacks**.

---

## 2. The festival crowd movie asset

- **TexMovie objects:** `crowd_mass01.tmov … crowd_massNN.tmov` (47 movie-related strings per
  festival milo), drawn by `coop_crowd_massNN_screenmask.shot` shots; the RT they paint is
  `crowd_mass.tex` (and per-variant `crowd_mass1.mat/.mask`, …).
- **Movie source files referenced (the `bink_movie_file` paths):**
  - festival_01: `texture/fest1_mass01.bik` … `fest1_mass06.bik`, `texture/fest1_mass.bik`
  - festival_02: `../textures/mass_crowd1.bik` … `mass_crowd.bik`
- **Format:** Bink (`.bik`). libavcodec **can** decode Bink (the intro `.bik` already decodes
  via `FFmpegMovieImpl` → `.webm` for web), so format is not the blocker.
- **Where referenced:** inline in `festival_0{1,2}.milo_xbox` (the `.tmov` PropSync
  `bink_movie_file`), under
  `orig-assets/extracted/world/venue/festival/festival_0{1,2}/gen/`.
- **🚫 ASSET BLOCKER:** these `.bik` files are **absent from every `orig-assets/` root**
  (`extracted`, `extracted-xbox-full`, `wii-extracted`) — only the milo archives that
  *reference* them are present, and they're not inline Bink data either. The festival venue's
  `gen/` dir contains only the two `.milo_xbox` archives; there is no `texture/` or
  `streams/` sibling with the movie files.
- **Can `transcode_videos.py` handle it?** Mechanically yes (it's a generic
  `ffmpeg .bik → VP9/Opus .webm`), but it is **hardcoded** to `CINEMATICS =
  ["rb3_intro_cinematic.bik", "rb3_end_credits.bik"]` under `…/videos/`. Adding the festival
  crowd biks is a trivial list/glob extension — **but only once the source `.bik` files
  exist**. As of now there is nothing to feed it.
- For reference, *other* venues' crowd streams DO exist as loose biks
  (`world/venue/arena/streams/crowd_arena_intro.bik`, big_club, small_club) — so the
  festival crowd biks plausibly exist in a fuller extraction; they were just not pulled into
  this repo. **Locating them is prerequisite step 0.**

---

## 3. Scope of the fix (delta), per platform, with file:line

Assuming the `.bik` assets are obtained, the render-side delta mirrors DC3 but must bridge
RB3's matched-Wii `Movie` shape. Two sub-options:

### Option B1 — RB3-only, surgical (recommended if assets exist)
Do **not** touch the shared `Movie`/`MovieImpl` shapes. Add a small RB3-native in-world
movie helper (sibling to `rb3_movie_native.cpp`) that owns an `FFmpegMovieImpl`/`WebMovieImpl`
**per TexMovie**, keyed off the `bink_movie_file` path, and paints the RT directly. Wire it
from `TexMovie::DrawToTexture` / `Poll` / `BeginMovie` under `HX_NATIVE`.

- **`rb3/src/system/movie/TexMovie.cpp:86` `DrawToTexture()`** — add an `#ifdef HX_NATIVE`
  branch that calls the helper: if it has a decoded frame, `UploadRGBAToRndTex(mTex, rgba,
  w, h)` (the engine primitive at `Tex_Wgpu.cpp:258` — already RB3-linked and RT-aware).
- **`rb3/src/system/movie/TexMovie.cpp:72` `BeginMovie()` / `:116` `Poll()`** — drive the
  helper's `BeginFromFile(path,…)` / `Poll()` (decode-on-timer; both impls already have a
  `SetVirtualTime` hook for headless capture).
- **NEW `rb3/native/src/rb3_texmovie_native.cpp`** — a thin map<TexMovie*, decoder> that
  instantiates `FFmpegMovieImpl` (native) / `WebMovieImpl` (web), forwards `Begin/Poll/End`,
  and exposes the latest RGBA frame. ~120–180 LOC; mirrors DC3's `DrawToTexture` logic but
  self-contained so it never touches the shared `Movie` class.
- **CMake:** un-exclude `FFmpegMovieImpl.cpp` for rb3-native (currently in
  `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE`, `native/CMakeLists.txt:178`) — **but** its header
  `#include "movie/MovieImpl.h"` is DC3-only. Fix by compiling `FFmpegMovieImpl.cpp` directly
  into `rb3-texmovie_native` with an include shim, OR (cleaner) lift the FFmpeg decode core
  into the new RB3 TU so it doesn't need the DC3 `MovieImpl` base at all. FFmpeg is
  system-present (`libavcodec 62.28`), so linkage is just adding `libavformat/avcodec/
  swscale/avutil` to the rb3-native link line.
- **DC3-safety:** `UploadRGBAToRndTex` is in shared `Tex_Wgpu.cpp` but is additive (DC3
  already uses it); the new RB3 TU + `TexMovie.cpp` `HX_NATIVE` branch are RB3-only. **Safe.**

### Option B2 — converge RB3's `Movie` onto the DC3 `MovieImpl*` shape
Re-shape RB3's matched `Movie` to expose `GetImpl()` + a `MovieImpl*` and a `CreateMovieImpl`
factory, then copy DC3's `DrawToTexture` verbatim. **Rejected:** RB3's `Movie` is a
**byte-matched Wii class** with an extensive Bink `Movie::Impl` (Movie.cpp, ~845 lines, many
100%-matched functions). Bolting a second impl pointer or refactoring it risks the match and
is far more invasive than B1 for the same visual result. Keep the matched class; use B1's
side-helper.

### Web specifics
- `WebMovieImpl` already decodes via a hidden `<video>` + canvas `getImageData` readback
  (`web_movie_read_frame`) → RGBA → `UploadRGBAToRndTex`. This is the **in-world** path
  (distinct from the intro overlay) and is exactly what DC3 uses. Reuse as-is.
- Transcode: extend `scripts/web/transcode_videos.py` to also emit `.webm` for the festival
  crowd biks (glob `world/venue/festival/**/*.bik`), and ensure `server.py` serves them as
  range-request sidecars next to the (would-be) `.bik` — same mechanism as the intro.

---

## 4. Effort, risk, and recommendation

| Item | Effort | Risk |
|---|---|---|
| **0. Obtain the festival `.bik` assets** | **UNKNOWN / unbounded** | **HIGH — gating.** Not in repo; may require re-extracting a full Xbox/PS3 disc or finding them in a fuller dump. Without these, nothing downstream is testable. |
| 1. RB3 in-world decoder helper (B1) + `TexMovie` `HX_NATIVE` branch | ~0.5–1 day | Med — RB3 `Movie`-shape adaptation; `FFmpegMovieImpl.h` DC3-include shim |
| 2. CMake: link FFmpeg + un-exclude/lift the decoder TU for rb3-native | ~1–2 hr | Low — FFmpeg system-present; isolate from DC3 `MovieImpl` base |
| 3. Web: reuse `WebMovieImpl` in-world path + transcode festival biks | ~2–3 hr | Low — proven path; just more assets |
| 4. RT format / per-frame upload | ~1 hr | Low — `UploadRGBAToRndTex` already RGBA→`WriteTexture`; crowd RT is small; ~47 `.tmov` but only the active screenmask shot samples per shot |

**Main technical risks (assuming assets exist):**
- **Per-frame decode cost:** 47 TexMovie objects per festival, but only the
  *currently-shown* screenmask shot's RT is sampled — drive decode by `mShowing`/`Poll`, so
  realistically 1 active decode at a time. Bink crowd loops are small/short; cost is low.
- **RT format:** the crowd RT is `crowd_mass.tex` (a `kRendered` RT). `UploadRGBAToRndTex`
  uploads RGBA via `Queue().WriteTexture`; must confirm the RT's GPU format matches (the
  Option-A path proves the RT exists with no view; `EnsureRenderTargetData` allocates it).
- **Audio sync:** N/A — the crowd movie is a silent visual loop (no soundtrack to sync).

**IMPLEMENT vs larger-than-it-looks:** the **render plumbing is IMPLEMENT** (B1 is bounded
and DC3-proven). The **overall task is larger than it looks** because step 0 (asset
acquisition) is an unbounded, out-of-repo prerequisite and the matched-Wii `Movie` shape
adds a real seam vs DC3's clean `GetImpl()`. **Recommendation: keep GAP 4 deferred** behind
Option A until the festival crowd `.bik` assets are located; once they are, B1 is a ~1-day
RB3-only, DC3-safe implementation.

---

## Key file references

- RB3 in-world movie (the gap): `rb3/src/system/movie/TexMovie.cpp:86` `DrawToTexture`
  (`#ifndef HX_WEB`), `:72` `BeginMovie`, `:116` `Poll`; `TexMovie.h:39` `mTex` / `:44`
  `mMovie`.
- RB3 matched Bink `Movie` (no `GetImpl`): `rb3/src/system/movie/Movie.{h,cpp}`; native
  routing all in `Movie.cpp` `#ifdef HX_NATIVE` → `rb3/native/src/rb3_movie_native.cpp`
  (fullscreen-only, `IsFullscreenCinematic`).
- DC3 reference (painted-RT path): `dc3-decomp/src/system/movie/TexMovie.cpp:201-232`;
  `Movie.h:36` `GetImpl`; `moviebink/BinkMovieSys.cpp:83-87` factory.
- Decoders: `milo-native-engine/src/platform/FFmpegMovieImpl.{cpp,h}` (native, has
  `GetRGBABuffer/GetDecodedWidth/Height/HasDecodedFrame` + `SetVirtualTime`),
  `WebMovieImpl.{cpp,h}` (web, in-world readback path).
- RT upload primitive: `milo-native-engine/src/platform/Tex_Wgpu.cpp:258`
  `UploadRGBAToRndTex(RndTex*, rgba, w, h)` — shared, RB3-linked.
- Option-A skip (current behaviour): `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:3346`
  (engine pin `998b8734`).
- Assets: `orig-assets/extracted/world/venue/festival/festival_0{1,2}/gen/*.milo_xbox`
  reference `texture/fest1_mass0N.bik` / `../textures/mass_crowdN.bik` — **MISSING from repo**.
- Transcode: `rb3/scripts/web/transcode_videos.py` (hardcoded to 2 cinematics).
- CMake exclusion to undo: `rb3/native/CMakeLists.txt:178` (`FFmpegMovieImpl.cpp` in
  `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE`).
