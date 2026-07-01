# task-fret-sphere — fix the wave-2 held-fret giant-white-sphere regression (impl)

**Issue key:** `fret-sphere` (wave-4) · **Implementer (opus)** · 2026-06-14
**Status:** DONE · verified=true · **engine-repo fix** (rb3 gets only the pin bump)
**Ports used:** 9011-9019 · **Evidence:** `/tmp/rp4-fret-sphere/`

---

## TL;DR

The wave-2 held-fret smasher glow rendered as a **giant white sphere (~110px)** hovering
above the now-bar, occluding gems, in bloom-heavy venues (documented repro: 20th Century
Boy, jump 49500, B-heavy sustain). Two compounding causes, both in the game.cam highway
path:

1. **`gem_smasher_glow.mat` was a halo-bloom source** (`IsHaloSourceMat`). The smasher
   glow is a **now-bar-sized plate textured with a SOFT radial glow**, not a small gem
   core — blooming it produced the giant soft sphere. (`RB3_HIGHWAY_BLOOM_OFF=1` removes
   the sphere entirely → bloom is the size driver, proven.)
2. **The wave-2 ×2.0 now-bar emissive boost** over-brightened the *colored* emissive
   sample so its bright core clamped to **white**, washing the per-slot hue
   (green/red/yellow/blue/orange) to near-white.

**Fix (engine):** exclude `gem_smasher_glow` from the halo classifier (like `surface.mat`),
and reduce the now-bar emissive boost ×2.0 → ×1.25. The held-fret glow now stays **small
+ per-slot colored** in all venues, gems are no longer occluded, and the gem-core halo
bloom + `RB3_FRET_GLOW_OFF` opt-out are both preserved.

## Root cause (corrected the verify-gems "suspect")

The verify-gems doc suspected `square_smasher_bright_*.tex` was "not bound on this path →
white fallback dominates." That was **wrong**. The texture IS bound and IS colored:

- `RB3_LIGHT_PROBE` shows the glow draws under `game.cam`/`track.env` with
  `mat='gem_smasher_glow.mat' color=(0,0,0) blend=2(kBlendAdd) emisMul=0.90
  emisMap=square_smasher_bright_red.tex` — the per-slot bright texture is correctly bound.
- `RB3_ISOLATE_MESH=gem_smasher_glow` (draw ONLY that mesh) gave the smoking gun:
  - **default (composed) build:** big WHITE rounded blobs, ~6000-14000 px, norm RGB
    ≈ (0.94-1.00, 1.00, 1.00) — white, oversized.
  - **+ `RB3_HIGHWAY_BLOOM_OFF=1`:** the blobs SHRINK and **turn colored** (cyan/blue/green
    frames, norm e.g. (0.43,1.00,1.00), (0.71,0.92,1.00)). So bloom is the size+white
    amplifier; the per-slot color is present underneath.
- The wave-2 evidence (`/tmp/rp2-fret-held/evidence/0[2-6]_AFTER_*.png`) shows the glow
  correctly colored — because that was the **menu/overshell smasher preview** (NOT
  game.cam): no track-light ×2 boost, no highway bloom. The regression is specific to the
  **gameplay highway compositing** that landed after wave-2 (halo bloom `59b7307`).

So the verifier's "per-slot color works in good venues" and "white sphere in this venue"
are the same code path; the difference was the bloom/×2 compounding, which only fires
heavily when the glow is shown continuously (a held sustain in a bloom-bright venue).

## What changed (files + why) — ALL in `milo-native-engine`

`src/platform/Rnd_Wgpu_RB3.cpp`, two region-scoped hunks (rb3 `src/` untouched → Wii
byte-identical):

1. **`BandRnd::IsHaloSourceMat` (~line 1969).** Added `gem_smasher_glow` to the
   halo-source exclusions next to `surface`. The smasher plate is no longer fed into the
   additive-halo bloom replay. Gem cores (`prism_gem`) still bloom (verified `halo=1`
   still fires). Opt back in for A/B with `RB3_SMASHER_HALO=1` (default-off).
2. **`BandRnd::DrawMesh`, the `gem_smasher_glow.mat` branch in the `sTrackLight &&
   game.cam` block (~line 4824).** `mu.emissiveMultiplier * 2.0f` → `* 1.25f`. Keeps the
   now-bar clearly brighter than authored (the wave-2 intent) without saturating the
   per-slot color to white. `RB3_FRET_GLOW_OFF=1` still zeroes it (unchanged).

A diagnostic `FRET_SPHERE_DBG` probe was added then **removed** before commit; the tree is
clean (`git diff` = only the two functional hunks).

## Branches + commit SHAs

- **engine** worktree `wt-task-fret-sphere` → **`b6733df36c66fafaa5cca7095d5246e521c38d02`**
  (`gfx(rb3): fix held-fret smasher glow giant-white-sphere regression`)
  - path: `/home/free/code/milohax/milo-native-engine-worktrees/task-fret-sphere`
- **rb3** worktree `wt-task-fret-sphere` → **`d084301527d9625c1d1b4ad96168bd1b68df221c`**
  (`build(native): bump MILO_ENGINE_PIN to b6733df`)
  - path: `/home/free/code/milohax/rb3/.claude/worktrees/task-fret-sphere`
  - base before pin bump: rb3 master `7f1c6a52`; engine base `469c550`.

## Verification (commands + results)

Harness: `/tmp/rp3-gems/song2_probe_bin.py` (by-shortname gameplay probe, `RB3_BIN`
override). All runs headless, ports 9011-9019, instances cleaned up.

**Repro / BEFORE (probe-build = no-fix):**
- `before-default/e_*.png` — giant white/dark sphere ≈(690,491) in **24/24** frames,
  occluding the highway (`e_006.png`). `RB3_FRET_GLOW_OFF=1` (`before-glowoff/e_006.png`)
  and `RB3_HIGHWAY_BLOOM_OFF=1` (`bloomoff/b_006.png`) each remove it → attribution
  confirmed (glow feature + bloom).
- Isolated glow BEFORE (`iso-glow/g_*.png`): white blobs 6000-14000 px, norm ≈ white.

**AFTER (committed fix):**
- `after-default/e_006.png`, `e_014.png` — sphere GONE; bounded now-bar glow, gems +
  highway unoccluded, venue/chars render fine.
- Isolated glow AFTER (`after-iso-glow/g_*.png`, `final-iso/f_*.png`): blobs now
  **300-8200 px** and **per-slot colored** — pure green (norm 0.02,1.00,0.00 / 0.89,1.00,0.21),
  blue (0.77,0.85,1.00 / ...B=1.00), cyan/red frames. `after-iso-glow/g_006.png` = small
  green sparkles (held green fret), no sphere.
- Side-by-side: `AB_20thcentury_sphere_before_top_after_bottom.png` (full scene),
  `AB_iso_glow_before_top_after_bottom.png` (isolated mesh).

**Multi-venue sweep (≥2 required; all 3 locally-playable songs):**
- `20thcenturyboy` (street/studio, the repro) — sphere fixed.
- `antibodies` (red venue, jump 15500, `after-song2/a_008.png`) — clean now-bar, no
  sphere (this venue never showed the sphere → no regression; `before-song2/a_008.png`
  confirms BEFORE was already clean here).
- `25or6to4` (green/purple venue, jump 30000, `after-song25/s_006.png`) — clean now-bar,
  colored buttons, gems visible.

**No regression:**
- Gem-core halo bloom STILL fires: `RB3_RENDER_DBG=1` →
  `[RB3_HALOBLOOM] … draws=5 … halo=1` (71 lines) in `after-renderdbg/`.
- The held-fret glow is still VISIBLE (feature preserved) — isolated AFTER glow is 300-8200
  px, never zero when shown.
- `RB3_FRET_GLOW_OFF=1` opt-out unchanged; added `RB3_SMASHER_HALO=1` to opt back into the
  old halo behaviour for A/B.
- Crazy Train SIGABRT seen in passing is a **pre-existing missing-asset** crash
  (`crazytrain.mid` absent from the local extract → empty `TrackInfo` vector `operator[]`
  assert) — reproduces identically on the main composed binary (`before-song3/`), NOT my
  change. Only `20thcenturyboy/25or6to4/antibodies` have extracted MIDIs locally.

## Wii byte-identity

No `src/` (Wii-matched) edits — both hunks are engine-side
(`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`). `git status src/` clean in the rb3
worktree → Wii build byte-identical (BandCharacter/Part/etc. untouched). objdiff not
needed (no shared-source delta).

## LANDING NOTES (for the orchestrator)

- **Order:** land engine `b6733df` first (make it reachable on engine main), then the rb3
  pin-bump `d0843015`. Standard one-way-dep.
- **rb3 conflict surface:** `native/CMakeLists.txt:74` (`MILO_ENGINE_PIN`) ONLY. If a
  sibling wave-4 task also bumps the pin, resolve cumulatively (point the pin at whichever
  engine commit lands last). No `src/` edits.
- **Engine conflict surface:** ONE file, `src/platform/Rnd_Wgpu_RB3.cpp`, two regions —
  exactly the surfaces other render-polish tasks touch, so cherry-pick carefully:
  - `BandRnd::IsHaloSourceMat` (~line 1969-1985): added a `gem_smasher_glow` exclusion
    branch + opt-out. Collides only with another bloom/halo-classifier edit.
  - `BandRnd::DrawMesh`, `gem_smasher_glow.mat` branch in the `sTrackLight && game.cam`
    block (~line 4809-4828): one constant change `2.0f → 1.25f` + comment. This is the
    same hot block the wave-2 fret-held / track-light / SP-overlay edits live in — watch
    for context conflicts; my hunk changes only the smasher `if`.
- **Build caveat (cosmetic):** a fresh worktree build dir needs `-DDawn_DIR=.../dc3-decomp-deps/dawn/lib/cmake/Dawn`
  on the first configure (Dawn isn't auto-found); the orchestrator's standard build already
  has this in its cache.
- **Tuning knob:** if a final visual pass wants a brighter now-bar, the ×1.25 is the dial
  (raise toward ×2 reintroduces white-wash); `RB3_SMASHER_HALO=1` reintroduces the halo.
