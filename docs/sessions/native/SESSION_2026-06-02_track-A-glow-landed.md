# Session 2026-06-02 (cont.) — Track-A gameplay glow LANDED + venue-environ unblocked

Continuation of the Track-A (gameplay FX/HUD parity) work begun in
`SESSION_2026-06-02_track-A-fx.md` (A1 hit-flames). This session landed the
**gameplay glow/lighting** stack and **venue-environ** — both of which the prior
session had concluded were blocked on a "deep, multi-session-hard" venue-environ
bring-up. **That premise was wrong.** Opus-led, with adversarial multi-agent
verification + main-loop image adjudication.

## Headline outcomes

| Item | Outcome |
|---|---|
| **#1 — Gameplay track glow/lighting (A2 gems + A4 highway/lanes + A3 strike-glow)** | ✅ **LANDED.** Engine `milo-native-engine` `f5ee015`; pin rb3 `7e2fe9a9`. |
| **#2 — Venue-environ bring-up** | ✅ **LANDED.** rb3 `d988a301` — a **one-line** transposed `ObjPair` fix, match-neutral. |

## The reframe that unblocked everything

The prior session believed A2/A3-glow/A4 were "coupled, scene-lighting-led, and
**blocked** on venue-environ bring-up (a deep world-subsystem task)." Both halves
were wrong:

1. **Gameplay lighting is decoupled from the venue.** The highway/gems/HUD draw
   under their **own camera** (`game.cam`, near30/far224); the venue/band/crowd draw
   under `world.cam`. `WriteSceneUniforms` is already re-invoked per-camera. So a
   `game.cam`-scoped change lights the track **without touching the venue** — and
   without any `RndEnviron`/scene-lighting port (which the prior session feared would
   white-flood the scene).
2. **The highway gray was its own prelit base, not ambient.** Probe-confirmed:
   `flat_curve.mesh`/`surface.mat` is **prelit white** (ignores scene lighting). So the
   fix is a per-material scale, not a lighting port.
3. **Venue-environ was never hard.** `WorldInstance::SyncDir` had a transposed
   `ObjPair(foundObj, it)` (from = a fresh null-Dir copy → trips
   `MILO_ASSERT(p->from->Dir())`); DC3 + the target binary use `ObjPair(it, foundObj)`.
   Match-neutral swap (96.63135% unchanged) + delete the 69-line deferral hack → venue
   backdrop renders. The prior "inlined-cached-shared proxy instancing gap / obstructions
   (a)(b)(c)" audit was diagnosing a symptom.

## #1 — what shipped (engine `f5ee015`, `Rnd_Wgpu_RB3.cpp`)

All scoped to `cam->Name() == "game.cam"`; **default-on**, `RB3_TRACK_LIGHT_OFF=1`
opt-out (mirrors `RB3_BLOOM_OFF`):

1. Darken the prelit `surface.mat` highway ×0.12 → dark track so gems pop.
2. Re-enable material EMISSIVE — `mu.emissiveMultiplier = mEmissiveMap ? mult : 0`
   (guard essential), bind `mEmissiveMap` to slot 5 (`MakeMaterialBindGroup`, was
   hardcoded `mBlackView`). Lights gem cores (`prism_gem_emissive`), the additive
   strike-line `gem_smasher_glow`, and the surface watermark. The dark surface is what
   makes emissive net-POSITIVE here (the prior session's revert tested it against the
   bright gray surface, where it washed out + blew out prelit trails).
3. Lit lanes — force `rails.mat` prelit (softened ×0.7); `rails.tex` was a non-prelit
   lit mesh that rendered as near-black dividers on the dark surface.
4. Brighter now-bar — boost the additive `gem_smasher_glow` emissive.

Also adds a gated `RB3_LIGHT_PROBE` per-mesh dump (cam/prelit/blend/color/diffuse/
emissive) used to diagnose all of the above — kept as a debug aid for the deferred
follow-ups.

**Verification:** a 3-agent adversarial workflow (regression / parity / code-scoping)
returned **clear_improvement, high confidence, zero genuine regressions**: no blowout
(saturation *increased*, gem hue preserved, white-clip 0.15%→0.28%), HUD/fret-buttons
pixel-identical, scoping provably inert off `game.cam` (`mu{}` zero-inits
`emissiveMultiplier`; set only in the game.cam branch). Landed combination
(glyph-fix `f52f2f1` + track-light) re-confirmed: builds, tree-identical to `f5ee015`,
gameplay renders correctly end-to-end (`/home/free/tmp/trackA-shots/landed/`).

## Deferred #1 follow-ups (NOT done)

- **Gem bloom-halo** — retail gems have a soft additive halo. The engine's V2 BloomPass
  runs only on the **venue intermediate**, but the highway draws *after* the venue grade
  flush (Tier-2 overlay seam), so it isn't bloomed. Needs a highway-layer bloom pass.
- **Star-power blue track overlay** — `peakstate_plane.mesh` (`spotlight_*_track.tex`,
  alpha 0) only shows when SP deploys; not exercised by the autohit harness.
- **Lane blue-tint** — lanes read white (asset `rails.tex` is white); retail's are
  bluer/subtler (partly from venue/SP tint that doesn't exist natively yet).
- **Venue backdrop's own lighting** (`world.cam`) — `RndEnviron::sCurrent` may still be
  the degenerate default even now that venue geometry loads; lights the band/stage/crowd,
  not the highway. Re-probe before pursuing.

## Method (carry forward)

- **Probe-first caught a wrong assumption again.** My first plan ("darken `game.cam`
  ambient") was wrong because the highway surface is prelit; the `RB3_LIGHT_PROBE` dump
  corrected it before any wasted implementation. MILO_LOG is swallowed natively — use
  `fprintf(stderr)` + `grep -a`.
- **Isolated worktrees, then land via cherry-pick.** rb3 worktree (`setup-worktree.sh`) +
  a separate engine worktree under **`~/tmp`** (NOT `/tmp` — tmpfs); native build pointed
  at it via `-DMILO_ENGINE_PATH` + `-DDawn_DIR` + `-DCMAKE_CXX_COMPILER=clang++`. Engine is
  the separate `milo-engine` cmake target. Concurrency moved both `master` and engine `main`
  mid-session (a concurrent commit even clobbered an unstaged pin edit) — re-check live SHAs
  before every commit; cherry-pick onto the moved HEAD rather than fast-forward-merge.
- **Adversarial verify, main-loop adjudicates images.** Reviewers do pixel analysis; the
  main loop reads the actual frames + compiled source for the verdict.

Tools: `scripts/native/gameplay-depth-capture.py` (boots headless to gameplay, RB3_HTTP).
Retail refs: `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_{guitar,drums_starpower}.png`.
Memory: `project-a234-emissive-glow-shared-rootcause`, `project-a4-scene-lighting-env-empty`
(both rewritten to RESOLVED).
