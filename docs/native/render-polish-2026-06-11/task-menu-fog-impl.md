# task-menu-fog — impl (Wave 4)

Fixes the menu-hub fog over-render flagged PARTIAL in `verify-menu-hub.md`: the
Wii-matched `InitParticle` sim fix (rb3 `745ee256`) correctly resurrected the
shell street-fog particle systems, but they rendered as a dense green-grey
full-frame wash (composed-build contrast 3.9:1 vs retail ~10:1). This wave thins
them toward the retail thin-haze look **without touching the Part.cpp sim**.

## Result

VERIFIED. Engine-only change → **rb3 `src/` byte-identical, zero Wii-match
impact** (the only rb3 commit is the `MILO_ENGINE_PIN` bump). The menu street-fog
now reads as a thin atmospheric haze that reveals the neon/storefronts behind it;
the worst-case green-grey wash dropped from **39.8%–61.7%** soft-green to a peak
of **5.3%** (retail band 3.4–3.8%) across the whole camera loop, drift-immune.
Gameplay venue FX, A1 hit-flames, and song-select are unaffected (the fix is
scoped to dampened-alpha haze systems only).

## Root cause

Three layers, established with the drift-immune live same-shot hide-test
(`/tmp/rp4-menu-fog/hide-attrib.py`) — never per-frame pairs across boots, which
are dominated by camera-shot pacing drift (confirmed: the dense-loop soft-green
distribution is statistical noise; only same-shot toggles and the drift-free
early frames f400–600 are valid A/Bs).

1. **The billboard renderer dropped the material register color.**
   `BandRnd::DrawParticles` (and the dc3 `Part_Wgpu.cpp` it was modelled on) drew
   `c = tex × p->col` only — it never multiplied by `mat->GetColor()`. The shell
   fog materials carry authored alpha dampeners (`fog_thin.mat`=0.10,
   `fog.mat`=0.50, `cloud_a01.mat`=0.48) that scale them to a thin haze; with that
   term dropped the revived fog rendered at up to **10× its intended opacity**.
   (Live PART_PROBE: `background_red_fog.part`/`forground_smoke01.part`, blend=3
   SrcAlpha, 20 particles each, sizes ramping to ~130u, per-particle alpha up to
   0.68 — all sim-correct; the over-render is downstream in the draw.)
2. **The Wii fog is authored heavier than the retail (360/PS3) target.** Even with
   matColor restored, the dominant `fog.mat` (0.50) red fog still washed —
   matColor alone barely moved the metric. Our DATA path is Wii; our visual ground
   truth is the retail 360/PS3 hub, whose same walking-band shot
   (`yt_mhKNp9uAT48_menu_playnow_submenu.png`) shows the band readable against deep
   blacks with only a faint warm haze.
3. **The camera dollies THROUGH the fog volume.** The worst single shot (one
   walking-band close-up) still smothered the frame at 60%+ because a large soft
   fog sprite (size ~130u) whose centre is near/behind the camera fills the whole
   billboard. This is the "camera inside the fog" case; retail keeps the band
   readable there, so the on-screen fog must fade as the camera enters it.

NOT the cause (ruled out): texture alpha decode (`fog.tex` decodes as a proper
soft cloud — `/tmp/rp3-menu-hub/iso_f02500.png` is wispy red-on-black, not a hard
square); additive clouds/bokeh/stars (hide-test: hiding them after the red fog
changes soft-green by ~0%); the postproc chain; the sim (Wii-matched, untouched).

## What changed (engine repo only)

All edits in the paired engine worktree
`/home/free/code/milohax/milo-native-engine-worktrees/task-menu-fog`
(branch `wt-task-menu-fog`), commit **`d72d837`**
(`d72d8373c1e673e4c77afae1eaeb472f11f24cfe`), based at engine `469c550`.

`src/platform/Rnd_Wgpu_RB3.cpp` — `BandRnd::DrawParticles` only (+64/−1), inside
the per-particle vertex-build region. All three measures are **scoped to
`matColor.a < 0.999` (dampened-alpha haze systems)**, so the common
`matColor==(1,1,1,1)` venue FX / A1 hit-flames take none of them:

1. **Fold `mat->GetColor()` RGBA into the per-vertex color** (`c = tex × p->col ×
   matColor`). Milo material model; no-op for `(1,1,1,1)`. Opt-out
   `RB3_PART_MATCOLOR_OFF=1`.
2. **Extra haze alpha scale** (`RB3_PART_HAZE_SCALE`, default **0.35**) on
   dampened-alpha systems — pulls the heavier Wii fog to the retail thin-haze
   level. Chosen by the drift-free f400 BABOON-shot A/B (0.5/0.35/0.25 — 0.35 is
   thin haze with crisp neon; 0.25 over-clears, 0.5 leaves a film). Opt-out
   `RB3_PART_HAZE_OFF=1`.
3. **Near-camera fade**: per particle, forward distance from the camera in units
   of its own half-size; alpha fades to 0 by the time the centre is at/behind the
   camera, full once it's ≥ 2 half-sizes ahead. Kills the "camera inside the fog"
   smother. Opt-out `RB3_PART_NEARFADE_OFF=1`.

## Branches + commits

| repo | branch | commit | what |
|---|---|---|---|
| milo-native-engine | `wt-task-menu-fog` | `d72d837` (`d72d8373c1e673e4c77afae1eaeb472f11f24cfe`) | DrawParticles haze fix |
| rb3 | `wt-task-menu-fog` | `c82e012d` | `MILO_ENGINE_PIN` 469c550 → d72d837 |

Engine wt: `/home/free/code/milohax/milo-native-engine-worktrees/task-menu-fog`.
rb3 wt: `/home/free/code/milohax/rb3/.claude/worktrees/task-menu-fog`.

## Verification (REAL fixed binary)

> Methodology note (learned the hard way): the CoW worktree reflinks the engine
> source per-checkout. An early build used `MILO_ENGINE_PATH=$(cat .engine-path)`
> while I had edited the MAIN engine repo, so that binary was the UNMODIFIED
> engine and its "results" were variance. All numbers below are from the binary
> built from the WORKTREE engine file (Rnd_Wgpu_RB3.cpp.o recompiled, verified).

Evidence dir `/tmp/rp4-menu-fog/` (harnesses `hubcap.py`, `hide-attrib.py`,
`measure.py`, `greenscan.py`).

### Drift-immune live same-shot hide-test (the decisive measure)

| build | worst wash shot found | red-fog contribution | contrast @ worst |
|---|---|---|---|
| FIX OFF (all 3 envs disabled) | **39.8%** soft (deeper search hit 61.7%) | 36% over baseline | 1.5–1.7 |
| FIX ON (default) | **5.3%** soft (whole-loop search at a 5% threshold) | **1.2%** over the 3.7% baseline | 3.1 |

With the fix on, an 8%-threshold search found **no** wash shot across a full loop
(timed out) — wash shots are now rarer/lower; lowering to 5% caught the single
worst at 5.3%, essentially the retail band.

### Drift-free early-frame matched A/B (f450/f500, identical camera shot)

`real_base/h_f00450..00500.png` (envs off) vs `real_final/…` (default):
BASE = thick red fog film smothering the PALACE/MUSIC/BABOON-NEST storefronts and
the walking band; FINAL = red wash cleared, neon crisp, band visible, thin warm
haze. (Metrics misleading here — the red film added spurious warmth; removing it
drops R:B 2.04→1.32 as the true scene shows through. The visual is decisive.)

### Regressions (REAL binary)

- **A1 hit-flames**: smasher-region brightness FIX-ON top-3 0.498/0.490/0.482 vs
  FIX-OFF 0.506/0.469/0.466 — equivalent. Their materials are `matColor.a≈1.0` →
  skipped by the `<0.999` guard. `rgame_on/burst_25.png` flame glow at full
  intensity. (`/tmp/rp4-menu-fog/rgame_{on,off}/`).
- **Gameplay**: `keyboard-to-gameplay.py --game-burst 50` reached `game_screen`,
  songMs advancing; `rgame_on/07_playing.png` highway/gems/venue (CORK bar, crowd,
  band) lit + correct, no washout, venue fog intact.
- **Song-select**: `song-select-capture.py` normal — list/text/panels crisp, no
  emissive washout, no new glow (`final_songselect/native_depth_08.png`). The grey
  album-art box + FRIEND RANKINGS overlay + header garbage digits are pre-existing
  (already in PLAN/verify), not this change.

## LANDING NOTES (orchestrator)

- **Commit order**: land engine `d72d837` FIRST (so the SHA is reachable), THEN
  the rb3 pin bump `c82e012d`. rb3 commit ONLY changes `native/CMakeLists.txt`
  (`MILO_ENGINE_PIN`), no `src/` → Wii byte-identical by construction.
- **Engine conflict surface**: my diff is a single tight region in
  `src/platform/Rnd_Wgpu_RB3.cpp` — `BandRnd::DrawParticles` (lines ~5294–5396,
  the camera-axes block + a new haze/near-fade setup block right after, plus the
  per-vertex `cr/cg/cb/ca` line and a `~12`-line near-fade block inside the
  particle loop). The wave-2 `menu-lighting` (engine `c5c94a2`) and `neon-slab`
  (engine `7d6252d`, diagnostics) siblings touch **`DrawMesh`** and
  `standard_wgsl.inc` / `UniformStructs.h`, NOT `DrawParticles` — **no overlap
  expected**. The neon-slab `PART_PROBE` block (also in `DrawParticles`, later in
  the function, ~5400+) is additive and untouched by me; if both land, keep both.
- **No shader/bindgroup/struct change** — the matColor + haze + fade all fold into
  the existing per-vertex color on the CPU. No `MaterialUniforms` / WGSL edits, so
  zero interaction with menu-lighting's `unlit` field work.
- **Tunables / opt-outs** (all default to the fix being ON): `RB3_PART_HAZE_SCALE`
  (default 0.35), `RB3_PART_HAZE_OFF=1`, `RB3_PART_MATCOLOR_OFF=1`,
  `RB3_PART_NEARFADE_OFF=1`. If a future venue regresses (an authored
  dampened-alpha system that SHOULD be heavy), the cleanest follow-up is to scope
  the haze-scale + near-fade to `world.cam` / the shell dir rather than all
  dampened-alpha systems; gameplay venues checked clean here so this was not
  needed.
- **Residual (out of scope, pre-existing)**: the verify doc's Fix 3 (the `ue=1`
  venue-heuristic floor — sidewalk/brick never reach retail's deep blacks) is
  independent of the fog and still open; the fog fix gets the dark cells most of
  the way there but the floor lighting is a separate lever.
