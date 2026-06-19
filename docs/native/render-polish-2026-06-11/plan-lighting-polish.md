# plan-lighting-polish — implementation plan (wrap-up, 2026-06-19)

PLANNER doc. Closes the three lighting-vs-retail residuals left open after the
7-wave render-polish campaign. **Engine-only, lit-path / scene-uniform exposure.**
KEEP the wave-4 `softClipLighting` (standard shader) + wave-5 postproc clip
(`fs_postproc`) as backstops — this plan tunes the *underlying* exposure so they
stop having to catch the over-bright reveal.

- **tractable: partial.** Sub-item (2) VENUE SONG-START EXPOSURE is the high-value,
  well-isolated, tractable fix — implement it fully. Sub-item (1) MENU HUB CONTRAST
  is **floor-lever-exhausted** (see Evidence below — pushing the floors lower no
  longer moves the dark cell; the residual is bright-side, not the ue=1 floor) — do
  the small in-scope part (point-light/grey-key trim shared with (2)) and defer the
  bright-side gap. Sub-item (3) ENDGAME BACKDROP TINT is taste-only and rides the
  same point-light exposure lever as (2) for free.
- **needsEngine: yes.** All changes are in the shared engine
  (`milo-native-engine`), `src/platform/Rnd_Wgpu_RB3.cpp` `WriteSceneUniforms` (and
  optionally one line in `src/gfx/standard_wgsl.inc`). No rb3 `src/` touched →
  **Wii byte-identical by construction** (engine is not compiled into the Wii DOL;
  the only rb3 commit is the `MILO_ENGINE_PIN` bump).
- **risk: medium.** The venue lit-path is shared by gameplay venue backdrop AND the
  song-select / score-screen backdrops (all draw under `world.cam` — see the
  CORRECTION below). A blanket lighting-sum scale would touch all of them. The plan
  scopes the change to the **point-light + grey-key contribution** and makes it a
  soft, env-tunable knee so correctly-exposed (dark) backdrops are untouched.

---

## CRITICAL CORRECTION to prior campaign docs (verified this session)

`task-venue-blowout-impl.md` follow-up #3 claimed the gameplay venue uses the
`else` fallback branch (white 1.0 dir + 0.45 ambient) because "the world.cam
venue-light path never fires during gameplay (camera is game.cam)." **This is
WRONG and the plan depends on the correction.**

Live `RB3_LIGHT_PROBE` during gameplay (small_club, charted song; evidence
`/tmp/rp8-lighting-polish/fp-game-9814.log`, retained):

```
  18921 world.cam        <- the gameplay VENUE BACKDROP is lit here
   1781 overshell.cam
    484 [ui.cam]
     62 Cam.cam
```

So the gameplay venue backdrop IS lit by the **venue-light path** (lines
1220–1299 of `WriteSceneUniforms`), reading the real `RndEnviron` point/dir lights
per-environ. The `else` fallback (0.45/1.0) is only for non-venue/non-world.cam
cams. `game.cam` (the highway/track) is a separate, untouched look. This is why
the song-start reveal goes hot: the venue's authored stage lights (crowd disco,
foreground reds, lamppole red=3.0) are summed on the native lit-path and bloom past
retail's GX-clamped backdrop.

`song_select` and the score screen ALSO draw a venue backdrop under `world.cam`
(verified: song_select has 2805 world.cam WriteSceneUniforms calls) — **so they
are the at-risk no-regression scenes for any venue-lit-path change**, not just
gameplay.

---

## Root cause (per sub-item)

### (2) Venue song-start exposure — REAL, tractable, high value

The native lit-path renders the venue backdrop's authored stage lights HOTTER than
the Wii GX backdrop, so the song-start reveal reads as a flat over-bright **pink**
field (peak lum ~200, red/pink tint) on disco-lit venues like small_club. Evidence:

- `RB3_PP_OFF=1` (composite off, raw lit-path direct) gameplay frame
  `/tmp/rp8-lighting-polish/game_ff_ppoff.png` = the whole small_club room washed
  flat pink/magenta (mlum 0.667, dark-cell 0.420 — nothing is dark). The default
  composited frame `game_settled_default.png` is the same venue, soft-clipped down
  but still pink-washed (mlum 0.466). So the soft-clip is *bounding* the white-out
  (its job) but the underlying reveal is structurally over-bright.
- `RB3_VENUE_PROBE` shows the hot lights (gameplay, world.cam): `main_crowd.lit`
  color `(0.93,0.69,0.99)` range 200 (the pink crowd light), `foregroundred`
  `(2.0,0.64,0.14)`, `lamppole.lit` `(3.0,0.14,0.00)`, `trainlight02` `(2.0,1.81,1.17)`.
  Raw color components reach **3.0**, CPU-clamped per-channel to **1.8 (point)** /
  **1.5 (dir)** at `Rnd_Wgpu_RB3.cpp:1272–1284`. Up to 4 point + 4 dir lights sum
  unbounded into the lit term, then `linearToSrgb` lifts the mids hard. Net: a
  ~1.0–1.8 lit term × a pale texture → a saturated pink room.

The fix: pull the venue lit-path exposure down so the reveal reads like retail's
darker, more saturated backdrop — NOT by lowering the soft-clip knee (that would
crush correctly-exposed venues), but by scaling the **light contribution** (the
per-channel clamps + an env-tunable point/dir exposure scale) so the hot pile-up
lands closer to the GX ceiling without needing the clip to catch it.

### (1) Menu hub contrast — floor-lever EXHAUSTED; residual is bright-side

The wave-5 menu-contrast fix (`facaa6a`) lowered three floors
(`RB3_VENUE_AMBIENT_FLOOR` 0.07→0.008, `_CLAMP` 0.25→0.09, `_GREY_KEY` 0.6→0.22).
The brief asks to push from ~6.8:1 (loop median) toward ~10:1 by re-tuning the
ue=1 venue-heuristic floor (the deferred wave-3 Fix 3). **A direct A/B this session
shows the floor lever is spent:**

frame-pinned hub (frame ~203), 3×3-cell luminance contrast (`/tmp/rp8-lighting-polish/measure.py`,
matches the documented method — measures the retail ref at 10.53:1 / dark 0.034):

| config | floors (FLOOR/CLAMP/GREY) | contrast | dark-cell | bright | mlum |
|---|---|---|---|---|---|
| old (pre-fix) | 0.07 / 0.25 / 0.6 | 3.78:1 | 0.154 | 0.581 | 0.377 |
| **default (current fix)** | 0.008 / 0.09 / 0.22 | **4.60:1** | 0.123 | 0.565 | 0.354 |
| darker candidate | 0.003 / 0.05 / 0.12 | 4.53:1 | **0.125** | 0.566 | 0.352 |
| retail ref | — | **10.53:1** | **0.034** | 0.362 | 0.190 |

(Single-frame numbers run lower than the reviewer's 6.8:1 loop median — different
camera phase; the *relative* lesson is what matters.) **Pushing the floors lower
(0.003/0.05/0.12) does NOT lower the dark cell** (0.123 → 0.125, noise) and does
NOT raise contrast. Why: the hub backdrop brick is lit by the authored *point
lights* (lamppole/road/theater), not the ambient floor — so once the floor is
already ~0, the dark zones are held up by point-light bleed + `linearToSrgb` mids,
which the floor lever cannot reach.

Visual confirms it (`hub_default.png` vs retail `yt_mhKNp9uAT48_menu_hub.png`):
native's brick/wall mid-tones are warm-grey where retail's are near-black; native's
bright cells are actually *brighter* than retail (0.565 vs 0.362). So the gap is:
native's whole backdrop sits ~2× too bright in the mids — the SAME over-exposure as
sub-item (2), just on the menu environs. The lever that helps both is the
**point-light exposure scale** (item-2 fix), NOT a further floor cut.

**Scope decision for (1):** the floor re-tune is done. In-scope here = let the
shared item-2 point/dir exposure scale apply to menu environs too (it darkens the
point-light mid-bleed → raises contrast). The remaining bright-side gap (retail's
neon hotspots a touch punchier) is the wave-2 emissive/register lever — explicitly
**deferred, out of scope** (changing emissive risks the gem/smasher bloom work).

### (3) Endgame backdrop tint — taste, rides item-2 for free

`endgame-crowd-tint` (wave 5) already adjudicated the green/pink as the FAITHFUL
authored `main_crowd.lit` disco color-wheel (pink→green→yellow). The brief asks
only to soften the green *peak* if quick. The crowd is lit by the same
`main_crowd.lit` point light under `world.cam` → the item-2 point-light exposure
scale softens its peak (green and pink alike) at no extra cost. **No separate code.**
If the green specifically is still too punchy after item-2, the optional add is a
per-channel green trim in the same clamp block — but only if a quick A/B shows it;
do not add speculative code.

---

## Exact files + approach

### Engine repo only (`milo-native-engine`)

**File: `src/platform/Rnd_Wgpu_RB3.cpp`, function `BandRnd::WriteSceneUniforms`**
(the `world.cam` venue-light block, lines ~1220–1299; helper getters ~1106–1114).

1. **Add an env-tunable venue lit-path exposure scale** next to the existing
   `sVenueAmbientFloor/Clamp/GreyKey` getters (~line 1114), same cached pattern:

   ```cpp
   // Venue lit-path exposure. The native lit sum (ambient + Σ point/dir diffuse)
   // runs hotter than the Wii GX backdrop on disco-lit venues, so the song-start
   // reveal reads as a flat over-bright pink field. Scale the LIGHT contribution
   // (point + dir colors) down toward the GX-clamped look so the soft-clip
   // backstops instead of doing the work. Ambient is NOT scaled (it's already
   // floored low); this only tames the bright stage-light pile-up. Default chosen
   // by A/B (see VERIFICATION); 1.0 = old behavior (full revert).
   static float sVenuePointExposure() { static float v = sVenueEnvFloat("RB3_VENUE_POINT_EXPOSURE", 0.70f); return v; }
   static float sVenueDirExposure()   { static float v = sVenueEnvFloat("RB3_VENUE_DIR_EXPOSURE",   0.80f); return v; }
   ```
   (Starting points — TUNE in the A/B below. Expect point ≈0.6–0.75, dir ≈0.75–0.9.)

2. **Apply the scale to the per-light colors** inside the venue block, at the
   existing clamp lines. Multiply BEFORE the `std::min(..., 1.5f/1.8f)` clamp so a
   raw-3.0 light still clamps but a moderate light scales proportionally:
   - dir (lines 1272–1274): `s.lightColors[dl][k] = std::min(lc.<c> * sVenueDirExposure(), 1.5f);`
   - point (lines 1280–1282): `s.pointLightColors[pl][k] = std::min(lc.<c> * sVenuePointExposure(), 1.8f);`
   - grey no-light key (line 1294): `const float grey = sVenueGreyKey() * sVenueDirExposure();`
     (so the ambient-only-env grey key is dimmed in lockstep — helps menu contrast
     item 1 too).

   Rationale for scaling the *light colors* rather than the final lit sum: it is the
   minimal, value-only change to fields already written; it leaves the soft-clip
   (`standard_wgsl.inc`) exactly as a backstop; and it is identity at exposure=1.0
   so `RB3_VENUE_POINT_EXPOSURE=1 RB3_VENUE_DIR_EXPOSURE=1` is a clean full revert
   for the reviewer's A/B (no rebuild). No struct/layout/bind-group change → zero
   uniform-layout risk.

3. **Do NOT touch** the `softClipLighting` knee/ceiling in `standard_wgsl.inc`
   (keep 1.0/1.05) or the `fs_postproc` ppKnee/ppCeil (keep 0.82/0.97). They stay as
   backstops, per the brief. The whole point is the reveal no longer *reaches* them.

**Optional (only if item-2 A/B shows the reveal still reads hot at sensible
exposure, e.g. needs <0.5):** instead of (or in addition to) the color scale, apply
a single uniform multiply on the lit term in the shader's lit branch
(`standard_wgsl.inc` ~line 833): wrap `softClipLighting(...)` argument in a
`scene`-carried exposure. This needs a `_pad` slot in `SceneUniforms` repurposed
(there are several: `_padN[3]`, `_padPL[3]`) → keep `sizeof==656`. Prefer the CPU
color-scale (no layout change); reserve the shader-uniform path as a fallback if
per-channel scaling proves insufficient.

### rb3 repo

- `native/CMakeLists.txt`: bump `MILO_ENGINE_PIN` to the new engine SHA (the ONLY
  rb3 change). Land engine commit first so the SHA is reachable, then the pin bump.

### Engine worktree discipline (per the campaign rules)

Edit ONLY the paired engine worktree (`<wt>/.engine-path`), never
`/home/free/code/milohax/milo-native-engine` directly. Build with
`-DMILO_ENGINE_PATH="$(cat .engine-path)"`. Conflict surface is TIGHT: the change
is confined to `WriteSceneUniforms` (the three getters block + the per-light clamp
lines + the grey-key line) — disjoint from `DrawMesh`, `DrawParticles`,
`fs_postproc`, and `standard_wgsl.inc` lit-compose, which is where prior
render-polish engine work lived.

---

## Match-neutrality

Engine-only (`src/platform`, `src/gfx`) — NOT compiled into the Wii DOL. No
`src/band3` / `src/system` edits. The only rb3 commit is the `MILO_ENGINE_PIN`
bump (`native/CMakeLists.txt`). **Wii byte-identical by construction** — confirm by
checking the rb3 commit diff is `native/CMakeLists.txt`-only and that
`build/SZBE69_B8/report.json` overall stays `81.86505` (or regenerate and compare).
No decomp source is touched, so no objdiff needed beyond the report sanity check.

---

## VERIFICATION plan (per symptom — for implementer AND reviewer)

Tooling (durable, reconstructed this session under `/tmp/rp8-lighting-polish/`):
- **`_framepin_capture.py`** (`scripts/native/`) — deterministic frame-pinned
  capture. `--screen {main_hub,song_select,game} --pin-frame N`. Because both
  builds run identical engine logic, "frame N on screen X" is identical content →
  valid strict A/B. THIS is the authoritative gate (single-binary, env A/B; or two
  binaries for a true pre-fix control).
- **`/tmp/rp8-lighting-polish/measure.py`** — 3×3-cell luminance contrast + dark/
  bright cells, mlum, soft/hard-green%, clipW%, crush%. Verified to read the retail
  hub ref at 10.53:1 / dark 0.034 (matches the documented baseline).
- **A/B method:** single FIX binary, default vs full-revert env
  `RB3_VENUE_POINT_EXPOSURE=1 RB3_VENUE_DIR_EXPOSURE=1` (proves the knob is the sole
  driver, no rebuild). For an extra-rigorous gate the reviewer may also build a true
  pre-fix binary (revert the diff) — not required since exposure=1 is exact identity.
- Reference: `images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`.
- Ports: use YOUR assigned range. Kill ONLY your own PIDs (check
  `/proc/<pid>/environ` `RB3_HTTP_PORT`); NEVER `pkill -f rb3-native` broadly.
- Evidence under `/tmp/rp8-<key>/`, lean, delete big PNG dumps after.

### (2) Venue song-start exposure — the PRIMARY gate

PASS = the song-start reveal reads bright-but-readable/saturated like retail, NOT a
flat pink/white field; steady-state venue NOT dimmed; soft-clip no longer doing the
heavy lifting.

1. **Reveal hotness (gameplay first frames).** Capture a charted song (small_club:
   e.g. 20thcenturyboy / 25or6to4 / antibodies) at several early game-screen pins
   (e.g. `--pin-frame 2,5,8,20`) on FIX-default and on full-revert env. Run the
   raw lit-path with `RB3_PP_OFF=1` BOTH ways too (the composite hides the
   improvement — the raw path is where you SEE the exposure win).
   - PASS: FIX-default `RB3_PP_OFF=1` reveal frame mlum drops materially toward the
     ~0.30–0.45 band (from the ~0.67 measured this session) and the pink-flood
     `dark`-cell drops (was 0.42 — nothing dark); no full-frame pink wash.
   - PASS: with the composite ON (default), peak `clipW%` stays ≈0 (the soft-clip
     still backstops) AND the frame is visibly less pink/more contrasty than revert.
2. **Steady-state NOT dimmed.** Capture a settled gameplay venue frame
   (`--pin-frame 90`) FIX vs revert. PASS: lit zones still clearly lit (band
   members, stage, gems bright); mlum drop is modest (≤~15–20%) and concentrated in
   the over-bright pink zones, not a global crush. Highway/gems/HUD (game.cam,
   ungraded) byte-identical.
3. **Visual A/B (decisive).** Side-by-side FIX vs revert at a matched pin frame on
   small_club: the pink wash should read as a *saturated club room with form* (walls/
   crowd/structure readable) rather than a flat magenta field. Compare against any
   retail small_club gameplay shot if available (request one if not — see below).

### (1) Menu hub contrast — SECONDARY gate (in-scope = the point-light trim only)

PASS = contrast moves UP from the shared exposure scale (point-light mid-bleed
darkens) without crushing the band/figures; floors NOT re-cut.

1. 50-frame (or pinned-frame loop) menu hub, FIX-default vs revert, `measure.py`.
   - PASS: median 3×3 contrast rises vs revert (target: meaningfully toward 10:1;
     accept any clean rise — the bright-side gap to ~10:1 is explicitly deferred);
     dark-cell drops toward retail 0.034; mlum drops toward retail 0.190; soft-green
     stays in the 2.6–3.8% band (no fog regression); hard-green ~0% (no neon slab).
   - PASS (no-crush): band close-up frame — figures/outfits still readable, not
     silhouetted to black; dark-cell min ≥ ~0.02 (retail reaches 0.034; do not go
     below).
2. Visual vs `yt_mhKNp9uAT48_menu_hub.png`: backdrop brick darker/warmer, neon pops.
   NOTE this is an animated camera loop — match camera phase before comparing, and
   weight the *backdrop brightness* trend over absolute framing.

### (3) Endgame backdrop tint — TASTE gate (optional)

PASS-if-attempted = the green disco peak is softer, faithful color-wheel preserved.
1. Drive to the endgame/score screen (`song-end-test.py --require-endgame` /
   `scoring-test.py` to a natural EOF). Capture across the color-wheel period
   (several frames). PASS: green phase peak less punchy than revert; pink/yellow
   phases still present (wheel intact); crowd still lit (not crushed). If item-2's
   exposure scale already softens it acceptably, ship nothing extra and note it.

### CRITICAL no-regression sweep (ALL implementers + reviewer)

A full menu hub → song select → gameplay (small_club + ONE other venue, e.g. a
street/rooftop song) → score screen pass, FIX-default. Confirm NONE get
darker/washed beyond intent:
- **song_select backdrop** — it draws under world.cam (2805 calls!) so the venue
  exposure scale touches it. Verified this session the backdrop is already near-black
  + UI is prelit (bypasses the lit path); crush% 0.19. PASS = song_select visually
  unchanged (UI chrome + album-art box + song list all clean; backdrop not crushed
  to a different black). `measure.py` mlum within boot-to-boot noise of revert.
- **score screen** — also world.cam backdrop. PASS = per-player score/star widget +
  band readable, not crushed; reaches `coop_endgame_screen` stable.
- **second gameplay venue** (non-club, e.g. street/rooftop) — PASS = lit, not
  crushed, not blown; the exposure scale is a uniform improvement, not a club-only
  hack.
- **game.cam highway** — byte-identical (the exposure scale is world.cam-only; do
  NOT let it leak into the game.cam / sTrackLight path).
- 0 crashes / NaN / asserts across the sweep; `interactionsOk` per the campaign bar.

---

## Decisive scope summary

- **(2) — IMPLEMENT FULLY.** Add `RB3_VENUE_POINT_EXPOSURE` (~0.7) +
  `RB3_VENUE_DIR_EXPOSURE` (~0.8), scale the venue per-light colors, tune by A/B.
  Highest value, well-isolated, env-revertible, Wii byte-identical.
- **(1) — IMPLEMENT the shared point-light trim part; DEFER the bright-side gap.**
  The ue=1 floor lever is exhausted (proven); the in-scope win comes from the item-2
  exposure scale also dimming menu point-light mid-bleed + the grey-key trim. Do NOT
  re-cut the floors and do NOT touch emissive (out of scope).
- **(3) — RIDES (2) for free; add a green trim ONLY if a quick A/B demands it.**

If the implementer finds the per-channel color scale insufficient to tame the
reveal at a sane exposure (>0.5), escalate to the shader-uniform exposure fallback
(repurpose a `SceneUniforms` `_pad` slot, keep sizeof==656) — but try the
no-layout-change CPU scale first.

## REFERENCE SCREENSHOTS NEEDED
- A retail **small_club gameplay** frame during the song-start reveal (to gate the
  exact target hotness of item 2). The hub ref exists; a club gameplay ref does not.
  `../xenia` can capture ground-truth if needed. Not blocking — the "readable not
  washed" + "not dimmed vs revert" gates are sufficient without it, but it would
  make item-2 tuning precise.
