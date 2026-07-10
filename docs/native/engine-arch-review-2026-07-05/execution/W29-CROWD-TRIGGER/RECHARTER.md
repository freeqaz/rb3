# W29-CROWD-TRIGGER — RE-CHARTER (Lever B) → supersedes the W28 CROWD narrative

**Status:** Lever B accepted. This is the Lever-B deliverable: it names the REAL
main_hub walkers, their NATIVE state (working), the WORKING TRIGGER MECHANISM, and the
CORRECTED acceptance target set, with STEP-0-grade evidence. All claims trace to
`evidence/raw/step0-trace-v2.log.gz` (grep the probe tags) and
`evidence/step0-play-backtraces-symbolized.txt`, binary `5a430eea`.

## The one sentence

The main_hub streetslomo crowd walk **works natively** — the W23→W28 "missing walkers"
was a measurement artifact of the `CHARDRV_PROBE=crowd` filter, which observed only the
8 idle cityscape (`crowd_*`) proxies and never the 4 animating streetslomo walkers
(`player0-3`, `char/main`).

## Why the bug was mis-scoped for six waves (W23→W28)

Every prior wave (including W28's own STEP-0) probed with `CHARDRV_PROBE=crowd`. That
filter matches a driver only if its owning-dir name or `mClipType` contains "crowd". It
matches the 8 `char/crowd/crowd_{male,female}0N` proxies — which play `crowd1-5` during
the sv8 splash and then sit idle. It does **not** match the `player0-3`
(`char/main/main.milo`) drivers that actually play the streetslomo walk. So every wave
saw "the crowd froze / zero plays after 2.433" and never saw the walk that was firing on
the un-probed drivers one filter away. W29 booted with `CHARDRV_PROBE='*'` and the walk
appeared immediately.

W28's specific claims, re-adjudicated:
- "Zero `CHARDRV_PLAY` after beat 2.433" → **false** (filter artifact). `player0-3`
  play `player3_m/player2_f/player1_f/player0_m` at 2.433 and loop.
- "`PanelDir::Enter streetslomo_ao nTriggers=0`" → **true, but a red herring.** The
  walk is driven by `BandCamShot::StartAnim` (camera-shot anim), not the PanelDir
  UITrigger list. `nTriggers=0` never implied the walk couldn't fire.
- "The 8 shared proxies rebind correctly to streetslomo but are never driven" → the
  rebind is correct; the proxies ARE undriven — but they are the **cityscape** crowd,
  not the streetslomo walkers, and their idle is **faithful** (no crowd clips exist in
  streetslomo).

## The REAL main_hub walkers

| Property | Value | Evidence |
|---|---|---|
| Char dirs | `char/main/main.milo` × 4, dirs `player0`, `player1`, `player2`, `player3` | CHARDRV_PLAY / CLIPSWAP `drvPath` |
| Driver | `main.drv` (CharDriver), driven via `BandCharacter::PlayGroup` | backtrace (i) |
| Bound clip set on main_hub | `clips (world/vignette/shell/sv3/a/streetslomo/streetslomo_clips.milo)` (resident) | CLIPSWAP beat=2.433, PathName-asserted |
| Walk clips (PLAYED) | `player0_m` (player2), `player1_f` (player3), `player2_f` (player1), `player3_m` (player0) — one per driver; loops | CHARDRV_PLAY beat 2.433 + replay ~25.3 |
| Trigger mechanism | `BandCamShot::StartAnim()` (BandCamShot.cpp:357) → DTA anim script → `BandCharacter::OnPlayGroup` | symbolized backtrace |
| Sustained animation | `playing=2209/2280` frames, `FirstPlaying()!=NULL` after 71-frame splash starve | CHARDRV_LIFE |
| Rendered / visible | fully rendered, textured, lit, mid-stride; matches retail | `evidence/shots/*.png` vs `yt_mhKNp9uAT48_menu_hub.png` |

## The 8 `crowd_*` proxies (what W23→W28 measured) — faithful idle

| Property | Value |
|---|---|
| Char dirs | `char/crowd/crowd_{male,female}0N.milo` × 8, `main.drv`, `mClipType='crowd'` |
| Splash (sv8) | play `crowd1-5.clp` at beat 0 (cityscape) — animating |
| main_hub (sv3) | rebind to `streetslomo_clips` at 2.433 (correct), but that bank has NO crowd clips and the streetslomo camshot script plays `player0-3` only → idle (`playing=71` frozen) |
| Verdict | **FAITHFUL.** Cityscape-only actors, harmlessly rebound. Not the main_hub walkers. Driving them would be a hack. |

## W29 ACCEPTANCE TARGET SET — CORRECTED (evidence-based, supersedes W28 CA4)

The W28 target set named the 8 `crowd_*` drivers. STEP-0 proves that conflated the idle
cityscape proxies with the real walkers. The clip NAMES were right; the DRIVER identity
was wrong. Corrected set (**MET** this wave):

1. **Target drivers:** the 4 `player0-3` (`char/main/main.milo`) `main.drv`
   CharDrivers, while `main_hub_screen` is active and their `mClips` resolves to
   `streetslomo_clips.milo` (PathName-asserted, not just count). — **MET** (CLIPSWAP).
2. **Animating criterion:** each target driver has `CHARDRV_PLAY` of a `playerN_{f,m}`
   clip AFTER beat 2.433 and `FirstPlaying()!=NULL` (`animating>0`) sustained on
   main_hub, driven by `BandCamShot::StartAnim`. — **MET** (CHARDRV_PLAY + CHARDRV_LIFE).
3. **Do NOT** count the sv8 cityscape `crowd_*` proxies as the walkers; their idle
   after 2.433 is faithful. A census that measures the `crowd_*` family (the
   `CHARDRV_PROBE=crowd` trap) is measuring the wrong crowd. — the W23→W28 root error.

## Recommended disposition for the coordinator

- **Close the CROWD narrative chain.** The walk works and renders; there is no lever.
  This lane writes no fix and reserves `RB3_VIGNETTE_TRIG_REPLAY` unused.
- **The folded verts=0/near-black thread is MOOT** — the walkers have geometry and are
  visible. If a Wave-30 lane wants it, the residual is a *lighting/saturation* delta
  vs retail (native reads brighter) and the arm IK spike-fan shard (PROP/BandPatchMesh,
  Lane 2's `RB3_PROP_POSE_FULL` surface) — both distinct from "missing crowd".
- **Census guidance:** any future crowd census MUST pin `mClips==streetslomo_clips`
  AND target the `player0-3` drivers (not `crowd_*`), or it re-enters the six-wave trap.
- **rb3-tests:** 10 GPU/WebGPU-family SEGFAULTs observed in the shared tree (headless
  adapter init; not this lane's code) — coordinator to confirm against a clean env.

## What this lane did NOT do (scope honesty)

No fix code, no flag added (reserved-unused), no default flip, no pin bump, no
census/classjson/sidecar/golden edits. Did not touch `CharDriver.cpp`/`CharClip*.cpp`/
`CharIKHand.cpp`/`boot-to-song.py` (READ-ONLY), the `Crowd.cpp:884-1000` gameplay
oracle, or the RndMesh loader. Added only
`scripts/native/_w29_crowd_trigger_boot.py` and this lane's docs/evidence.
