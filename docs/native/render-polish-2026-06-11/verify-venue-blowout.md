# verify-venue-blowout — wave-4 independent adversarial review

**Reviewer:** independent Opus verifier (not the implementer). Composed build,
rb3 master `9c1f4449`, engine pin `58254f7` (soft-clip = engine `1abd595`).
**Evidence (all my own, captured fresh):** `/tmp/rp4rev-venue-blowout/`.

## VERDICT: **CONFIRM**

The GX-faithful `softClipLighting` (Reinhard knee 1.0 → ceiling 1.05) tames the
genuine venue lighting blowout decisively, with **zero dimming** of correctly-lit
scenes (median clip unchanged). I independently REPRODUCED the blowout on a true
pre-fix build, confirmed the fix removes it, and **AGREE with the implementer's
re-diagnosis** that the dominant `06_game_screen` red/white wash is a separate
**clear-color load transient** (venue not yet drawn), not a lighting blowout.
No interaction regression from the other wave-4 fixes was seen in the sweep.

---

## Method (controlled A/B, my own builds)

There is **no runtime opt-out** for the soft-clip — it is a pure WGSL shader
change in `standard_wgsl.inc` (confirmed: no `getenv`/uniform gate; `RB3_VENUE_LIGHT_OFF`
toggles a *different* path, the venue-light vs white-flood fallback). So I built a
true BEFORE binary:

- Paired throwaway worktree `rp4rev-venue` (rb3 + engine), engine at pin `58254f7`.
- In the engine worktree, `git revert --no-commit 1abd595` (clean, no conflict —
  the soft-clip is isolated) → **BEFORE** shader = unbounded `baseColor.rgb *
  (ambientLight + diffuse*shadow)`; restored to HEAD → **AFTER** shader with
  `softClipLighting`.
- Built BOTH from the SAME worktree/config (clang, Dawn from dc3-decomp-deps) so
  the ONLY difference is the shader. Binaries:
  `/tmp/rp4rev-venue-blowout/rb3-native-{BEFORE,AFTER-matched}` (md5
  `da6f06fa…` / `8a08c144…`).

Metric tool: `/tmp/rp4rev-venue-blowout/measure.py` — `clipW` (% pixels ≥250 in
ALL channels = the blowout class), `clipAny`, luminance, mean RGB, `detailSD`
(luminance stddev = texture/contrast proxy).

### Caveat I discovered (sharpens the implementer's "frame-locked" claim)

The implementer's strict `_framepin_capture.py` A/B is **NOT** as deterministic as
claimed across boots. Pinning `game` frame N lands at very different real frames
(roundtrip lag: pin 700 → captured ~4012/4076) AND boot-to-boot variance picks a
different random quickplay **song + venue** (my pf2400 BEFORE was *20th Century
Boy / CORK club*, a different scene). So pf700 BEFORE-vs-FIXED was likely *not*
identical content. I therefore did NOT rely on single-frame pins; I used a
**burst-distribution** method (many consecutive game-screen frames per boot ×
multiple boots) which is robust to camera-shot randomness. It reaches the same
conclusion as the implementer's headline, more defensibly.

## (a) Hot venue blowout — REPRODUCED and TAMED

3 boots × 25–30 game-burst frames per binary = **80 frames each**, `--diff hard`
quickplay (Antibodies / small_club, hot window songMs ~16–23s):

| metric | BEFORE (no clip) | AFTER (soft-clip) |
|---|---|---|
| **max clipW** | **32.2%** | **4.3%** |
| frames >5% clipW | 1 | 0 |
| frames >2% clipW | 5 | 1 |
| frames >1% clipW | 9 | 3 |
| **median clipW** | **0.37%** | **0.38%** |
| mean clipW | 0.97% | 0.51% |

- The blowout is **real and reproduced** on the pre-fix build:
  `burst_BEFORE/burst_01.png` — clipW **32.2%**, RGB (211,163,179), the crowd-club
  room blown to flat pink-white, wall/window/crowd detail destroyed. Highway/gems/HUD
  (game.cam) stay crisp (the wash is venue-only). This is the user-reported symptom.
- The fix **eliminates the blowout class**: AFTER max clipW 4.3%, no frame >2% that
  isn't the load transient (see (c)). The single AFTER 4.3% frame
  (`burstB3_AFTER/burst_02.png`) is the authored pink moment rendered CORRECTLY —
  wood-grain, hex window, hair all readable; the 4.3% is bright highlights, not a
  white-out.
- **Detail preserved:** AFTER hot frames keep `detailSD` 65–73 vs the washed
  BEFORE frame's flat smear. Visual A/B (`burst_BEFORE/burst_01.png` vs
  `burst_AFTER/burst_00.png`): same pink tint, texture intact.

Implementer claimed pf700 clipW 4.0%→0.3%. I can't reproduce that exact pin
(non-determinism above), but my burst stats show the SAME effect at population
scale: the hot tail collapses (max 32→4%, >2% frames 5→1).

## (b) No dimming — CONFIRMED (median + menu self-noise)

- **Median clipW byte-equal: 0.37% → 0.38%.** The bulk of (dark, correctly-lit)
  frames is untouched — the soft-clip is identity below the 1.0 knee, as designed.
- **main_hub** (framepin, deterministic): lum 0.385→0.387, clipW 1.53%→1.53%.
  BEFORE-vs-AFTER meanAbsDiff 24.31 is *smaller* than the BEFORE-vs-BEFORE
  self-noise floor 25.28 (animation-phase jitter) → no systematic change.
- **song_select**: lum 0.298→0.295 (noise), clipW 0.38%→0.38%, B-vs-F diff 5.73.
- **dark venue frames** (median band): identical (median 0.37/0.38 above).
- **score screen**: structurally immune — the results UI is prelit, and the shader
  takes `if (prelit || unlit) { finalColor = baseColor.rgb; }` BEFORE the lit
  branch, so the soft-clip cannot touch it. The endgame path reaches game-over on
  the composed build (`song-end-test.py` PASS, port 9118) — the sibling endgame fix
  works and the soft-clip doesn't regress it.
- **highway/gems/HUD (game.cam)**: unaffected — they go through the emissive/bloom
  path, not the `else` lit-compose branch the fix touches. Crisp in every frame.

## (c) `06_game_screen` red wash — AGREE it's a load transient, NOT a blowout

This is the most important adjudication, and the implementer is RIGHT.

Same-boot AFTER progression (the fix is ON):

| frame | clipW | note |
|---|---|---|
| `06_game_screen` (first game frame) | **37.97%** | washed |
| `07_playing` (~1 s later) | 1.56% | already resolved |
| `burst_02`+ (steady) | 0.28% | normal dark venue |

Two independent proofs it is a clear-color transient, not lighting:

1. **The fix does NOT remove it.** On the FIXED build, `06_game_screen` is *still*
   37.97% clipW (even higher than the BEFORE boot's 4.45%, due to camera-shot
   variance) — if it were a lit-path blowout the soft-clip would have tamed it. It
   doesn't, because the wash is not lit geometry.
2. **It resolves within ~1 s of the SAME boot** (37.97% → 1.56% → 0.28%) as the
   venue scene finishes drawing. Visual (`burst_AFTER/06_game_screen.png`): the
   right ~60% of the frame is a flat pink/magenta field with NO geometry — the
   framebuffer clear color showing through before the venue draws — while the left
   (hex window, drum kit, wall) is real, correctly-lit geometry and the highway is
   crisp.

So: I **AGREE** the dominant `06_game_screen` wash is a **separate first-frame
clear-color load transient** (venue not yet drawn over a near-white `mClearColor`),
distinct from the genuine lit-path over-drive the soft-clip fixes. The wave-3
`verify-venue-wash.md` attribution (all wash = lighting blowout) was partly wrong;
the implementer's re-diagnosis corrects it. Follow-up (clear to black during venue
load / defer first present) is correctly filed and out of scope for a shader change.

## Interaction sweep (other wave-4 fixes) — OK

menu hub → song select → gameplay (lit venue) → game-over: no NEW visual wrongness
attributable to the soft-clip. Menus within self-noise; gameplay venue correctly
lit + detailed; gems/HUD crisp; endgame reaches game-over (endgame fix intact).
No asserts/crashes/NaN in any of my 6 burst boots + 4 framepin boots + song-end run.
`interactionsOk = true`.

## Residuals / honest caveats

- The `06_game_screen` first-frame clear-color wash is a **real, still-open**
  follow-up (correctly filed by the implementer; not this fix's job). A casual
  observer screenshotting the very first game frame will still see a pink flash.
- The framepin harness's cross-boot non-determinism means the implementer's exact
  "pf700 4.0%→0.3%" number is not independently reproducible frame-for-frame; the
  *effect* is confirmed at population scale. Not a defect in the fix — a note on the
  measurement method.
- The authored dark-red venue look is still unvalidated against retail/Xenia for
  this song/venue (pre-existing REFERENCE-NEEDED item from wave 3). The fix is
  GX-faithful by construction (identity ≤1.0), so this doesn't block CONFIRM.

---

## Wave-3 section (preserved)

The wave-3 adjudication lives at `verify-venue-wash.md` (root-caused the blowout to
the unbounded lighting sum, exonerated the wave-2 changes, FAIL verdict requesting
the clamp). This wave-4 doc supersedes its *attribution* of the `06_game_screen`
wash (that frame is a load transient, not a blowout) and confirms the landed
soft-clip fix. See `verify-venue-wash.md` for the original method/evidence index.
