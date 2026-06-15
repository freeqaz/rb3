# task-first-frame-flash — gameplay-entry venue pink/white flash (WAVE 5)

**Status:** DONE + verified. Engine-only fix (web + native). Wii byte-identical.

## TL;DR

The user-reported pink/red full-frame flash on entering gameplay is the
**postproc COMPOSITE structurally over-brightening the venue during its
song-start lighting reveal**, NOT a clear-color / venue-not-drawn issue.
The venue is fully drawn (clear is black, ~349 meshes); the composite clips it
to a flat pink/white wash for the first ~1-2s, then it resolves.

Fix: soft-clip the venue postproc composite OUTPUT to a ceiling below pure white
(Reinhard rolloff, knee 0.82 → ceiling 0.97). One WGSL block in `fs_postproc`.

**Verified (interleaved N=16 A/B, same/paired binary):** first-frame venue
blowout rate **9-15/16 boots → 0/16** (max clipW ~83% → <0.55%); steady-state
venue + song-select luminance unchanged.

## Branches / commits

- Engine: `milo-native-engine` worktree, branch `wt-task-first-frame-flash`,
  commit **`8db8b3a64b9ccc9619e7f37241f57cc0cfce1a3c`** ("fix(rnd-rb3): tame
  gameplay-entry venue postproc white/pink first-frame flash").
- rb3: worktree branch `wt-task-first-frame-flash`, commit **`c95bbc8d`**
  (bump `MILO_ENGINE_PIN` 58254f7 → 8db8b3a, `native/CMakeLists.txt`).
- Base engine pin: `58254f7` (wave-4 composed state).

## Root cause (re-diagnoses the wave-4 `verify-venue-blowout.md` attribution)

The wave-4 doc guessed the `06_game_screen` wash was "framebuffer clear color
showing through before the venue draws." **That is wrong.** Evidence:

- `RENDER_DBG` per-frame: at the game-flip frames the clear is `(0,0,0)` BLACK
  and ~349 meshes / ~205k tris are drawn — the venue IS fully rendered. The
  wash is venue geometry tinted/blown, not the clear color.
- The wash is a **transient** that resolves over ~1-3s as the venue lighting
  reveal settles (per-frame burst `enter_baseline`: pink → white lum 220 /
  clipW 73% → settles to lum ~46).

### Isolation (the decisive part)

Rigorous interleaved A/B on ONE binary (env-gated, so the only difference is the
flag — kills the venue/song-phase lottery that made small-N runs misleading):

| config | blowouts (peak clipW >5% over the entry burst) |
|---|---|
| composite ON (default) | **9/16** boots, mean 19.8%, max 83% |
| `RB3_PP_OFF=1` (render direct, no composite) | **0/16** boots, mean 0.6% |
| composite ON + identity levels | 4/16 (mean 11.4) — levels are NOT the cause |
| composite ON + identity levels + bloom-off + noise-off | **3/16** (max 44%) |

So the postproc composite is the cause, and it is **NOT** any grade term
(levels/bloom/noise) — the wash survives with all of them neutralized. It is the
**intermediate→composite STRUCTURE** itself over-brightening the venue during the
bright song-start lighting reveal (the stage point-light flashes up at song
start; the native lit-path runs that peak hotter than the Wii GX backdrop, e.g.
`geom.env` point-light sum 0.18 → 2.5 at the flip). Rendering direct to the
framebuffer (PP_OFF) avoids it entirely.

> NOTE / honest caveat: I could not pin down the exact mechanism by which the
> intermediate→sample→composite path produces brighter pixels than a direct
> render at an identity grade (formats match: both `RGBA8Unorm` headless; the
> grade is mathematically identity at neutral params). The bisection is solid
> (composite = cause, grade terms exonerated), but the structural "why" is
> unresolved. The fix bounds the OUTPUT so it cannot wash regardless of the
> mechanism, which is why it is robust.

A failed first attempt (committed only on disk, then reverted): soft-clipping
the LEVELS remap (knee 0.88, asymptote → levelOutHi=1.0). It did NOT help —
N=12 A/B showed no improvement — because it still reaches white. Recorded here so
the next agent doesn't repeat it.

## The fix

`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, in the `fs_postproc` WGSL
(string `kRB3PostProcShaderSource`), just before the final
`return vec4f(clamp(color,0,1),1)` (≈ line 2542). Reinhard rolloff:

```wgsl
let ppCeil = 0.97;
let ppKnee = 0.82;
let ppSpan = ppCeil - ppKnee;
let ppOver = max(color - vec3f(ppKnee), vec3f(0.0));
let ppRolled = vec3f(ppKnee) + vec3f(ppSpan) * (ppOver / (ppOver + vec3f(ppSpan)));
color = select(color, ppRolled, color > vec3f(ppKnee));
```

Properties:
- **Identity below the knee (0.82)** — correctly-exposed venue / menu /
  song-select frames are untouched (they never reach the knee).
- **C1-continuous rolloff** above the knee → no banding.
- **Ceiling 0.97 < pure white** → a hot venue moment compresses to a bright-but-
  readable result; it physically cannot reach the clipW threshold (~0.98), so the
  flat white/pink wash is impossible.
- The composite grades ONLY the venue backdrop. The gameplay highway, gem cores,
  and HUD draw UNGRADED onto the framebuffer AFTER the mid-frame flush
  (`FlushPostProcMidFrame`), so this NEVER dims them.
- WGSL-only → applies identically on web and native.

## Verification

Harness: `/tmp/rp5-first-frame-flash/capture_enter.py` (boots headless via the
keyboard-to-gameplay nav, tight-polls health, bursts ~30 frames at the
game_screen flip). Metric: `peak_all.py` / `measure.py` — `clipW` = % pixels
≥250 in ALL channels (the wash class). Venue/song is random per boot, so I used
**interleaved batches on the same/paired binary + large N** to beat the lottery.

- **Fix works (final clean-binary A/B, before=PROBE, after=FINAL):**
  - N=16: before 9-15/16 blowouts → after **0/16** (max clipW 0.52%).
  - N=8 confirmation: before 3/8 (max 15.7%) → after **0/8** (max 0.48%).
- **No regression — steady-state venue:** settled-frame luminance comparable
  before/after (50-76 both, overlapping; differences are venue variance), clipW
  identical (~0.3-0.5). The soft-clip is identity at those brightness levels.
- **No regression — song-select** (`song-select-capture.py`): luminance
  identical (dLum ±0.4 = noise); only the pure-white sliver (~0.4% of pixels)
  drops 255→~251, imperceptible. Visual: white text / album-art / UI all crisp.
- **Visual:** `evidence/BEFORE_first_game_frame.png` (whole venue washed
  pink/white, only highway readable) vs `evidence/AFTER_first_game_frame.png` +
  `evidence/AFTER_clean_venue.png` (wood-club venue fully detailed — dartboard,
  poster, paneling, band char, gems — cleanly lit, no wash).

Evidence dir: `/tmp/rp5-first-frame-flash/evidence/` (BEFORE/AFTER first-frame,
BEFORE white-blowout lum220, AFTER song-select).

### Wii byte-identical
No Wii-compiled source touched. The only edit is in `milo-native-engine`
platform/gfx code (`Rnd_Wgpu_RB3.cpp`), which is NOT compiled into the Wii DOL
target. `git diff --stat` on the rb3 worktree shows zero `src/band3` / `src/system`
changes. Byte-identical by construction (no objdiff needed).

## Landing notes

- **Engine first:** land `8db8b3a` (`milo-native-engine`). Single file
  `src/platform/Rnd_Wgpu_RB3.cpp`, one +26-line WGSL block inside
  `kRB3PostProcShaderSource`'s `fs_postproc` (≈ line 2542, immediately before the
  closing `return vec4f(clamp(...))`). Then bump `MILO_ENGINE_PIN` in
  `rb3/native/CMakeLists.txt` (rb3 commit `c95bbc8d`).
- **Sibling conflict surface:** `Rnd_Wgpu_RB3.cpp` is touched by multiple
  wave-tasks. My edit is in the postproc fragment shader (the `fs_postproc`
  return region) — distinct from the lit-path `softClipLighting`
  (`standard_wgsl.inc`), the venue scene-uniform lighting (`WriteSceneUniforms`),
  and the bloom/halo regions. It composes with the wave-4 lit-path soft-clip
  (different stage: per-mesh lighting vs whole-venue composite).
- If another wave-5 task also edits `fs_postproc`, the only collision point is the
  ~26-line block before the final `return`; trivially re-appliable after theirs.
- Tunables if the ceiling needs adjustment per art review: `ppCeil` (0.97) and
  `ppKnee` (0.82). Lowering `ppCeil` darkens bright highlights more; raising the
  knee narrows the rolloff band.
