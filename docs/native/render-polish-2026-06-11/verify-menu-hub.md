# verify-menu-hub — Wave-3 composed-build verification (menu-lighting + neon-slab/Part.cpp)

Wave-3 verifier, 2026-06-11. Read-only judgment of the MERGED build
(`native/build-native/rb3-native`, rb3 `cca1869a`, engine pin `469c550`) against
retail (`images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`,
`..._menu_playnow_submenu.png`). Evidence: `/tmp/rp3-menu-hub/` (65-frame dense
hub-loop capture `hub_f*.png`, isolate/A-B runs, live hide-test, `measure.py`/
`greenscan.py` harnesses). Ports 8831–8839, all instances torn down.

## VERDICT: PARTIAL

The two landed fixes each do what they claim — **the saturated green slab is
dead across the whole loop** and **unlit/emissive materials render at retail
brightness** (sign zone matches retail to within 3%). But the fixes
**interact**: the Part.cpp sim fix resurrected the street-fog particle systems
that were broken-absent (flown to y≈−11,800) in the build where menu-lighting's
contrast win was verified. The revived fog renders as a dense **green-grey
full-frame wash** in a chunk of the camera loop and as a global haze floor in
the rest — the wave-2 "contrast 3.3:1 → 12.5:1" claim does **not** reproduce on
the composed build (max 3.9:1 over all 65 frames; retail ~10:1).

## Metric method

Same as the wave-2 impl doc (`analyze.py`): mean RGB, R:B warmth ratio, 3×3-cell
luminance contrast (max/min cell), mean luminance, green-excess fraction
(`G − max(R,B) > 0.2` = the scout's saturated-slab metric; I added a *soft*
variant `> 0.03` that catches the desaturated wash, plus a saturated-green
locator `G>0.35 ∧ G>1.4R ∧ G>1.4B`). Retail baselines (recomputed):

| ref | meanRGB | R:B | contrast | mlum | hard-green | soft-green |
|---|---|---|---|---|---|---|
| menu_hub | 0.236/0.182/0.135 | 1.75 | 10.2:1 | 0.193 | 0.00% | 3.40% |
| playnow_submenu | 0.219/0.167/0.120 | 1.82 | 13.5:1 | 0.178 | 0.00% | 3.79% |

(Retail's ~3.4–3.8% soft-green = authored green neon; that's the "clean" band.)

## What PASSES on the merged build

1. **Saturated green slab: FIXED, whole loop.** Hard green-excess ≤ 0.55% on
   all 65 frames (`hub_f00400..03600`); wave-1 baseline (`/tmp/rp-menu-lighting/
   von_f*.png`) measured up to **38.9%** on the same metric. Scout pass-criterion
   #5 ("no slab ≥5% across a full loop") met with 10× margin.
2. **ARCADE/green neon reads as thin tubes, not slabs.** The only saturated-green
   pixels outside wash shots are thin tube glyphs (e.g. the question-mark neon
   squiggle, `crop_f2450_green.png`; storefront tubes in `crop_f2750_green.png`,
   0.12–0.20% of frame). Confirms the neon-slab task's "mesh was never the slab"
   conclusion on the composed build.
3. **Unlit + emissive materials at retail brightness.** On the retail-matched
   BARBER shot (live capture `fog_off.png`, fog hidden — see below), the
   upper-centre sign cell is **0.368 vs retail 0.357** (within 3%; scout
   criterion #3 target ≥0.30 ✓). BARBER SHOP sign, TATTOO neon, marquee
   bulb-strings, `_ill` windows all visibly self-lit (`hub_f01800.png`,
   `hub_f02850.png`, `fog_off.png`). Wave-2 Fix 1+2 behavior present and
   correct in composition.
4. **Warmth restored where fog is light.** Playnow-matched shot `hub_f02300.png`:
   R:B **1.77 vs retail 1.82** (criterion #1 target ≥1.4 ✓). Loop median ~1.5;
   pre-fix neutral-green 1.10 is gone. (Fog-washed shots drop to 0.78–1.3,
   see residuals.)
5. **Song select: no emissive washout.** `songselect/native_depth_{00,08,16}.png`
   normal contrast, crisp text, no new glow. (Known pre-existing issues visible:
   grey album-art box + FRIEND RANKINGS overlay — already queued in PLAN;
   header shows garbage digits after "SORTED BY SONG NAME" — noted below.)
6. **UI chrome / menu text** unaffected throughout (force-prelit path).

## What FAILS / regressed in composition

### F1 — Residual green-grey FOG WASH (the re-baselined "green wash" is NOT zero)

The saturated cyan slab is dead, but a **desaturated green-grey wash** covers
large frame fractions in roughly a quarter of the camera loop (soft-green
metric; retail band is ≤3.8%):

| frame | soft-green % | note |
|---|---|---|
| hub_f02500 | **74.2%** | full-frame cloud, scene unreadable (`hub_f02500.png`) |
| hub_f03100 | **56.7%** | bottom-right half washed |
| hub_f02650 | 34.0% | BARBER shot washed left ⅔ |
| hub_f02950 | 29.3% | |
| hub_f03200 | 23.0% | |
| hub_f02750 | 8.4% | retail-ref camera shot, washed |
| rest of loop | 0.02–2.9% | within/near retail band |

Saturated-green locator agrees: f2500 16.4% / f3100 10.0% of pixels at
`G>1.4·max(R,B)` — the wash is genuinely green-tinted, not neutral haze.

**Attribution (live same-shot hide-test, `wash-hide-test.py` — immune to the
camera-desync confound):** at a wash shot (f2905, soft 9.8%), runtime-hiding
`forground_smoke01.part` + `background_red_fog.part` via `/api/dta/eval
set_showing FALSE` dropped soft-green **9.8% → 3.2%** (= retail baseline)
within the same shot; re-showing restored it (18.2%). Sky domes/
difference_clouds: no effect. **The wash is the street-fog particle systems.**
Evidence pair: `wash_on.png` (smothered) vs `fog_off.png` (dark night street,
glowing BARBER/TATTOO neon — visibly close to the retail ref).

Sim state is *sane* (PART_PROBE: counts 5–20/system, positions parked at the
street y≈−342, warm-red authored colors, no negative alphas) — i.e. the
Part.cpp fix works as verified. The defect is downstream in **how DrawParticles
renders them**: particles whose probed vertex colors are deep red
(0.33–0.41, 0.07, ~0.0) composite as a *green-grey brightening* wash
(particle-only frames `iso_f*.png` are pure red on black — so the hue flip
happens in the blend against the scene; srcAlpha factors look correct in
`PipelineManager::MapBlend`, so suspect the alpha/texture term: `fog.tex`
alpha decode, the color-ramp end state, or a missing size/alpha fade).
Authoring ground truth says fog should be *subtle* (speed 0.01, warm, alpha
ramps down); retail's same shot has mild haze with deep blacks.

### F2 — Authored contrast still missing; wave-2's headline number does not hold

Whole-loop 3×3 contrast on the merged build: **1.4–3.9:1, never ≥5:1**
(scout criterion #2: ≥5:1, retail 10–13:1). The impl doc's "f1050 3.3→12.5:1"
was real on its build (`/tmp/rp2-menu-lighting/after/von_f01050.png` re-measures
12.5:1, mlum 0.131) — but that build's fog was **broken-absent** (pre-Part.cpp:
particles at y≈−11,800 = invisible except as the slab). On the composed build
the revived fog adds a global haze floor: loop-wide mlum floor is **0.209**
(vs 0.12–0.13 dark shots on the wave-2 build), and the retail-matched BARBER
shot measures (same camera shot, live pair):

| | R:B | contrast | mlum | darkest cell | bottom row |
|---|---|---|---|---|---|
| merged, default (`wash_on.png`) | 0.78 | 1.4:1 | 0.376 | 0.309 | 0.43/0.43/0.39 |
| merged, fog hidden (`fog_off.png`) | 1.30 | 2.4:1 | 0.293 | 0.182 | 0.44/0.33/0.30 |
| retail ref | 1.75 | 10.2:1 | 0.193 | **0.035** | 0.245/0.155/**0.035** |

So even fog-aside there is a second residual: **the dark cells never get dark**
(0.18 vs 0.035 — the street floor/sidewalk runs 3–9× retail brightness). That
is the scout's Fix 3 (venue-heuristic re-tune: 0.07 ambient floor + grey 0.6
key on the `ue=1` minority — sidewalk/brick/props), explicitly deferred from
wave 2 — confirmed still needed. Fog removal alone recovers only 2.4:1.

### F3 — Score screen UNREACHABLE (sweep blocked, pre-existing)

`song-end-test.py --require-endgame` on the merged build: gameplay + game-over
PASS, then SIGABRT before the endgame screen — `MetaPerformer.cpp:1079 Error:
netServer`, data stack `ui/endgame/endgame_helpers.dta(64):meta_performer`
(log `/tmp/rb3-song-end-8838.log`). Identical to the wave-2 impl doc's blocker,
so **not a regression** — but note the wave-2 `all-inst-crash` TheNet.mSession
wiring did *not* cover this path. The emissive-regression sweep of score
screens therefore could not be performed. Already in PLAN's wave-3 queue.

## Methodology warning for the tuning pass (false-positive trap, hit twice)

Cross-run frame-locking is INVALID on the hub: camera-shot pacing drifts
several seconds between boots, and washes follow the *shot*, not the frame
number. My first attribution (postproc chain — `RB3_PP_OFF=1` "removed" the
wash) was refuted by a default-config control run over the same frame numbers
coming back clean (`plain_f*.png` ≤0.35% soft). The neon-slab task's hide-test
confusion was the same trap. **Use the live same-shot hide-test pattern**
(`/tmp/rp3-menu-hub/wash-hide-test.py`: poll screenshots for the wash metric,
then `set_showing` toggles within ±0.3 s) or whole-loop distributions — never
per-frame pairs across runs.

## Numbers for the tuning pass

- Fog wash target: soft-green ≤ ~4% on every shot of the loop (retail 3.4–3.8%);
  currently 5 shots in 8.4–74%.
- Haze floor: loop mlum floor 0.209 → retail-matched dark shots want ~0.13–0.19;
  fog-hidden floor is 0.293 on the BARBER shot (fog is only ~28% of the excess
  there — the rest is the ue=1 floor lighting, Fix 3).
- Dark-cell target: darkest 3×3 cell ≤ ~0.07 on night-street shots (retail
  0.028–0.035; merged never below 0.08, retail-matched shot 0.18 fog-off).
- Contrast target: ≥5:1 on sign shots (retail 10–13:1; merged max 3.9:1).
- Keep: sign-cell brightness 0.357–0.368 (at retail), R:B 1.7–1.8 on matched
  shots (at retail), hard-green 0% (fixed).

## New issues noticed in passing (off-topic)

1. **Song-select header garbage digits**: "VIEWING ALL 83 SONGS, SORTED BY SONG
   NAME" is followed by stray digits (`1843…`) in `songselect/native_depth_00/
   08/16.png` — looks like an unformatted token/number in the header format
   string. Possibly the known stale-slot/Text-class issue; flagging because it
   reproduces at every depth on the merged build.
2. Wave-2 menu-lighting impl noted `/api/dta/eval` Color sub-property reads
   crash (also in neon-slab doc) — still unfixed; I avoided those getters.

## Evidence index (`/tmp/rp3-menu-hub/`)

| file(s) | what |
|---|---|
| `hub_f*.png` (65) | dense default-config loop capture, f400–f3600 every 50 |
| `wash_on.png` / `fog_off.png` / `sky_off.png` / `restored.png` | live same-shot hide-test (THE decisive pair) |
| `iso_f*.png` + `iso.log` | particles-only frames (RB3_ISOLATE_MESH=zzznothing) + PART_PROBE dump |
| `isocloud/isodome/isodiff_f*.png` | mesh-isolation candidates (all refuted) |
| `ppoff/noiseoff/bloomoff/plain_f*.png` | postproc A/Bs + the control run that refuted them |
| `songselect/native_depth_*.png` | song-select regression sweep |
| `/tmp/rb3-song-end-8838.log` | score-screen crash log |
| `measure.py` / `greenscan.py` / `wash-hide-test.py` | harnesses (reusable) |
