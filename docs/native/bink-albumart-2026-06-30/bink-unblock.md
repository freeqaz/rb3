# TASK A — Unblock the festival crowd-backdrop Bink: asset-ops + render plan

**Date:** 2026-06-30 · **Agent:** bink-asset-ops (Opus) · **Engine pin:** `998b8734` (Option-A screenmask skip) · **Build:** rb3-native built clean off current master (exit 0).

## VERDICT: still-BLOCKED — on ASSETS, not on plumbing

The Option-B render plumbing is **IMPLEMENT-ready** (bounded, DC3-proven, finalized below).
But the source movies **do not exist in any RB3 ARK we can access — Wii *or* Xbox** — so there
is nothing to decode/transcode. **The faithful asset is unobtainable from current data.**

The festival jumbotron biks (`fest1_mass*.bik`, `fest2_mass*.bik`) are referenced by the
Xbox/PS3 milos but were **never packed into the main ARK on either platform**. They are
loose, Xbox/PS3-disc-only streaming files that ship *outside* `main_xbox.ark`; we only have
the ARK (and the Wii wbfs), not a full 360/PS3 disc-filesystem image. **To unblock you must
obtain a full RB3 Xbox 360 (or PS3) disc image** and pull the loose `fest{1,2}_mass*.bik`.

---

## 1. Asset hunt — exhaustive, definitive

### What the milos reference (the `bink_movie_file` TexMovie sources)

Confirmed by `strings` on the inflated milos. Paths are **relative to the milo's `gen/` dir**.

| Venue milo | RT (`.tex`) | TexMovie objects (`.tmov`) | Bink sources (relative) |
|---|---|---|---|
| `festival_01.milo_xbox` | `crowd_mass.tex` | `crowd_mass.tmov`, `crowd_mass01..05.tmov`, `crowd_mass2/3.tmov`, `fest_mass06.tmov` | `texture/fest1_mass.bik`, `texture/fest1_mass01..06.bik` |
| `festival_02.milo_xbox` | `crowd_mass.tex` | `crowd_mass.tmov`, `crowd_mass01..05.tmov`, `fest2_mass06.tmov` | `textures/fest2_mass.bik`, `textures/fest2_mass01..06.bik` |

Driven by `coop_crowd_massNN_screenmask.shot` (festival_01: 01..10; festival_02: 01..11),
each with `_ps3` and `_new` variants. These resolve to e.g.
`world/venue/festival/festival_01/gen/texture/fest1_mass01.bik`.

### Where I looked, and what I found (all NEGATIVE for the jumbotron biks)

1. **Wii disc** `Rock Band 3 (USA).wbfs` — `wit FILES` shows the disc is ARK-packed
   (`files/gen/main_wii_{0..10}.ark` + `main_wii.hdr`); `wii-extracted/` is its un-ARK.
   The Wii ARK **header (file table) has 0 references** to `fest1_mass`/`fest2_mass`/
   `mass_crowd`/`crowd_mass*.bik`. (The 36 hits in `main_wii_6.ark` are the milo-internal
   `coop_mass_crowd0N.shot` object-name strings, not bik files.)
2. **Xbox 360 ARK** `orig-assets/xbox-zip/gen/main_xbox.{hdr,ark×9}` (5.5 GB, the source the
   native-rendered `.milo_xbox` came from). The `.hdr` is encrypted, so I did a **full fresh
   `arkhelper ark2dir` extraction** (7 863 files) and searched it: **zero** `fest*_mass`/
   `mass_crowd` biks. Only **6 biks total** in the whole Xbox ARK — `videos/rb3_intro_cinematic.bik`,
   `videos/rb3_end_credits.bik`, and 4 `ui/trainers/videos/guitartutorial*.bik`. The festival
   `gen/` dirs contain **only** the two `.milo_xbox` archives — **no `texture/`/`textures/`
   sibling** with biks. (Rescan dir then deleted; it duplicated `extracted-xbox-full`.)
3. **All existing extractions** (`extracted`, `extracted-xbox-full`, `wii-extracted`,
   `xbox-zip`) and a **filesystem-wide `find`** over `/home/free/code/milohax`, Downloads,
   /tmp: **0 hits** for `fest1_mass*`/`fest2_mass*`/`mass_crowd*`.
4. **Inline in the milo?** No. `festival_01.milo_xbox` (48 MB) has **no Bink container magic**
   (`BIKi/BIKb/BIKg/BIKh/KB2*`) anywhere — the prior "BIK string" hits were DXT/texture noise.
   Bink movies are external streams, never embedded in milos.
5. **No RB3 Xbox 360 disc image on the system** — only `.xex` files for *other* games
   (DC3, GH2, Fantasia). We have RB3's ARK but not its full disc filesystem.

### Platform determination — the screenmask jumbotron is Xbox/PS3-only

- The `fest{1,2}_mass*.bik` references and `coop_crowd_massNN_screenmask.shot` shots exist
  **only in the `.milo_xbox`** files. The **`.milo_wii`** festival milos use a different
  render path — `coop_crowd_massNN_render.shot` / `..._render.shot`, **no `_screenmask`** —
  and reference only `crowd_{intro,norm,good,peak,poor,danger}.bik` (the ambient crowd
  streams), not the jumbotron movies.
- The Wii "crowd" biks that *do* exist as loose files
  (`world/venue/{arena,big_club,small_club}/streams/crowd_*_intro.bik`) are **4×4-pixel
  dummy-video Bink files carrying only a `binkaudio_dct` track** (crowd *ambience audio*,
  ~21 s) — confirmed via `ffprobe` (`binkvideo, 4, 4`). **There is no crowd *video* footage
  anywhere in our assets, on any platform.** So even a non-faithful visual substitute is not
  available from what we have. Native renders the Xbox milos, so the Xbox bik would be
  preferred anyway — but it isn't shipped in the ARK.

**Conclusion:** the festival crowd jumbotron is an Xbox/PS3 feature whose source biks live as
loose disc files outside the ARK. **Unobtainable without a full 360/PS3 disc image.**

## 2. Transcode pipeline — PROVEN (just has no source to feed)

`ffmpeg` here decodes Bink fine: the 4 guitar tutorials probe as `binkvideo 1280×720` and the
intro cinematic transcodes cleanly. I produced a sample to prove the exact pipeline:
`ffmpeg .bik -> VP9 .webm` → `orig-assets/derived/bink-scratch/intro_5s_sample.webm`
(`vp9 1280×720, 5.005 s`, 470 KB; gitignored). `scripts/web/transcode_videos.py`'s VP9+Opus
recipe is the right tool — it's just hardcoded to the 2 cinematics today. So the *mechanical*
`.bik→.webm` step is ready; **there is simply no `fest*_mass.bik` to convert.**

Note for when assets arrive: the crowd jumbotron is an **in-world silent LOOP** (no audio —
the crowd *audio* is the separate `crowd_arena_*.mogg`/Wii `crowd_*.bik`), so transcode
**video-only** (`-an`) and rely on the engine `mLoop` to repeat. Expect small/short clips
(unlike the 720p fullscreen cinematics), so per-frame decode cost is negligible.

## 3. FINALIZED render plan (Option B1 — RB3-only, DC3-safe) — ready once assets exist

Adopt the scope doc's **Option B1** (do not touch the byte-matched Wii `Movie`/`MovieImpl`
shapes). The engine primitives all exist and are RB3-linked:
- Decoders: `milo-native-engine/src/platform/FFmpegMovieImpl.{cpp,h}` (native) +
  `WebMovieImpl.{cpp,h}` (web). FFmpeg API confirmed: `BeginFromFile(path,…)`, `Poll()`,
  `Draw()` (consumes frame), `HasDecodedFrame()`, `GetRGBABuffer()`, `GetDecodedWidth/Height()`,
  `SetVirtualTime(ms)` (headless capture), `mLoop`.
- RT upload: `UploadRGBAToRndTex(RndTex*, rgba, w, h)` — `Tex_Wgpu.cpp:258`, shared, RB3-safe.
- DC3 reference to mirror: `dc3-decomp/src/system/movie/TexMovie.cpp::DrawToTexture` (native
  branch uploads `HasDecodedFrame()` RGBA *before* `mMovie.Draw()`; web branch identical with
  `WebMovieImpl`; Wii branch keeps `MakeDrawTarget/Draw/FinishDrawTarget`).

RB3's `Movie` (`src/system/movie/Movie.h`) is the matched-Wii Bink wrapper — `Impl* mImpl`,
no `GetImpl()`/`MovieImpl*` — so we **bridge with a side-helper**, not by reshaping `Movie`.

### Deltas (file:line)

- **NEW `rb3/native/src/rb3_texmovie_native.cpp`** (~120–180 LOC). A `std::map<TexMovie*,
  FFmpegMovieImpl*>` (native) / `WebMovieImpl*` (web), keyed by the TexMovie. Exposes
  `RB3TexMovieBegin(TexMovie*, path, loop)`, `RB3TexMoviePoll(TexMovie*, virtualMs)`,
  `RB3TexMovieUploadIfReady(TexMovie*, RndTex*)` (→ `UploadRGBAToRndTex`), `RB3TexMovieEnd`.
  Self-contained so it never touches the shared `Movie` class.
- **`rb3/src/system/movie/TexMovie.cpp:86` `DrawToTexture()`** — add an `#ifdef HX_NATIVE`
  (+ `__EMSCRIPTEN__`) branch calling `RB3TexMovieUploadIfReady(this, mTex)` instead of the
  Wii `MakeDrawTarget/Draw/FinishDrawTarget` block (which is already `#ifndef HX_WEB` and a
  no-op on native because `Movie` is routed away).
- **`TexMovie.cpp:80 BeginMovie()` / `:116 Poll()`** — on `HX_NATIVE`, drive
  `RB3TexMovieBegin(this, unk_0x38, mLoop)` / `RB3TexMoviePoll(this, …)` so decode advances
  only while the shot is showing (≈1 active decode at a time across the ~9 `.tmov`).
- **`native/CMakeLists.txt:178`** — `FFmpegMovieImpl.cpp` is in
  `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE`. Its header `#include "movie/MovieImpl.h"` is DC3-only;
  either compile it into the new RB3 TU with an include shim, or lift the FFmpeg decode core
  into `rb3_texmovie_native.cpp`. Add `libavformat/avcodec/swscale/avutil` (system-present,
  `libavcodec 62.x`) to the rb3-native link line.
- **Web:** reuse `WebMovieImpl`'s in-world `<video>`+canvas-readback path (distinct from the
  intro `<video>` overlay). Extend `scripts/web/transcode_videos.py` to also emit `.webm` for
  the festival biks (glob `world/venue/festival/**/*.bik`, `-an`); `native/web/server.py`
  already serves `.webm` sidecars next to their `.bik` via the extracted-xbox-full fallback
  root + range requests — same mechanism as the intro.
- **Remove the Option-A skip** for the now-painted RT only after verifying paint:
  `Rnd_Wgpu_RB3.cpp:~3342` (`if (!hasTex && diffuse && diffuse->IsRenderTarget()) return;`).
  Once the RT is painted, `GetRB3TexView` resolves a real view → `hasTex==true` → the quad
  draws normally and the skip is naturally bypassed; keep the skip as the unpainted fallback.

### Loop / mapping / DC3-safety

- **Mapping:** all `crowd_massNN.tmov` in a venue paint the single shared RT `crowd_mass.tex`;
  the active `coop_crowd_massNN_screenmask.shot` selects which `.tmov` (→ which
  `fest{1,2}_mass*.bik`) is showing. So you need the **7 biks per venue** transcoded to
  `.webm` at the relative paths above.
- **Loop:** silent in-world loop — set `mLoop`, video-only transcode, no audio sync.
- **DC3-safety:** `rb3_texmovie_native.cpp` + the `TexMovie.cpp` `HX_NATIVE` branch are
  RB3-only; `UploadRGBAToRndTex` is additive and already used by DC3;
  `Rnd_Wgpu_RB3.cpp` is RB3-only (DC3 compiles `Rnd_Wgpu.cpp`). **Safe.**

## 4. What's needed to UNBLOCK + recommendation

1. **Obtain a full RB3 Xbox 360 disc image** (ISO / GOD / extracted disc filesystem — *not*
   just the ARK we have) **or a PS3 disc**, and extract the loose
   `world/venue/festival/festival_0{1,2}/gen/texture[s]/fest{1,2}_mass*.bik` (7 per venue).
   These ship outside `main_xbox.ark`, which is why the ARK extraction can't see them.
2. Transcode them video-only via the (extended) `transcode_videos.py`.
3. Implement Option B1 above (~1 day, bounded, DC3-safe).

**Until step 1 lands, keep Option A** (skip the unpainted `crowd_mass.tex` quad). There is no
faithful *or* plausible substitute in our current assets — the only real-video biks we have
are the intro/credits/guitar-tutorials (no crowd footage), and the Wii "crowd" biks are 4×4
audio-only dummies. Substituting any of those on the festival jumbotron would look wrong.

## Key paths
- Asset hunt (all NEGATIVE): Wii `.../orig-assets/wii/files/gen/main_wii.hdr` (0 refs);
  Xbox ARK `.../orig-assets/xbox-zip/gen/main_xbox.{hdr,ark}` (full extract → 0 jumbotron biks).
- Milos referencing the biks: `orig-assets/extracted/world/venue/festival/festival_0{1,2}/gen/festival_0{1,2}.milo_xbox`.
- Transcode proof (gitignored): `orig-assets/derived/bink-scratch/intro_5s_sample.webm`.
- Tools: `/usr/bin/wit`, `/home/free/code/milohax/tools/mackiloha/arkhelper`, `ffmpeg`,
  `rb3/scripts/web/transcode_videos.py`, `rb3/scripts/milo/extract_ark.sh`.
- Plan targets: `rb3/src/system/movie/TexMovie.cpp:86`, NEW `rb3/native/src/rb3_texmovie_native.cpp`,
  `native/CMakeLists.txt:178`, `milo-native-engine/src/platform/{FFmpegMovieImpl,WebMovieImpl,Tex_Wgpu}`,
  Option-A skip `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:~3342`.
</content>
</invoke>
