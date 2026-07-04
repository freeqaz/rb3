# C8 impl: over-bright / flat face shading (b) — character-environ real-light shading

Implements the fix proven by `research-c8-shading.md` (§A3-A5 addendum). Scope
item (b) of the C8 follow-ups. Landed 2026-07-02.

TexBlender port (c) was NOT implemented — the research proves it a NO-GO for the
native-vs-Dolphin gap (§A6/§4). See "Deferred" below.

## Commits

- Engine: `5587ce0` (milo-native-engine) — `src/platform/Rnd_Wgpu_RB3.cpp`
- rb3:    `7f603e17` — `native/CMakeLists.txt` (pin bump `04c8e1c` -> `5587ce0`)

## Root cause (confirmed on this build)

`BandRnd::WriteSceneUniforms` (world.cam venue path) iterated **`mLightsApprox`
only** and promoted every approx light to a full Lambert directional/point.

On Wii, `RndEnviron` splits its lights (verified in `src/system/rndobj/Env.cpp`):
- `mLightsApprox` = the "fake" lights. `IsFake()` tests membership in that list;
  `UpdateApproxLighting()` folds them through `BoxMapLighting` into GX **ambient**.
  They never produce Lambert directional shading.
- `mLightsReal` = the GX hardware lights = the actual directional/point shading.

For **character environs** (`chars.env` / `char.env` / `*_char.env`), the approx
set is `rim.lit` + four white `*_silhouette.lit` spots. Promoting those to full
directionals floods band flesh with a flat frontal white key (over-bright,
shadowless). Meanwhile `mLightsReal` — a single dim, distance-attenuated warm key
(`main.lit`) — was **entirely ignored**. Probe on this build (subway/street venue;
the "CORK" sign is a prop):

```
chars.env  mLightsReal  = main.lit  type=0 point color=(0.93,0.69,0.99) range=625 pos=(-138.9,107.4,184.1)   <- the key Wii shades faces with; native ignored it
chars.env  mLightsApprox= rim.lit(dir grey) + rim_underneath(black) + bass/drums/guitar/vocals_silhouette(white pts r40) + main_crowd  <- native flooded faces with these
```

GT (`dolphin-shots/face_guitarist_ambient.png`): faces are dim, lit by a single
directional key with a real shadow side. Native OFF = flat bright orange flesh.

## Fix

In `WriteSceneUniforms`, venue branch only: pick the direct-shading light list.
- If the env name contains `"char"` AND `mLightsReal` has a usable (showing,
  non-black) light -> `useReal = true`: iterate `mLightsReal` for the dir/point
  slots, then demote `mLightsApprox` to a **modest averaged ambient fill** (avg,
  not sum, so N white spots don't re-flatten; clamped low so the real key carries
  the directional shading).
- Otherwise (every non-char / venue-geometry environ, and any char env whose real
  list is empty) -> keep the **legacy approx-promotion untouched**.

This deliberately does NOT flip the venue-geometry environs (`geom.env`,
`theater.env`, `street.env`, ...), so the converge-2026-06-20 backdrop tuning and
the converge STEP 1 arena tight-spot band key do **not** regress (research risk
#3 avoided by construction — the geometry-wide flip is the deferred faithful
follow-up that WOULD need a full re-tune + 5-venue gate).

Scoping verified on this build (RB3_VENUE_PROBE=1, `[CHAR_REAL]` log):
`chars.env / char.env / char_rooftop.env / street_slomo_char.env /
subwayhangout_char.env` -> `useReal=1`; every `*_geom.env / theater / street /
road / sky / cityscape / crowd / logo / train / back_left / buildings_dim` ->
`useReal=0`. Clean split, no venue-geometry env touched.

### Gating (house style)
- Default-ON, full clean revert `RB3_CHAR_REAL_LIGHT_OFF=1` (no rebuild).
- Tuning knobs: `RB3_CHAR_APPROX_AMBIENT` (default 0.11 avg coeff),
  `RB3_CHAR_AMBIENT_MAX` (default 0.14 per-channel clamp).
- Engine change lives in the RB3-only TU `Rnd_Wgpu_RB3.cpp` -> DC3 byte-identical
  (DC3 uses a different render backend). Wii match build untouched (no `src/`
  change; `Env.h`/`Env.cpp` were read, not modified).
- world.cam venue path only -> game.cam (highway) + menu cams byte-identical.
  Menu-hub flesh scopes under `env=''` (isChar=0) -> unaffected, per research.

## Verification (native, band-closeup-capture.py, ALWAYS --anchor-ms)

Random per-launch band rotation is the known A/B trap (memory:
crowd-origin-inststrings). Controlled by capturing at matched `--anchor-ms` (same
LightPreset cue state) across multiple launches and judging shading QUALITY
(directional shadow-side vs flat flood), not one frame.

Harness gate: every ON and OFF run PASS 6/6 pinned, 0 drops (no geometry
regression, venue backdrop unchanged).

Evidence images (beside this doc):
- `shading_faces_18k_on_vs_off.png` — anchor 18000 (non-warm cue), top=ON
  bottom=OFF, L|R members. **Decisive**: OFF floods heads/faces/skin with flat
  bright orange (hair blown out); ON is dim, directional, GT-like with a shadow
  side. Matches `dolphin-shots/face_guitarist_ambient.png`.
- `shading_confirm_18k_3x.png` — 3 ON launches (top) vs 3 OFF (bottom) at anchor
  18000: ON row consistently dim/restrained flesh across rotations; OFF row shows
  the orange flesh flood (bottom-left clearest). (bottom-right = pre-existing
  unrelated magenta full-frame wash, research trap #4 — NOT this fix.)
- `shading_montage_n00.png` — wide front_n00 at anchors 18k/24k/30k, ON row / OFF
  row: OFF shows more warm flesh flooding at the non-warm cues (18k, 30k); 24k is
  a genuine bright warm cue in both (whole venue lit).
- `shading_baseline_front_n01.png` — baseline (pre-fix) reference: right
  guitarist flat hot-orange flesh flood.

Skin-color luma stats were attempted but are unreliable in this warm wood-cabin
venue (warm walls misclassify as skin: one frame reported 1.2M "skin" px) and are
dominated by rotation albedo variance — the visual matched-anchor comparison is
the load-bearing evidence. Band flesh draws under merged-instance names
(FINDINGS §3) so `RB3_ISOLATE_MESH` by head/skin substrings catches only crowd
extras, not the band — mesh isolation was not usable here.

Web: `scripts/web/build.sh --debug` -> WEB_EXIT=0 (shared engine change compiles +
links for WASM).

## Deferred / not done

- **(c) RndTexBlender / normal-map port — NO-GO now** (research §4/§A6). The Wii
  `DrawShowing__13RndTexBlenderFv` is a faithful empty `blr` @100%; the RB3 native
  backend never binds a normal map (binding 3 hard-wired to `mFlatNormalView`),
  and RndMat discards the normal map at load. Porting it changes zero pixels and
  cannot close a native-vs-Dolphin gap. It is a separate optional Xbox-fidelity
  slice (research §5 slice plan) — do the lighting fix first (done), then
  re-evaluate. Not implemented.
- **Geometry-wide `mLightsReal` flip** — the faithful whole-venue version of this
  fix. Deferred: it destabilizes the converge backdrop tuning and needs a full
  `RB3_VENUE_*_EXPOSURE` re-tune + the converge-2026-06-20 5-venue gate. The
  char-scoped slice here is the proven, contained win.
- **Display-gamma midtone lift** (research §2.3) — documentation-only amplifier;
  untouched this pass.
- **Ambient-fold fidelity** — the fold is a flat averaged ambient, not a true
  per-object BoxMap eval (scene uniforms are per-environ, not per-object). Good
  enough for the GT-dim look; a spare-pad 6-face box (converge gating pattern) is
  the later upgrade if wrinkle-side shadow shaping is wanted.

## Notes / residual risks

- Effect verified in one venue (subway/street, `main.lit` warm-lavender key). The
  `useReal` guard means venues whose char env has an empty real list silently keep
  the old behavior — safe by default, but other venues' char keys were not
  individually eyeballed. Spot-check bright venues (festival/arena) later; the
  gate makes any regression a one-env, opt-out-revertable change.
- The `[CHAR_REAL]` fprintf is gated behind `RB3_VENUE_PROBE` (zero cost
  otherwise) and left in as diagnostic infra, matching the existing VENUE_PROBE
  block in this function.
- FxSendNative.cpp (another agent's uncommitted engine edit) was never touched or
  staged.
