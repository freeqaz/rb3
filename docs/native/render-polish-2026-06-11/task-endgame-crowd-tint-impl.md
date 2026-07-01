# task-endgame-crowd-tint — diagnosis (Wave 5)

**Status: PARTIAL (diagnosis-only — no safe fix; the symptom is faithful authored
behavior, NOT a native bug).** A rigorous root-cause investigation with A/B
attribution, per-environ light probes, and a same-angle time-lapse proves the
endgame celebration crowd's "greenish/dark tint + foggy wash" is the authored
celebration look, not a render gap. No engine/rb3 change is landed (the
diagnostic probe was reverted; both worktrees are clean). Wii byte-identical by
construction (nothing touched).

Investigator: wave-5 implementer (Opus rigor). Ports 9251-9259. Evidence under
`/tmp/rp5-endgame-crowd-tint/` (decisive frames copied to `.../keep/`).
Baseline binary built from the worktree at engine pin `58254f7` (the composed
wave-4 build state).

---

## TL;DR

The endgame celebration crowd has TWO visible components, and BOTH are faithful
to the authored venue, neither is a native bug:

1. **"Greenish tint"** = the crowd's authored stage light `main_crowd.lit`
   (`crowd.env`) is **color-ANIMATED** — it cycles pink → green → yellow → pink
   (a disco/celebration color-wheel, driven by a `PropAnim` in the venue milo).
   The "green tint" screenshots are simply frames captured during the GREEN phase
   of the cycle. It is a transient phase (~20% of frames, periodic), not a
   constant tint. Proven by a same-camera-angle time-lapse: consecutive frames
   `tl_14` (crowd solid green) and `tl_15` (same crowd/angle, now warm neutral).

2. **"Foggy wash"** = mostly the authored celebration smoke machines
   (`crowd_smoke_a/b.part`, `cavern_smoke01.part` on `beam_fog.mat`,
   matColor.a = **1.00**) — stage smoke that is SUPPOSED to be there. The thin
   atmospheric haze (`fog_thin.mat` a=0.10, `cloud_a.mat` a=0.52) is already
   thinned by the wave-4 menu-fog fix (it fires for all matColor.a < 0.999
   systems, endgame included). With `RB3_PART_HAZE_OFF=1` the haze fully clears;
   the celebration smoke (α=1.0) correctly remains.

The report's premise ("greenish/dark tint, same CLASS as crowd-render + venue
lighting residuals") was a TRIAGE FALSE-POSITIVE: the crowd is rendering its
authored moody color-cycling venue lighting correctly. There is no
match-neutral fix that wouldn't deviate from the authored retail look.

---

## SYMPTOM (reproduced)

Reach the celebration via the wave-4 endgame path (jump to song end on
`coop_endgame_popups_screen`). The endgame camera cycles ~12 shots; on a couple
of angles the crowd reads strongly green with a grey haze film. Harness:
`/tmp/rp5-endgame-crowd-tint/endgame_capture.py` (mirrors `song-end-test.py`'s
boot→jump→endgame flow, adds `/api/screenshot` capture + env passthrough).

Decisive frames in `/tmp/rp5-endgame-crowd-tint/keep/`:
- `base_03.png`, `bl_05.png` — the symptom: green crowd + foggy upper frame.
- `green_phase.png` (=`tl_14`) vs `neutral_phase.png` (=`tl_15`) — **consecutive
  frames, same angle**: green → warm. The decisive proof it's an animated cycle.

## ROOT CAUSE (probe-proven)

### A/B attribution (env opt-outs, same scene)

| variant | crowd green excess | foggy wash | conclusion |
|---|---|---|---|
| BASELINE | periodic +6…+12 on ~20% of frames | present | (both components) |
| `RB3_VENUE_LIGHT_OFF=1` | **gone** (≤ +1.2 all frames; warm/white flood) | still some haze | **green = venue lighting** |
| `RB3_PART_HAZE_OFF=1` | still green (+9.2) | **gone** (crisp) | **wash = particle haze (NOT the green)** |
| `RB3_NO_CROWD_REBIND=1` | still green + bodies SHARDED | — | rebind is needed (shreds bodies); not a tint cause |

Quantified crowd-region green-excess (G − (R+B)/2), tools in the evidence dir:
- BASELINE long run: oscillates min −2.2 / max +9.2 / **mean +0.7**; strong-green
  (>+4) on **8/40 frames = 20%, periodic**.
- VENUE_OFF: neutral on every frame (max +1.2). Venue lighting is the green source.

### Why the venue light is green only sometimes — it's ANIMATED  [the key finding]

`RB3_VENUE_PROBE=1` + a custom `RB3_CROWDTINT_PROBE` (logged the active
`RndEnviron` + its summed showing-light color at each `*_crowd_body*` draw under
`world.cam`) showed the crowd is lit by `crowd.env`, whose single point light is
`main_crowd.lit`. That light's color is **not static** — across one endgame the
probe captured `crowd.env` lightSum sweeping smoothly through:

```
(0.93, 0.69, 0.99)  pink     (727 draws)
(0.10, 0.95, 0.46)  GREEN    (694 draws)   <- the "tint"
(0.95, 0.84, 0.08)  yellow   (138 draws)
... + a continuous ramp BETWEEN them (0.77,0.60,0.58)…(0.57,0.80,0.76)… etc.
```

That is a color-wheel animation. With the crowd's low authored ambient
(`crowd.env` amb = 0.09) and dark/desaturated body textures, the single cycling
light dominates → the crowd takes the light's hue. The green frames are the green
phase of the cycle. Confirmed in the venue milo
(`world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox`): it carries
`PropAnim` objects + `main_crowd.lit` + `saved_color.grp`/`failed_color.grp`
lighting groups (outcome-dependent celebration lighting) — i.e. the crowd light
IS authored to animate color.

On a couple of angles the crowd also draws under `theater.env`
(lightSum 2.49,1.19,1.55 warm) / `rooftop_foreground.env` (2.00,0.64,0.14
orange) — those are warm, not green, so they are not the green source. The green
is purely the `crowd.env` cycle.

### Why gameplay crowd is never green (the clean control)

Gameplay uses `game.cam`, which does NOT take the venue-light path (it's
byte-identical to the pre-venue-light white-flood default). So the gameplay crowd
is warm/white-lit and never green (`keep/gameplay_crowd_neutral.png`). The
endgame uses `world.cam` → the venue-light path → the authored animated venue
lights. The difference is the camera, and it's correct.

### The wash is mostly authored celebration smoke

`PART_PROBE=1` in the endgame: the dominant near-crowd systems are
`crowd_smoke_a/b.part`, `crowd_smoke_a01/b01.part`, `cavern_smoke01.part` — all
on `beam_fog.mat`/`beam_fog04.mat` with **matColor.a = 1.00** (so the wave-4
haze thinner, gated on a < 0.999, intentionally leaves them alone). These are the
celebration stage smoke machines and are authored to be visible. The genuinely
thin atmospheric systems (`fog_thin.mat` a=0.10, `cloud_a.mat` a=0.52,
`cloud_a01.mat` a≈0.48) are already thinned by the landed menu-fog fix.

## WHICH RESIDUAL OWNS IT

- The **green** belongs to the **venue-lighting** class — but specifically it is
  CORRECT venue lighting (an authored animated light), so it is not an open
  defect. The only adjacent open lever is the venue *exposure / ambient floor*
  tuning (PLAN.md wave-5 "menu hub contrast" / venue-blowout follow-up #2): the
  crowd ambient floor is very low (0.07–0.09) so a single cycling light fully
  saturates the crowd. Lifting that floor would soften the green PEAK, but it
  would also flatten the authored moody contrast and is NOT endgame-specific —
  it's the same global venue-light exposure knob. Not safe to touch for this
  cosmetic, no-ground-truth case.
- The **wash** belongs to the **menu-fog / particle-haze** class, and the
  actionable part of it (thin street haze) is already fixed; the rest is authored
  α=1.0 celebration smoke.

## WHY NO FIX IS LANDED

1. The green is the authored `main_crowd.lit` color cycle — matching this IS the
   correct behavior. Desaturating it would deviate from retail.
2. No retail endgame-celebration ground-truth screenshot exists
   (`images/retail-screenshots/` has menu/song-select/gameplay only). Without it,
   "less green" is an unfounded aesthetic change, not a verified fix.
3. The only match-neutral lever (venue ambient floor) is a global venue-lighting
   exposure knob with high blast radius across every venue/scene — it belongs to
   the open venue-exposure residual, not a targeted endgame fix, and the task
   explicitly says to NOT force a risky landing.

This is the "rigorous root-cause + ranked fix design = SUCCESS without a forced
landing" outcome the task allows.

## RANKED FIX OPTIONS (if a future wave decides the green peak should be softened)

All require a retail ground-truth capture FIRST (REFERENCE SCREENSHOTS NEEDED).

1. **Raise the crowd ambient floor (preferred if anything).** In
   `BandRnd::WriteSceneUniforms` (engine `Rnd_Wgpu_RB3.cpp:1203-1205`) the venue
   ambient is floored at 0.07. Raising the floor (e.g. 0.12-0.15) for the crowd
   env specifically would mix in white and pull the green peak toward grey while
   keeping the hue. Risk: changes the moody contrast of EVERY venue env (the
   floor is global to the venue-light path), so it must be gated to `crowd.env`
   by name to stay scoped — and it's still an authored-look deviation.
2. **Desaturate the venue light color toward white above a saturation threshold.**
   Same WriteSceneUniforms light loop (`:1240-1251`). Higher blast radius (touches
   all venue lights, e.g. the gameplay highway venue accents) — not recommended.
3. **Do nothing (recommended).** It's faithful.

Engine line references are at pin `58254f7`.

## VERIFICATION

- Symptom reproduced + captured (`base_*`, `bl_*`, `tl_*`).
- A/B attribution decisive (`no_venuelight/` removes green; `no_haze/` removes
  wash, green persists).
- Animated-cycle proof: same-angle consecutive frames `tl_14` (green) /
  `tl_15` (neutral) + the per-draw lightSum sweep in the CROWDTINT probe log.
- Gameplay crowd control: `game.cam` crowd is warm/neutral, never green — so the
  wave-2 gameplay crowd fix is NOT regressed by anything here (nothing changed).
- No fix landed → no regression possible; the baseline binary == composed
  wave-4 state.

## LANDING NOTES (orchestrator)

- **Nothing to land.** rb3 worktree `wt-task-endgame-crowd-tint` and the paired
  engine worktree `wt-task-endgame-crowd-tint` are both CLEAN (the diagnostic
  `RB3_CROWDTINT_PROBE` block in `Rnd_Wgpu_RB3.cpp` was reverted). No engine pin
  bump. `wiiByteIdentical` = N/A (no source touched).
- File the green-peak softening (option 1) under the existing venue-light
  exposure residual, NOT as an endgame task. It needs a retail endgame-celebration
  ground-truth capture first (add to REFERENCE_SCREENSHOTS_NEEDED.md).
- Worktrees can be torn down after this doc lands (they hold only the reverted
  probe + a stock baseline build).

## Evidence index (`/tmp/rp5-endgame-crowd-tint/`)
- `keep/green_phase.png` + `keep/neutral_phase.png` — same angle, consecutive
  frames, green→neutral (DECISIVE: animated cycle).
- `keep/base_03.png`, `keep/bl_05.png` — the symptom.
- `keep/novenue_03.png` — venue-light OFF: green gone, warm crowd.
- `keep/nohaze_03.png` — haze OFF: wash gone, green persists.
- `keep/gameplay_crowd_neutral.png` — game.cam crowd, warm, no green (control).
- `corr/engine-c-9258.log` — CROWDTINT per-draw lightSum sweep (the cycle).
- `probe/engine-probe-9256.log` — VENUE_PROBE full env/light dump.
- `partprobe/engine-pp-9256.log` — PART_PROBE endgame fog systems (beam_fog α=1.0).
- harnesses: `endgame_capture.py`, `endgame_timelapse.py`.
