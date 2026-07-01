# Festival `*_screenmask` white-blank — Option A FIX (IMPLEMENTED)

Implements `festival-screenmask.md` / `DEFERRED-PLAN.md` item 1. Engine-only change
in the **RB3-only** GPU backend TU. DC3-safe by construction.

- **Engine commit:** `998b87340438dfa8c993ef8b46fffb0c725f5da1`
  (branch `wt-converge-screenmask`, based on engine `20dba55`).
- **File:** `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`,
  `BandRnd::DrawRect` (the `if (!hasTex) texView = mWhiteView;` fallback site).
- **NOT pushed; master pin NOT bumped** — coordinator consolidates.

---

## The bug (recap)

`coop_crowd_mass*_screenmask` shots render a flat near-white field (luma ~200,
white% ~83) hiding the mass crowd. The screenmask material's diffuse
(`crowd_mass.tex`) is a RENDER TARGET fed ONLY by a `TexMovie` Bink movie. Native
has no in-world Bink decoder, so the RT is never painted; `DrawRect` resolved no
view and blitted the 1x1 `mWhiteView` over the whole screen.

## The fix (Option A, ~10 lines, RB3-only)

At the fallback site, BEFORE `if (!hasTex) texView = mWhiteView;`, when the
diffuse is an **unpainted render target**, `return;` early (skip the quad)
instead of blitting white — revealing the band + venue/world behind it.

```cpp
    if (!hasTex && diffuse && diffuse->IsRenderTarget()) {
        static const bool kScreenmaskFallbackOff =
            getenv("RB3_SCREENMASK_FALLBACK_OFF") != nullptr;
        if (!kScreenmaskFallbackOff) {
            if (getenv("RB3_SCREENMASK_DBG"))
                fprintf(stderr, "[dbg] DrawRect skip unpainted-RT diffuse '%s'\n",
                        diffuse->Name() ? diffuse->Name() : "?");
            return;   // skip the quad — reveal what is behind the dead movie RT
        }
    }
    if (!hasTex) texView = mWhiteView;
```

- Gate `RB3_SCREENMASK_FIX` is **default-ON** (no env needed).
- Opt-out `RB3_SCREENMASK_FALLBACK_OFF=1` restores the original white blit.
- `RB3_SCREENMASK_DBG=1` traces which textures get skipped.

## The unpainted-RT predicate (the whole risk)

`diffuse->IsRenderTarget() && !hasTex`. `RndTex::IsRenderTarget()` is
`mType & kRendered` (`src/system/rndobj/Tex.h:125`). `hasTex` was already computed
by the existing view-resolution (`GetRB3TexView` → `UploadRndTexIfNeeded`).

Why this skips ONLY the dead movie RT and nothing legitimate:

- **Painted RT (sky-dome) — NOT skipped.** A real RT is created lazily by
  `BandRnd::BeginDrawTarget` (`Rnd_Wgpu_RB3.cpp:1906`) when something draws into
  it; that sets `sTexGpu[tex].uploaded = true` and a valid `view`. So
  `GetRB3TexView` returns the view → `hasTex == true` → the `!hasTex` guard fails
  → the quad draws normally. Verified by trace: the skip NEVER fires on any
  sky/backdrop texture in any venue.
- **Unpainted movie RT — skipped.** `crowd_mass.tex` (and the alpha=0 transition
  `movie.tex`) are `kRendered`-type RTs that never went through `BeginDrawTarget`
  (no native movie decoder paints them). No view → `hasTex == false` →
  `IsRenderTarget()` true → skipped.
- **Null / non-RT diffuse — unchanged.** Solid-color UI rects and the base-layer
  null-diffuse quads have a null or non-`kRendered` diffuse, so `IsRenderTarget()`
  is false (or diffuse is null) → they still fall through to `mWhiteView` exactly
  as before. The white fallback is preserved for everything that legitimately
  wants it.

**Trace confirmation** (`RB3_SCREENMASK_DBG=1`, festival_01): the skip fired on
exactly two textures across the run — `crowd_mass.tex` (423×, the target bug) and
`movie.tex` (562×, the separate intro/transition movie that was already invisible
via `mod alpha=0`, so skipping it is a no-visible-change equivalence). It fired on
**zero** sky-dome / backdrop / UI textures.

---

## A/B measurements (band-closeup-capture + festival_01 override; 8/8 deterministic pins)

PNGs under `shots/screenmask/{before,after}/`. Luma/white% via `/tmp/luma_analyze.py`
(white% = pixels with L>230).

### Festival — the PASS gate

| shot | BEFORE luma / white% | AFTER luma / white% | result |
|---|---|---|---|
| `coop_crowd_mass01_screenmask` | 201.9 / 82.5 | **31.8 / 0.4** | **white blank GONE → venue visible** |
| `coop_crowd_mass01_screenmask` (f1) | 203.1 / 83.2 | **27.8 / 0.5** | fixed |
| `coop_crowd_mass_screenmask` | 203.1 / 83.2 | **28.0 / 0.5** | fixed |
| `coop_crowd_mass_screenmask` (f1) | 203.2 / 83.2 | **27.6 / 0.4** | fixed |

Luma drops 202 → ~28 (into / below the venue's normal 60-80 range), white% 83 → 0.5.
The band + festival world geometry/lighting now show through the (now-skipped)
screenmask. PASS.

### MUST-NOT-BREAK — festival direct-crowd shots (no screenmask)

Same-binary same-anchor (8000ms) A/B isolates the fix from per-boot variance:

| shot | FIX ON (default) | FIX OFF (`RB3_SCREENMASK_FALLBACK_OFF=1`) |
|---|---|---|
| `coop_crowd_mass01_screenmask` | ~28 (skipped) | **200.8 / white% 80.4** (white blit restored — gate proof) |
| `coop_dir_crowd00` | 69.3 | 79.6 |
| `coop_dir_crowdb` | 78.3 | 105.7 |

The opt-out **exactly restores** the white blit (200.8, white% 80.4) — proves the
gate and that the white blank was this fallback. The `coop_dir_crowd00/crowdb`
backdrop renders the intended B/W comic poster in both cases (luma 60-105, white%
<0.8, NOT white-blank); the small luma spread is per-boot crowd/lighting flicker
(the `live_shot` differs per boot, e.g. `coop_bs_d_n02` vs `coop_fs_v_c03`), NOT my
code — the skip never fires on the static-textured backdrop (trace-confirmed).

### MUST-NOT-BREAK — sky-dome / venue backdrop wide shots

| venue / shot | BEFORE luma / white% | AFTER luma / white% | skip on sky? |
|---|---|---|---|
| small_club `coop_all_n00` | 27.1 / 0.4 | 36.7 / 0.7 | no (`movie.tex` only) |
| small_club `coop_dir-all`  | 35.1 / 0.6 | 28.7 / 0.7 | no |
| big_club `coop_all_n00`    | 28.1 / 0.5 | 30.5 / 0.6 | no |
| big_club `coop_dir_all00`  | 21.1 / 0.5 | 26.3 / 0.6 | no |
| arena_02 `coop_dir_all00`  | 47.1 / 0.6 | 60.1 / 2.8 | no |
| arena_02 `coop_dir_b00`    | 35.1 / 0.5 | 28.1 / 0.6 | no |

No new black, no missing sky, no white-blank anywhere. The skip fires only on the
invisible `movie.tex` in these venues — never on a sky-dome or backdrop texture.
(arena_01 was NOT tested — it CRASHES per the task; arena_02 used instead.)

---

## DC3-safety (RB3-only TU)

`Rnd_Wgpu_RB3.cpp` is compiled ONLY for the `rb3` GPU backend flavor
(`MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`, CMakeLists.txt:320-322). The `dc3` flavor
compiles a different file, `src/platform/Rnd_Wgpu.cpp`
(`MILO_ENGINE_GPU_PLATFORM_SOURCES`, CMakeLists.txt:303-312). DC3 never compiles
the edited TU. No shared shader / struct / uniform / header touched — `IsRenderTarget`
is an existing inline accessor on the shared `RndTex`. DC3-safe by construction; no
flag/ifdef needed.

## Build

Rebuild touched only `Rnd_Wgpu_RB3.cpp.o` + relink; clean build, no warnings on the
change. rb3-native links and runs headless.

## Scope note

This is Option A — converts the artifact from "jarring full-screen white" to
"acceptable band-through-world." It does NOT play the real animated crowd movie
(Option B = a native in-world `TexMovie`/Bink decoder; the movie bytes are inline
in the milo, so the intro `<video>`-overlay trick does not apply). **Do not close
GAP 4** — downgrade it to "root-caused, Option-A landed; Option B deferred."

## Landing (for the coordinator)

1. Engine commit `998b87340438dfa8c993ef8b46fffb0c725f5da1` → push to engine master.
2. Bump `MILO_ENGINE_PIN` in `rb3/native/CMakeLists.txt` to that SHA in the matching
   rb3 commit.
