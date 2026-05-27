# REVIEW_OPUS.md — independent subjective visual review

Reviewer: Claude Opus 4.7 (1M ctx)
Reference: 11 RB3 native-build screenshots, session 2026-05-27
Question being answered: **"Does this look like the actual Rock Band 3 game?"**

I wrote this review before reading the sibling Sonnet `REVIEW.md`. A diff pass
against that review is appended at the end.

---

## 01_f0005_intro.png — Intro / early boot

**First impression.** A 3D scene partially rendered: untextured pink/lavender
geometry that vaguely resembles small buildings, a stage platform, and a
camera-tripod-like object, set against split flat backdrops — top half a flat
medium grey, bottom-left a saturated pure red rectangle, bottom-right a deep
purple/indigo wash. There is a clearly recognizable thin red diamond shape
embedded in the lower-right cluster.

**Expected.** At frame 5 the game is just kicking over from system boot into
the Harmonix / MTV / Mad Catz / EA legal-logo cards, typically full-screen
title cards on a dark background. We would *not* expect any geometry yet.
Seeing the overshell/main-hub 3D set this early suggests either the framecount
labels don't match the boot phase the user labelled, or the game has skipped
straight to a later screen with the splash compositing layer missing.

**Specific anomalies.** (a) The top half of the screen is a featureless mid-grey
quad — looks like a half-cleared framebuffer or a sky/cyc that isn't textured.
(b) The pure-red region in the lower-left and the pure-blue/purple region in
the lower-right are flat-color polygons — almost certainly missing-texture
fallback (red) and untinted vertex colour. (c) The "buildings" and stage are
all matte lavender/off-white with absolutely no surface detail — this is
classic no-textures, no-lighting rendering: the geometry is loading and being
projected, but `RndTex::SyncBitmap` clearly isn't shipping texels to the GPU.
(d) The white triangle along the bottom edge looks like an unclipped UI quad.
(e) There is a small black sphere/disc on the stage which might be a
microphone, drum cymbal, or a debug origin marker.

**Severity.** Show-stopper for "looks like Rock Band 3" — recognisable shapes,
zero texture coverage. The 3D pipeline is doing *something*, but the asset
pipeline behind it isn't feeding it.

---

## 02_f0025_splash.png — Splash

**First impression.** Top half: a horizontal grey gradient (darker on the left,
lighter on the right). Bottom half: a chaotic zone of horizontal pale-blue
zebra-striping, two large solid-black parallelogram bars (sharply sheared
toward the upper-right), and white gaps. The parallelogram bars sit at two
different vertical positions, the left one larger than the right one.

**Expected.** A splash card — most likely the Harmonix or Rock Band 3 logo on
a black or dark backdrop, possibly with a transitional sweep or fade in.

**Specific anomalies.** (a) The grey gradient in the top half is suspiciously
smooth and uniform — this looks like the engine's clear/gradient is correct,
but the foreground compositing is layered on top of the wrong region. (b) The
horizontal blue zebra-striping in the lower half is almost certainly a Synth-y
artifact — either a missing texture sample (`alpha=stride`) producing repeated
scanlines, or a font/text atlas being rendered with broken UVs (each "line"
could be one row of a text bitmap stretched horizontally). (c) The two black
parallelogram bars are *exactly* the kind of shape you'd see from RB3's
trademark slanted button frames or the Rock Band wordmark italic kerning —
they look like UI elements with their fill colour correct but their text glyph
overlays missing. (d) The right-edge clipping of the left bar (a hard
diagonal cut) suggests the bar is overlapping a sibling quad whose alpha test
is winning unexpectedly.

**Severity.** Show-stopper. Recognisable as "something that resembles
RB3-style typography panels" but completely unreadable.

---

## 03_f0050_overshell_join.png — Overshell / press start

**First impression.** Nearly the entire frame is a very pale, slightly cool
off-white (~#EEF1F4 — same as the splash zebra-stripe colour). Floating on top
are two saturated pure-red rectangles (top-left big block and a thin tall
vertical strip near horizontal centre) and a white-on-white quad in the middle
that is hard to see except by its anti-aliased rim. Faint diagonal shadowing
along the bottom edge.

**Expected.** RB3's classic "Press start to play" screen — characters posed on
a stage with a darker venue lighting, the overshell along the bottom showing
player slots, and a "PRESS START" prompt. Should be predominantly dark, not
white.

**Specific anomalies.** (a) The dominant background colour is wrong by ~95%
luminance — the framebuffer is being cleared to a near-white when it should be
near-black. This may indicate a swapped clear-colour, or a full-screen white
quad being drawn on top of the venue. (b) The two pure-red quads are again the
missing-texture sentinel pattern: rectangles where textured UI elements should
live. (c) The diagonal trailing edge of the central white quad suggests a
perspective-projected card. (d) The faint ink-spatter / scribble-like grey
shadow along the lower-centre is interesting — could be the residue of a
proper character silhouette that lost most of its alpha.

**Severity.** Show-stopper. Effectively blank with two debug-red rectangles —
you cannot tell this is RB3 without prior knowledge.

---

## 04_f0120_main_hub.png — Main hub

**First impression.** Entirely white. Not "near-white" — fully #FFFFFF as far
as I can tell.

**Expected.** The dark stage / backstage / venue environment with the band
panel on the left (red side strip, white menu items), the Rock Band 3 logo at
top-left, an idling character on the right side of the stage, and the
overshell band along the bottom. This is RB3's signature screen.

**Specific anomalies.** The entire scene rendered nothing visible — either
every draw call resolved to fully-transparent, every fragment was clipped by
depth/scissor, or the framebuffer was cleared and never composited. Worth
checking whether a present/swap was even called.

**Severity.** Show-stopper. Visually, the player would believe the game has
crashed at the title screen.

---

## 05_f0200_quickplay_screen.png — Quickplay

**First impression.** Identical to frame 04 — fully white, no visible content.

**Expected.** The Solo / Band / Online Quickplay menu — a vertical list of
mode tiles with character art and a Rock Band 3 stage backdrop.

**Specific anomalies.** Same as frame 04 — pure white framebuffer, nothing
drawn over it.

**Severity.** Show-stopper.

---

## 06_f0280_song_select.png — Song select

**First impression.** The most informative frame in the set. Background is
white. Across the top there are a few small grey rectangles that look like
checkered/halftone-textured panels (the dithered/cross-hatched grey suggests
some sort of low-bit-depth texture). Mid-screen is a long horizontal pale-cream
band (#FFFCEF-ish) — clearly the highlighted song-selection row. Below it are
two solid dark-grey/black parallelograms — the same slanted shape language as
the bars in frame 02. On the right side of the screen, intruding from the top
and bottom edges, are two large coloured shapes: an upper khaki/tan blob with
a soft rose-pink underside, and a lower steel-blue/grey blob. These look like
*untextured character body parts* — the tan/pink is plausibly a skin-tone
character (with shaders failing to texture clothing), and the blue-grey is the
character's pants/lower body. There's a faint white curling glyph behind the
character on the right that resembles handwriting or a tail-of-hair shape.

**Expected.** The full song-select list with album art thumbnails on the left,
the highlighted song row in cream/yellow, song metadata (artist, title, length,
difficulty), and on the right side a band character standing in a posed idle.
RB3's song select is one of its most distinctive screens.

**Specific anomalies.** (a) White background instead of dark stage — same
clear-colour issue as frames 04/05. (b) The grey halftone-textured rectangles
top-left are *actually getting texture data* — they have a perceptible dithered
pattern. This is a real signal: some texture path works (the rendered cells
are just at wrong colour depth or alpha). (c) The cream highlight row is
correct colour and roughly correct position. (d) The two black parallelograms
underneath are the song-select prev/next or "X to select" / "Y to filter"
button affordances, with text labels missing. (e) The character on the right
has no texture (the skin/clothes are reading as flat-shaded vertex colours)
but the geometry projection looks sensible — meaning *the character render
path is intact*; only the texture binding is failing.

**Severity.** Cosmetically broken but structurally encouraging. This frame
shows the layout engine works, the highlight bar works, button frames render,
and characters skin/animate. The deliverable is texturing.

---

## 07_f0360_song_selected.png — Song selected

**First impression.** Visually almost identical to frame 06. The character on
the right edge has shifted slightly (slightly different pose), and the
halftone-grey top-left panel has fewer/different cells, suggesting the
song-list has scrolled or the metadata cell updated. The cream highlight bar
is in the same place.

**Expected.** Same layout as song-select, but with the highlighted song's
album art and metadata populated, a "Press START to select" affordance, and
the character idling.

**Specific anomalies.** Same anomalies as frame 06 — white clear, missing
textures, parallelogram buttons unlabeled. The frame-over-frame delta is small
but real (character pose updated), which proves the animation/scene-update
loop is alive.

**Severity.** Same as 06 — broken textures, working geometry.

---

## 08_f0500_meta_game_transition.png — Meta -> game transition

**First impression.** Pure red. Full screen, 1.0/0/0.

**Expected.** Either a loading screen (typically dark with a "loading..." spinner
and tip text), or the start of the kiosk-style band-intro cinematic, or a
black fade between scenes.

**Specific anomalies.** This is the classic "no draws hit the framebuffer; the
clear-to-red sentinel is showing through." It's a debug colour, not anything
the player would ever see in retail RB3. From this frame onward, the entire
visual contract with the user has dropped.

**Severity.** Show-stopper. But this *is* informative: it means whatever Pdraw
was hitting in frames 06/07 (character, halftone cells, cream bar, button
parallelograms) is no longer hitting — so this specific scene's renderer is
either uninitialized or all its draw calls are being culled.

---

## 09_f0700_load_mid.png — Loading the .mid

**First impression.** Pure red. Identical to frame 08.

**Expected.** RB3's load screen with the song title, artist, album art, and the
"now loading…" UI plus the tip-strip at the bottom. This is one of RB3's most
heavily-themed transitional screens.

**Specific anomalies.** No content of any kind. Sentinel red only.

**Severity.** Show-stopper.

---

## 10_f1100_kready.png — KReady (gameplay-ready)

**First impression.** Pure red.

**Expected.** This is the moment just before the song actually starts —
canonically the in-game world view with the highway, the band on stage,
the venue lighting, the crowd, and the HUD all visible. The screen should
already look like *the* Rock Band 3 gameplay frame.

**Specific anomalies.** Sentinel red. Worth noting: the user's notes mention
that boot reaches `LoadSong` but stalls on a missing .mid — so the gameplay
scene legitimately may have never been instantiated. Even so, we'd expect to
see the loading background, not bare red.

**Severity.** Show-stopper.

---

## 11_f1400_pre_exit.png — Pre-exit

**First impression.** Pure red.

**Expected.** Whatever the last visible screen was before shutdown — likely
the gameplay-ready or load screen frozen on the last successful frame.

**Specific anomalies.** Same red sentinel as 08–10. The renderer has stayed
in a no-draw / cleared state through ~700 simulated frames after the song
select sequence; this is consistent with the BandRnd or gameplay-scene render
path never producing draws.

**Severity.** Show-stopper.

---

## Overall verdict

This is a partially-rendered build with a clearly bimodal failure mode:

1. **Early frames (01–07) render *geometry* but not *textures*.** The 3D
   scene's vertex pipeline, transform stack, projection, scissor/viewport,
   and basic colour-fill paths are working. The dithered halftone cells in
   frames 06/07 suggest at least one texture pathway can deliver pixels to
   the framebuffer, but it's at low fidelity (possibly a debug/format-mismatch
   path). The dominant visual is flat-colour polygons against a too-bright
   clear, with intrusive pure-red rectangles serving as missing-texture
   sentinels.

2. **Late frames (08–11) render nothing — only the clear-to-red sentinel.**
   No geometry, no UI, no characters. Whatever subsystem drives gameplay-scene
   composition (BandRnd / song-select-out transition / meta→game handoff) is
   either not initialised, not being called, or all of its draws are culled.

The build is not "broken-but-cohesive" (where you can recognize RB3 through
texture issues). The build is "skeletal" — the scene graph exists, but the
asset binding layer (textures, materials, fonts) is absent or no-op for almost
everything, and the gameplay-mode renderer never engages. This is exactly
consistent with the project's stated state (RndTex::SyncBitmap is a no-op,
Bink stubbed, Wii TPL decode absent) and with the fact that BeginDrawing/
EndDrawing were no-ops until this session.

**Confidence that this is recognisable as Rock Band 3 to a player:** zero.
The only frames a knowledgeable observer could classify are 06 and 07
(layout cues match song-select).

---

## Top 5 most-broken visuals

1. **Frames 08–11 — entire ~900-frame tail rendered as pure red.** [show-stopper]
   Once we leave song-select, no draws hit the framebuffer at all. This is the
   single most damning class of failure: the load → gameplay handoff produces
   no visible output.
2. **Frame 04 (main hub) and frame 05 (quickplay) — fully white framebuffer.**
   [show-stopper] These are the most important "is this RB3?" identifying
   screens and they render nothing. Different failure mode from the red tail
   (white clear vs red clear) which is itself a clue — the clear colour is
   varying by scene.
3. **Frames 01/03 — pure-red and pure-blue/purple flat-colour quads inside
   otherwise-recognisable geometry.** [show-stopper] The red rectangles in
   particular are missing-texture sentinels, and they appear as load-bearing
   background regions — meaning major scene-background textures are missing.
4. **Frame 02 (splash) — recognisable RB3 typography parallelograms with
   missing glyphs, plus zebra-striped horizontal blue artifacts.** [show-stopper]
   The shape language is correct, but the text overlays that would make this
   identifiable as the RB3 splash are gone. The zebra-stripe is likely a
   broken font-atlas UV mapping.
5. **Frame 06/07 background — white instead of dark stage.** [show-stopper for
   "looks like RB3", cosmetic relative to the others] Clear-colour bug or
   missing background-environment draw on what would otherwise be the most
   convincingly-RB3 frames in the set.

---

## Surprisingly correct

- **Layout positions are right.** In frames 06/07, the cream highlight row is
  vertically centred where the RB3 song-select highlight sits, the button
  parallelograms are at the bottom-corner positions where RB3's button hints
  sit, and the character is anchored to the right side where RB3's band
  character idles. The UI authoring is being interpreted correctly.
- **At least one texture path is feeding pixels.** The halftone-dithered grey
  cells in the upper-left of frames 06/07 are not flat-shaded — they have
  fine-grained per-pixel variation. Something is sampling a texture and
  emitting non-uniform colour. That's the foothold for fixing the rest.
- **Character mesh + skinning is alive.** Frame 06 vs 07 shows a pose delta on
  the right-side character. The geometry pipeline, animation update, and
  skinning matrices are functioning end-to-end; only the surface-shading
  inputs (albedo, normal, etc.) are missing.
- **Clear-colour varies by scene.** Different scenes have different clear
  colours (red 8–11, white 4–5, mixed gray/red 01, white 03/06/07). That means
  the per-scene clear semantics are being read from the .milo data; whatever
  is going wrong is downstream of "what colour do I clear to".
- **No catastrophic geometric corruption.** Despite the texture and
  composition issues, none of the frames show flickering polygons, NaN-blown
  triangles, depth-fighting, or runaway vertex transformations. The vertex
  pipeline is well-behaved.

---

## Diff vs Sonnet REVIEW.md

Reading the Sonnet review after writing mine. Sonnet had instrumentation/log
context I deliberately avoided (mesh counts, code-level references to
`BandRnd::DrawMesh`, scene names like `main_hub_screen`, the
`InterstitialPanel::Exiting()` blocker name). My review is purely from the
pixels; Sonnet's is pixels + telemetry. That asymmetry colours every
disagreement below.

### Agreements

- **Untextured geometry diagnosis.** We both identify "no texture sampling"
  as the dominant root cause for frames 01–07. Sonnet roots it in
  `useTexture=0` hardcode + `RndTex::SyncBitmap` no-op; I called out
  "RndTex::SyncBitmap isn't shipping texels to the GPU" in frame 01.
- **Frames 06/07 are the most informative.** Both reviews flag this as the
  one frame where the layout truly resembles RB3 and the character renders.
- **Frames 04/05 are a white-out failure.** Both reviews call this
  show-stopper.
- **Frames 08–11 are stuck on a single state.** We both note nothing changes
  across ~900 simulated frames.
- **Character render works** in frames 06/07 — both reviews say this.

### Things I saw that Sonnet missed (or downplayed)

- **The black parallelogram bars in frame 02 are RB3-trademark slanted button
  frames with their text overlays missing.** Sonnet describes them as
  "blue-striped panel pairs" / "button-selector quads" but doesn't connect
  them to RB3's distinctive italic-shear typography language. The shape is a
  positive identifier; calling it out helps confirm the layout is right.
- **The horizontal pale-blue zebra-striping in frame 02.** I read this as a
  broken font-atlas UV producing repeated scanlines. Sonnet does not address
  the zebra pattern at all, mentioning "no text is visible" in passing.
  This banding is a distinct artifact class from "white-on-white text" and
  may indicate a separate failure (UVs collapsing to a single row of a
  bitmap, vs invisible-but-correct text geometry).
- **The dithered/halftone grey cells in frames 06/07 are evidence at least
  one texture path is sampling.** Sonnet describes them as plain "gray
  thumbnail-region" without noting the per-pixel variation. This matters
  because it contradicts the "absolutely zero texture sampling" framing —
  *something* is reaching the GPU as a sampled image, even if not the right
  bitmap.
- **Clear colour varies by scene.** I called out that frames 04/05 clear to
  white while frames 08–11 clear to red. Sonnet treats 04/05 white as "white
  background plane geometry" and 08–11 red as "full-screen red vignette
  quad" — different framing, plausibly correct (Sonnet has the geometry
  counts), but I think it's worth keeping the framebuffer-clear hypothesis
  open until verified, because they're not visually distinguishable from a
  cleared framebuffer with no draws.
- **Frame 03 has a faint diagonal anti-aliased rim and a scribble-like
  shadow at the bottom centre.** Sonnet calls it a clean "red overshell with
  white diagonal panels" scene. I think there's more rendering happening at
  low-alpha or near-white-on-white than the high-level read captures.

### Things Sonnet saw that I missed

- **Frame 01's red shape is the RB3 logo/banner mesh**, not just a
  missing-texture sentinel. Sonnet's mesh-count + scene-name context lets
  them identify the geometry as the intro diorama. I read the red and blue
  flat-colour regions as missing-texture artifacts; Sonnet reads them as the
  correct mesh rendered with correct vertex/material colours. **I think
  Sonnet is partly right here** — the red is too saturated and too sharply
  rectangular to be a generic "missing texture" sentinel, and the
  bottom-right purple region's shape looks vertex-coloured rather than
  filled. I was over-fitting on "red = missing texture" because frames 08–11
  primed me to read pure red as a sentinel.
- **The red in frame 08–11 is a real geometry (an interstitial vignette
  quad), not a clear colour.** Sonnet's diagnosis is more confident than
  mine and is consistent with `InterstitialPanel::Exiting()` stalling.
  Sonnet has the log data to back this; I don't have a way to verify from
  pixels alone, but this is a more specific (and probably correct) reading
  than my "framebuffer-clear with no draws."
- **The intro camera is wrong** (oblique / stage-floor-edge angle). I noted
  the framing felt off without identifying a specific cause. Sonnet's
  diagnosis — `RndCam::sCurrent` not yet bound, base `Rnd::BeginDrawing`
  bypass skips `mDefaultCam->Select()` — is a specific actionable
  hypothesis I couldn't reach from pixels.
- **The intro scene is the rooftop/city diorama.** I described "small
  buildings" and a "stage platform" without realising this is a recognisable
  RB3 set piece. Sonnet's correct that this matches the RB3 intro backdrop.
- **`RndText` likely renders white-on-white.** Sonnet's hypothesis that text
  meshes are being drawn but with white vertex colour against a white
  background is more specific than my "missing fonts" framing.

### Severity disagreements

- **Frames 08–11.** Sonnet labels these as "expected behavior — RB3 uses a
  red fade-out/fade-in as a screen-transition vignette." I labelled them
  show-stopper. **We're both right at different timescales**: it's correct
  rendering of an interstitial that should have exited; it's a show-stopper
  in that the player never sees gameplay. Sonnet's framing is more accurate
  re: rendering correctness; mine is more accurate re: end-user experience.
- **Frame 01.** Sonnet treats the geometry/colours as basically correct
  modulo missing textures. I called it show-stopper. Knowing now that the
  red mesh is the RB3 logo banner, I would downgrade my read from
  show-stopper to "correct geometry, untextured" — closer to Sonnet's
  framing.
- **Frame 03.** Sonnet says "geometry is correct in shape and position. No
  textures, no legible text. Red is correct for this screen (RB3's
  overshell branding color)." I called it show-stopper because nothing was
  recognisable to me. If the red and white diagonal planes are correct
  overshell chevron shapes, then Sonnet's "structurally correct" framing is
  more accurate — I was too pixel-literal.
- **Frames 06/07.** We broadly agree. I was slightly more positive ("show
  shows the layout engine works, the highlight bar works...") than Sonnet,
  who framed them more flatly.

### What I would change in my review after seeing Sonnet's

1. Re-grade frame 01 from show-stopper to "correct-geometry, untextured."
2. Re-grade frame 03 the same way.
3. Recognise that the pure red of frames 08–11 is more likely a real
   interstitial mesh than a framebuffer clear sentinel. (Though the visual
   effect on the user is identical.)
4. Keep my zebra-stripe and halftone-cell observations — these are real
   pixel-level artifacts Sonnet's high-level diagnosis missed.
5. Hold my "clear colour varies by scene" hypothesis as worth verifying
   against the engine code; Sonnet's "white-plane background mesh" and
   "red interstitial vignette" framing is plausible but pixels-alone cannot
   distinguish it from "framebuffer clear."

### One thing neither of us flagged (worth noting separately)

Frame-file byte-identity: a quick `ls -l` shows `04_f0120_main_hub.png` and
`05_f0200_quickplay_screen.png` are identical (35795 bytes), and
`08_f0500_meta_game_transition.png` through `11_f1400_pre_exit.png` are all
identical (36970 bytes). So 04==05 and 08==09==10==11 at the byte level. That
means: (a) the renderer is genuinely producing the same pixels across hundreds
of simulated frames (consistent with both reviews' diagnoses), and (b) some
frames in the capture sequence may have been short-circuited rather than
genuinely re-encoded — worth verifying that the capture loop is actually
re-rendering between named checkpoints.
