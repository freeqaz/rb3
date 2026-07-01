# festival `*_screenmask` near-white blank frame — VERDICT: **BUG** (missing movie-fed crowd backdrop → white-texture fallback)

Backlog item 1 (GAP 4 / Group-C C2). Research-only; no code changes.

Pin: engine `20dba55`. Repro/measurement done on current master `rb3-native`.

---

## TL;DR

The festival `coop_crowd_mass*_screenmask` shots render a **flat near-white field**
(mean luma ~200, white% ~83) hiding the mass crowd. This is a **BUG**, not an intended
white flash. Root cause is a missing-texture **white fallback**, not a blend/alpha bug:

```
shot coop_crowd_mass01_screenmask
  -> ScreenMask "crowd_mass.mask"   (RndScreenMask, fullscreen quad, mColor = white default)
     -> mMat = "crowd_mass.mat"
        -> diffuse tex = "crowd_mass.tex"   (a RENDER TARGET, not a static bitmap)
           <- painted by TexMovie "crowd_mass*.tmov"  (an animated Bink movie texture)
```

On `HX_NATIVE` there is **no Bink decoder**, and the native movie shim
(`rb3_movie_native.cpp`) only handles **fullscreen cinematics**
(`rb3_intro_cinematic`/`rb3_end_credits`) — explicitly *not* `TexMovie` in-world
videos. So the `crowd_mass*.tmov` movie never opens, `crowd_mass.tex` is never
painted, and `BandRnd::DrawRect` for the screenmask material hits its
`if (!hasTex) texView = mWhiteView;` fallback (a 1×1 solid-white texture) modulated by
the ScreenMask's default-white `mColor` → the whole screen is white.

The festival's **direct** crowd shots (`coop_dir_crowd00`/`coop_dir_crowdb`) render the
intended stylized B/W comic-poster crowd backdrop fine, because they draw authored
**static geometry/textures**, not the movie-fed screenmask. So the venue's crowd
backdrop CAN render — only the movie-texture screenmask path is broken.

**It is a real bug worth fixing, but NOT a small overlay blend/alpha tweak.** A correct
fix has to make the screenmask reveal *something other than white*: either (a) play the
crowd movie into the RT (needs a native movie decoder — large), or (b) when the movie
texture is unavailable, fall the screenmask through to a sensible substitute instead of
white. Recommended near-term fix is a small, RB3-only, DC3-safe **screenmask
fallback** (option B below): when `crowd_mass.mat`'s render-target texture is empty,
skip/blacken the screenmask quad rather than blitting white — the band + the festival's
own world geometry then show through, which is far closer to retail than a white screen.

---

## 1. Reproduce (confirmed)

Tool: `/tmp/bch_override.py` (= `scripts/native/band-closeup-capture.py` + venue
override). Pinned 8/8 frames deterministically.

```
SHARD_DBG=1 SHARD_RATIO_DBG=1 MILO_HEADLESS=1 python3 /tmp/bch_override.py \
  --override festival_01 --song-downs 0 \
  --shots "coop_crowd_mass01_screenmask,coop_crowd_mass_screenmask,coop_dir_crowd00,coop_dir_crowdb" \
  --frames 2 --frame-dt 600 --out /tmp/bch_fest_sm --tag festsm
```

Per-frame mean luma / white% (white% = pixels with L>230):

| shot | mean luma | white% | reads as |
|---|---|---|---|
| `coop_crowd_mass01_screenmask` (×2 frames) | 197 / 204 | 78 / 83 | **flat white field** behind HUD + highway |
| `coop_crowd_mass_screenmask` (×2)          | 204 / 204 | 83 / 83 | flat white |
| `coop_dir_crowd00` (×2)                     | 65 / 61   | 0.4     | **intended** B/W comic poster backdrop |
| `coop_dir_crowdb` (×2)                      | 73 / 74   | 0.4     | intended B/W comic poster backdrop |

Persistent across **all 3 frames** in a separate 3-frame run (200.9 / 206.3 / 205.5) →
**not a 1-frame flash**.

Evidence PNGs saved:
- `shots/festival_screenmask_WHITE.png` — the white-field bug
- `shots/festival_dir_crowd00_INTENDED.png`, `shots/festival_dir_crowdb_INTENDED.png` —
  the venue's real authored crowd backdrop (B/W comic poster: sunburst rays, crowd
  silhouettes, amp stacks, monster graphic) — proves white is NOT the intended look.

---

## 2. Asset ground-truth (the white is not authored)

festival_01 milo (`orig-assets/extracted/world/venue/festival/festival_01/gen/festival_01.milo_xbox`,
uncompressed Version-A; strings in object order):

```
2118 crowd_mass.tex        <- RndTex render target (the screen the movie paints)
2143 crowd_mass.mat        <- RndMat, samples crowd_mass.tex
2168 crowd_mass1.mat
2194 crowd_mass2.mat
2220 crowd_mass3.mat
2312 ScreenMask  2326 crowd_mass.mask    <- RndScreenMask, mMat = crowd_mass.mat
2345 TexMovie    2357 crowd_mass.tmov    <- animated Bink movie -> paints crowd_mass.tex
2376 TexMovie    2388 crowd_mass01.tmov
2409 ScreenMask  2423 crowd_mass1.mask
2477 TexMovie    2489 crowd_mass2.tmov
... (crowd_mass01..05.tmov + fest_mass06.tmov; 6 movie variants)
18567 BandCamShot !coop_crowd_mass01_screenmask.shot   (+ _ps3 variants, 10+ shots)
```

This is unambiguously a **designed animated crowd backdrop**: the shot is literally named
`crowd_mass…_screenmask`, and the screenmask material is fed by a `TexMovie` movie
pipeline. A white screen-flash would be authored as a constant-white quad, not a
fullscreen ScreenMask bound to a movie-fed material. The mechanism is **festival-specific**:
festival_01 + festival_02 both carry the full `crowd_mass*.tmov` movie pipeline; clubs
have no ScreenMask; arena_02 has a `ScreenMask` but with a **static** `screenmask.mat`
(no TexMovie). So the festival "outdoor mass-crowd" aesthetic is intended to be an
animated comic/poster crowd, replacing the real 3D crowd in those framings.

Note: there is **no external `.bik` file** in the extracted xbox tree for `crowd_mass`
— the movie data is embedded **inline in the milo stream** (TexMovie::Load passes the
BinStream to `BeginMovie` for `gRev > 4`). So this can't be transcoded to a `.webm`
sidecar the way the intro cinematic was; the bytes live inside the milo.

### Retail ground-truth (web)

No retail festival gameplay screenshot exists in `images/retail-screenshots/` (club only),
and direct web searches did not surface usable festival-venue footage (the Fandom
"Background Videos" wiki page is paywalled — HTTP 402; YouTube video search not
machine-fetchable here). **However, ground-truth is not actually needed to call this a
bug**: the in-game asset proves intent (a movie-fed crowd backdrop), and the venue's own
direct-crowd shots prove the crowd backdrop is meant to render content, not white. The
white is a native missing-texture artifact. (If desired, a human can confirm the exact
animated look against retail festival footage, but the bug/intended verdict does not
depend on it.)

Sources consulted (none had the specific festival-screenmask visual; listed for the record):
- Rock Band Wiki — Background Videos (paywalled 402): https://rockband.fandom.com/wiki/Background_Videos
- Rock Band Wiki — Category:Venues: https://rockband.fandom.com/wiki/Category:Venues
- TCRF — Rock Band 3: https://tcrf.net/Rock_Band_3

---

## 3. Root cause (engine + src, verified by code read + runtime)

### Data flow on native

1. `RndScreenMask::DrawShowing()` (`src/system/rndobj/ScreenMask.cpp:57`) draws a
   fullscreen quad: `TheRnd->DrawRect(drawRect, mColor, mMat, NULL, NULL)`.
   `mColor` defaults to **white** `(1,1,1,1)` (ctor line 14). `mMat` = `crowd_mass.mat`.

2. `BandRnd::DrawRect` (`../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:3248`)
   resolves the diffuse:
   ```cpp
   RndTex* diffuse = mat ? mat->GetDiffuseTex() : nullptr;   // crowd_mass.tex
   if (diffuse) { texView = GetRB3TexView(diffuse);
                  if (!texView) texView = UploadRndTexIfNeeded(mGpu, diffuse);
                  if (texView) hasTex = true; }
   if (!hasTex) texView = mWhiteView;                        // <-- WHITE FALLBACK (line 3326)
   ```
   `mWhiteView` is a 1×1 solid `(255,255,255,255)` (`CreateDefaultTextures`, line 837/840).
   Modulated by white `mColor` → flat white.

3. `crowd_mass.tex` is a **render target**, painted only via
   `TexMovie::DrawToTexture()` (`src/system/movie/TexMovie.cpp:86`):
   ```cpp
   if (!unk_0x38.empty() && mTex && mMovie.Ready() && mMovie.IsOpen()) {
       mTex->MakeDrawTarget(); mMovie.Draw(); mTex->FinishDrawTarget(); ...
   }
   ```
   The guard requires **`mMovie.IsOpen()`**.

4. On `HX_NATIVE`, `Movie::Begin` routes to `RB3MovieNativeBegin`
   (`src/system/movie/Movie.cpp:259`), and `Movie::IsOpen` →
   `RB3MovieNativeIsOpen()` (`Movie.cpp:241`). The native shim
   (`native/src/rb3_movie_native.cpp`) only opens **fullscreen cinematics**
   (`IsFullscreenCinematic` = `rb3_intro_cinematic`/`rb3_end_credits`, line 146-150);
   the file's own header comment states *"TexMovie in-world videos still no-op."*
   So for `crowd_mass*.tmov`, `RB3MovieNativeBegin` returns 0, `gActive=false`,
   `IsOpen()` = false.

5. Therefore `DrawToTexture` never blits → `crowd_mass.tex` is never even registered as
   a render-target entry in `sTexGpu` (RT entries are created lazily in
   `BeginDrawTarget`, `Rnd_Wgpu_RB3.cpp:1906`, which only runs when something draws INTO
   the target). `GetRB3TexView(crowd_mass.tex)` returns empty;
   `UploadRndTexIfNeeded` returns empty (an RT tex has no CPU bitmap pixels,
   `Rnd_Wgpu_RB3.cpp:635-647`). `hasTex=false` → white. (`RndTex::MakeDrawTarget` is also
   an empty no-op on the RB3 backend, line 6046, so even the TexMovie's own draw-target
   path wouldn't paint the RT.)

### Runtime confirmation

`RB3_DRAWRECT_DBG=1` over the screenmask shot logs `movie.tex` rects (the separate
intro/transition movie, `mod alpha=0`, invisible) and the eyes-RTT draws, and **zero**
`crowd_mass` / TexMovie-open lines — consistent with the crowd movie never opening and
the screenmask falling to the white path (the per-kind DBG cap suppresses the repeated
fullscreen screenmask draw, so it doesn't appear by name, but the absence of any
TexMovie/crowd_mass activity is itself the confirmation).

---

## 4. VERDICT: BUG

The white field is a **missing-texture artifact** (white-fallback for an unpainted
render target), not an intended flash. The screenmask quad is NOT drawing opaque white
because of a mask/alpha/blend mis-application (the original GAP-4 hypothesis) — the blend
math is fine; the **texture is simply absent** because its feeding `TexMovie` movie never
decodes on native.

Severity: MEDIUM. Visible whenever the auto-director picks a `*_screenmask` shot in the
festival venue (10+ such shots authored). Hides the mass crowd the framing is built for.

---

## 5. Fix plan

Two layers; pick by appetite. **Engine change is RB3-only and DC3-safe** (lives in
`Rnd_Wgpu_RB3.cpp`, an RB3-only TU; DC3 never compiles it). No shared shader/struct touched.

### Option A (preferred near-term, small, RB3-only) — screenmask empty-RT fallback

When the screenmask material's diffuse is an **unpainted render target** (no GPU view),
do NOT blit `mWhiteView`. Instead make the screenmask a **no-op (skip the draw)** so the
band + the festival's own world geometry/lighting show through — far closer to retail
than a full-screen white.

- **Where:** `BandRnd::DrawRect` (`../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:3316-3326`).
  Add: if `diffuse` is non-null but resolves to no view AND `diffuse->IsRenderTarget()`
  (i.e. an RT that was never painted), `return;` early (skip the quad) instead of falling
  to `mWhiteView`. Gate behind `RB3_SCREENMASK_FIX` default-ON with opt-out
  `RB3_SCREENMASK_FALLBACK_OFF`.
- **DC3-safety:** RB3-only TU. The early-return only triggers for the specific
  empty-RT-diffuse case (the sky-dome `clouds_rnd.tex` RT path *does* get painted by
  `BeginDrawTarget`, so it has a view and is unaffected; verify with `RB3_RENDER_DBG`).
- **Risk:** a screenmask whose RT legitimately *should* be painted (e.g. a future native
  movie decoder) would then correctly show content; skipping only the unpainted case is
  safe. Must-not-break gate: the `coop_dir_crowd00`/`crowdb` direct shots (no screenmask)
  stay identical; the `*_screenmask` shots go from white → band-through-world (verify
  luma drops from ~200 into the venue's normal 60-80 range, crowd/world visible).
- **Cost:** ~10 lines, one TU, pin bump. **This is the recommended deferred-batch fix.**

### Option B (faithful, large) — native TexMovie decoder

Give `TexMovie` a real native movie path so `crowd_mass*.tmov` actually plays the comic
crowd animation into `crowd_mass.tex`. This is a substantial workstream:
- The movie bytes are **inline in the milo** (no `.bik`/`.webm` sidecar), so the web
  `<video>`-overlay trick used for the intro does NOT apply — you'd need an in-engine
  Bink (or transcoded) decoder writing into a WebGPU render target, plus wiring
  `RB3MovieNativeBegin`/`IsOpen`/`Draw` for non-cinematic TexMovies, plus
  `RndTex::MakeDrawTarget`/`FinishDrawTarget` actually painting the RT on the RB3 backend
  (currently `MakeDrawTarget` is a no-op, `Rnd_Wgpu_RB3.cpp:6046`).
- High effort; only pays off the festival venue's screenmask shots. **Defer** unless a
  general in-world-movie capability is wanted (it would also light up any other
  TexMovie-backed surfaces).

### Recommendation

Ship **Option A** in the deferred convergence batch (small, safe, removes the most
jarring artifact — a full-screen white blanking the festival crowd shots). Treat
**Option B** (true animated crowd backdrop) as a separate, lower-priority "native
in-world movie" feature. Do **not** close GAP 4 — it is a genuine bug; downgrade it from
"needs ground-truth" to "root-caused, Option-A fix ready."

---

## 6. Files / symbols (all absolute)

- `src/system/rndobj/ScreenMask.cpp:57` — `RndScreenMask::DrawShowing` (white `mColor` default, `DrawRect(mColor, mMat)`)
- `src/system/movie/TexMovie.cpp:86` — `TexMovie::DrawToTexture` (gated on `mMovie.IsOpen()`)
- `src/system/movie/Movie.cpp:241,259` — `Movie::IsOpen`/`Movie::Begin` HX_NATIVE routing
- `native/src/rb3_movie_native.cpp:146` — `IsFullscreenCinematic` (excludes TexMovie videos)
- `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:3326` — DrawRect white fallback (**fix site, Option A**)
- `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:635,840,1906,6046` — RT-tex upload skip / mWhiteView / BeginDrawTarget RT creation / MakeDrawTarget no-op
- asset: `orig-assets/extracted/world/venue/festival/festival_01/gen/festival_01.milo_xbox` (ScreenMask `crowd_mass.mask` + TexMovie `crowd_mass*.tmov` + RT tex `crowd_mass.tex`)
- evidence: `docs/native/converge-2026-06-20/deferred/shots/festival_{screenmask_WHITE,dir_crowd00_INTENDED,dir_crowdb_INTENDED}.png`
