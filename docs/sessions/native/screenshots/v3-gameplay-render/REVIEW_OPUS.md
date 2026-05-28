# REVIEW_OPUS.md — independent subjective visual review (v3, post-render-fix)

Reviewer: Claude Opus 4.7 (1M ctx)
Reference: 11 rb3-native screenshots, session `v3-gameplay-render` (2026-05-27,
captured after the textures + camera + alpha-write render overhaul landed).
Question being answered: **"Does this look like the actual Rock Band 3 now?"**

Methodology: I formed independent impressions of all 11 PNGs before reading the
sibling `REVIEW.md` or the prior session's `REVIEW_OPUS.md`. Diff vs the prior
review is appended at the end (per protocol).

---

## 01_f0005_01_intro.png — Intro / early boot

**First impression.** The dark "Lonely Roads"-style rooftop diorama is now
clearly visible: a small night-time rooftop stage with a drummer behind a kit,
a flanking PA stack, building facades with windows in lit purple, and a smaller
billboard / poster panel on the lower left that *almost* reads as artwork.
There's a striking pure-red vertical bar pinned to the right edge of the frame
and a top half that's a hard pitch-black slab. The composition feels like a
correctly-textured 3D scene that's been chopped by an aspect-ratio mismatch
or a too-tight camera framing.

**Expected.** RB3's intro / attract sequence: a stylised rooftop city scene at
night (or the splash card immediately before it). Textured buildings, the
silhouette of a band on a rooftop stage, brand purples and pinks throughout.
This frame is *close* to that read.

**Specific anomalies.** (a) The black slab covering the top half is the
giveaway — at retail, the sky continues with city skyline or nightclouds.
This is consistent with the render-fix agent's flag: the camera here is the
default cam (wrong aspect / wrong elevation) before the scene's named cam
selects. (b) The vertical pure-red bar on the right edge is suspicious — it
looks like a missing-texture sentinel quad or an un-cropped background frame
buffer artifact. It does *not* look like a real RB3 scene element. (c) The
left/right band of small purple buildings on the lower edge is well-textured
and reasonably lit; the diorama assets are healthy.

**Severity.** NICE_BUT_WRONG. The diorama is rendering correctly; the
framing and one stray red quad are the bug. Massive improvement from the
prior "mostly red/grey colour planes" reading.

---

## 02_f0025_02_splash.png — Splash

**First impression.** Effectively identical to frame 01 — same rooftop
diorama, same black-top / lit-bottom split, same red bar on the right. There
may be a faint extra glyph near the right side that wasn't in 01, but it's
barely perceptible. The diorama is the same and the camera hasn't budged.

**Expected.** A title / splash card with the "Rock Band 3" wordmark on the
intro scene, possibly with the lit-up logo bar or "PRESS START" call to
action.

**Specific anomalies.** (a) No wordmark, no logo, no "Press Start" text.
This is the most identifiable missing-text moment in the new set. (b) The red
right-edge bar is still here; if it isn't a real element it's persisting across
20 simulated frames. (c) The camera evidently hasn't transitioned between
frames 01 and 02 — either the scene cam isn't being selected, or the title
overlay layer is missing entirely.

**Severity.** COSMETIC for the diorama itself, SHOW_STOPPER for "this is the
splash card" — without any text or logo, it's not identifiable as a splash.

---

## 03_f0050_03_overshell.png — Overshell / press-start

**First impression.** A near-monochrome cloudy/foggy plane fills most of the
frame — large soft black-cloud blobs over a near-white background, with two
diagonal streaks of dense, multicoloured confetti-like specks sweeping
upper-mid and lower-mid (purple, cyan, white pinpoints). There are slivers of
the previous magenta rooftop scene leaking in from the screen edges (top,
left edge, bottom-left corner), like the overshell layer is mid-transition
over a still-rendered intro. A small vertical UI sliver is visible in the
bottom-left corner that looks like an overshell player tile.

**Expected.** RB3's "Press Start" / band-join screen: characters posed on a
darker venue stage, the trademark overshell player tile pulling up from the
bottom, "Press START to join the band" prompt, possibly the song-selection
list pre-populated in the background.

**Specific anomalies.** (a) The white/grey cloudy background looks like a
huge background quad with an alpha noise pattern — possibly a venue smoke /
fog effect being rendered at the wrong scale, or an alpha-channel cutout
applied with inverted alpha. (b) The confetti streaks are striking and
familiar — they look like RB3's particle-system spark sprites for the
"start-of-show" celebration burst, with their *additive blend* now working
(post-alpha-write fix) but the colour key inverted (white background instead
of dark stage). (c) The overshell player tile in the lower-left is positioned
correctly and has a yellow "ready" pip — that's a real RB3 element. (d) The
right edge has a faint set of greyed-out menu text rows visible — text *is*
being rasterised here, just at very low contrast against the white-cloud
plane.

**Severity.** NICE_BUT_WRONG. Several real RB3 elements are appearing
(particles, overshell tile, faint menu text). The mistake is that the
background didn't transition to the dark venue / didn't clear to a dark
colour. Particles correctly drawn against the wrong backdrop.

---

## 04_f0120_04_main_hub.png — Main hub

**First impression.** A real, identifiable Rock Band 3 main-hub frame. On
the right, a large textured "Tig..." (Tiger / tiger billboard) panel with
yellow-diamond marquee border around a swirly red+gold scrollwork frame and
a tiger illustration — this is clearly textured artwork, full-saturation
correct colours, with a real glow on the marquee bulbs. On the left half: a
deep black backdrop dotted with one streetlamp arm-and-bulb pair in
desaturated purple/red, a vertical streetlamp pole, and a horizontal arm
with a second lamp head, all reasonably lit. Lower-left has a horizontal
green/khaki bar (a UI selector) and what looks like a thin black overshell
band along the bottom edge.

**Expected.** RB3's signature main-hub screen: the band on stage backstage,
billboards / posters around them, the menu list of modes (Quickplay, Tour,
Practice...) on the left in red-strip styled cards, the overshell along the
bottom, and the Rock Band 3 logo somewhere upper-left.

**Specific anomalies.** (a) The "TIG" billboard is rendering *beautifully* —
that's an enormous win and the single biggest indication the texture path
works for asset content. The marquee bulbs (yellow diamonds), the
scrollwork, and the tiger painting all read correctly. (b) The streetlamps
are correctly textured (lit purple bulbs against a black sky). (c) The menu
panel on the left is mostly black — the menu items should be a vertical
stack of titled cards, but only a single greenish-yellow selection bar is
visible; the other rows seem to render with text but at very low contrast.
(d) No Rock Band 3 wordmark logo visible at top-left. (e) The composition
implies the camera is too high or the field-of-view is wrong — the right-hand
billboard is occupying nearly the entire right half of the frame; in retail
the framing is wider and the menu panel is more prominent.

**Severity.** COSMETIC — and this is a *great* frame. It is clearly,
recognisably the RB3 main hub. The textured billboard alone proves the
pipeline now ships pixels to the GPU. The remaining issues are framing, menu
text contrast, and a missing logo.

---

## 05_f0200_05_quickplay.png — Quickplay

**First impression.** Visually almost identical to frame 04 — the same tiger
billboard, the same streetlamps, the same selection bar on the left. The
file is byte-distinct from 04 (different MD5), so the renderer *is* producing
new pixels here, but the visible delta is small. The menu-selection bar may
have shifted, or the highlighted menu item may have changed; in screen-scale
terms the change is below my eye's threshold.

**Expected.** A different sub-menu (Solo / Band / Online Quickplay tiles)
overlaying the same hub backdrop, with characters posed on the stage.

**Specific anomalies.** Same as frame 04, plus: the Quickplay sub-menu's
content (the mode-tile list, the character poses, the secondary overshell
state) is not visually distinguishable from the main hub. Either the sub-menu
overlay is rendering at very low contrast/opacity, or the menu state actually
transitioned but the visible rendering doesn't reflect it.

**Severity.** COSMETIC — same as 04. The framing of "looks like RB3" is
intact, but the screen-to-screen state delta isn't reading.

---

## 06_f0280_06_song_select.png — Song select

**First impression.** Bright steel-blue and gold scene with a horizontal
gold-glow bar across the middle — this is *immediately* the song-select
screen. The blue/silver curving panels in the upper portion are the song-list
container; the gold horizontal band is the highlighted song row. There's a
tiny scrap of text mid-frame on the highlighted row, and below the gold bar
are three black tiles with faint text on them — the "Score: --" /
"Difficulty: --" / "[Last Played]" cells that hang under a selected song.
The visible resolution is lower than the other screenshots (the file is
small / scaled).

**Expected.** RB3's Music Library screen: a vertical scrollable list of
songs with album art thumbnails, the highlighted song's row glowing gold/cream,
song metadata stats (instrument scores, difficulty stars) below, and a
character idling somewhere on the stage.

**Specific anomalies.** (a) The gold highlight bar is *correct*. The blue
panel framing is *correct*. The three black stat tiles below are *correct in
position*. (b) Text inside the highlight bar is rendering but is barely
legible — looks like very low-res text or a font-atlas glyph issue. (c) The
"PLEASE INSERT..." or "PLEASE SELECT..." style text I expect under the stat
tiles is not legible. (d) No album-art thumbnails visible in the list region
— the entries are blank or rendered at near-zero contrast. (e) Resolution is
much lower (looks like ~320x180 region) than frames 01–05; this is the same
in 07. Possibly a render-target / scale issue specific to this scene.

**Severity.** NICE_BUT_WRONG. The screen is unmistakably song-select — the
biggest "yes, this is RB3" identifier in the set. The remaining work is text
legibility, album-art thumbnails, and resolving the resolution mismatch.

---

## 07_f0360_07_song_picked.png — Song picked

**First impression.** Same blue/silver song-select frame as 06, but with the
gold highlight bar dimmer (more amber/orange) and the upper-right region of
the song list panel now showing some visible content — a darkly textured
panel that might be the highlighted song's album art / "now selected" callout.
The bottom row of three black stat tiles is identical to 06.

**Expected.** Same as 06 but with the chosen song's metadata populated:
album art appears prominently, the song name and artist appear in clear
text, the stat cells fill with the player's prior scores.

**Specific anomalies.** (a) Gold bar dimming is plausibly a "deselect"
animation as a song confirmation happens — that's actually a believable
real-game state transition. (b) Upper-right detail emerging is the right
*location* for "selected song" art but is at the wrong fidelity. (c) Text
across the screen is no more legible than in 06. (d) Same low resolution as
06.

**Severity.** NICE_BUT_WRONG. Same as 06; small but real state delta from
06 → 07 proves the scene is updating.

---

## 08_f0500_08_meta_to_game.png — Meta-to-game transition (gameplay highway)

**First impression.** First view of the gameplay highway. A perspective
3-lane runway emerging from screen-centre at the bottom, leading off into
the black distance — red lane on the left, gold/yellow lane in the centre,
blue lane on the right. Lane stripes / fret lines are clearly visible.
There's a small label/badge at the top of the highway near the vanishing
point that reads "W?ll N?me" or similar (probably "Will Name" — placeholder
text). The rest of the frame is pure black.

**Expected.** Either a loading screen with the song title and album art, or
the very first frame of the in-game venue: the highway visible at the
bottom, the band on stage above it, the venue lighting and crowd in the
background, and the HUD (score, multiplier, star meter, BRE indicator) along
the edges.

**Specific anomalies.** (a) The highway is *rendering correctly* — three
lanes, correct colours (red/yellow/blue for guitar/bass/drum or band
overall), correct vanishing-point perspective, fret lines spaced
appropriately. This is the single biggest "this looks like Rock Band" win
in the set. (b) Everything *above* the highway is black — no venue, no
band, no crowd, no stage lighting. The world isn't loaded yet, the
gameplay scene's environment hasn't been authored, or the venue scene cam
isn't selecting. (c) The "Will Name" placeholder is interesting — looks like
a string-table key wasn't substituted. (d) HUD elements (score, multiplier)
are *not* visible — they should appear by this frame.

**Severity.** NICE_BUT_WRONG. The highway alone makes this immediately
recognisable as Rock Band gameplay. The missing venue and HUD are the
remaining work, but the core "is this RB?" identification works on the
highway alone.

---

## 09_f0700_09_song_loaded.png — Song loaded (~200 frames later)

**First impression.** Byte-identical to frame 08 (same MD5 hash). Same
3-lane highway, same "Will Name" badge, same black void around it. No
visible delta in 200 simulated frames.

**Expected.** Some kind of progression: gameplay HUD appearing, the band
strumming the song intro animation, note gems flowing down the highway,
crowd particles emerging — *any* of these would indicate the song actually
loaded.

**Specific anomalies.** No state change visible at all. Either: (a) the
game is truly stuck at this state (consistent with the documented "stops
on absent .mid asset" issue), (b) the capture loop short-circuited a frame
re-encode, or (c) the song loaded but nothing in the loaded state produces
new pixels yet (no notes streaming, no HUD update, no band animation).

**Severity.** SHOW_STOPPER for gameplay progression, COSMETIC for the
existing highway render (which is still correct from frame 08).

---

## 10_f1100_10_playing.png — Playing

**First impression.** Same highway as 08/09, but now *with* a HUD element
visible above the highway. The HUD shows what looks like a score / multiplier
bracket, but the visible text inside it reads exactly `%d%%` — literal
printf format-string with unsubstituted arguments. This is the "format
string never got its varargs" failure mode in plain pixels.

**Expected.** A real score (e.g., `1247` or similar) and a real multiplier
display (e.g., `×2`), plus note-gems streaming down the lanes, plus star
meter, plus band performance.

**Specific anomalies.** (a) `%d%%` is a printf bug bleeding through to UI.
Whoever produces the HUD score text is calling the formatter without args
or with the wrong vararg pack. The "%%" suggests "score: %d%%" or "Combo:
%d%%" was the intended format. (b) The HUD's *frame* (the rounded-rect
container) is rendering correctly — only the content is wrong. So
RndText / RndQuad pipelines work; only the string source is broken. (c)
Still no note-gems on the highway, no band, no venue. (d) The HUD's
horizontal position (just above the vanishing point of the highway) is
slightly higher than retail RB3's HUD overlay positioning.

**Severity.** COSMETIC for the format-string bug (high-visibility but
trivially fixable), SHOW_STOPPER for the still-missing notes / band /
venue.

---

## 11_f1400_11_pre_exit.png — Pre-exit

**First impression.** Byte-identical to frame 10 (same MD5 hash). Same
highway, same `%d%%` HUD, same black void.

**Expected.** Whatever the last gameplay frame was before shutdown — the
band, the venue, notes streaming, etc. At minimum, *some* progression of
note-gems flowing down the highway between frames 10 and 11.

**Specific anomalies.** No delta from frame 10. The "playing" → "pre-exit"
window is 300 simulated frames and the renderer produces identical pixels.
Either (a) the song isn't truly playing (consistent with absent .mid
blocker), (b) the capture is short-circuiting, or (c) only the highway and
HUD-frame render, and the notes/band layer is fully absent.

**Severity.** Same as 10.

---

## Overall verdict

**Yes — this now looks like Rock Band 3 in roughly half the frames.** The
gap between the prior session and this one is enormous. The previous build
produced two "structurally suggestive" frames (06/07) and nine frames that
were essentially debug-colour rectangles. This build produces:

- **A textured rooftop diorama** (01/02) that's clearly the RB3 intro
  backdrop, modulo a wrong camera and a stray red quad.
- **A textured main hub** (04/05) where the tiger billboard renders so
  cleanly it's the strongest single piece of evidence that the texture
  pipeline is shipping real artwork to the GPU.
- **An unmistakable song-select screen** (06/07) with the gold highlight
  bar, blue/silver panels, and stat-cell layout all positioned correctly.
- **A real Rock Band gameplay highway** (08–11) — three lanes, fret lines,
  correct colours, correct perspective. The highway alone is sufficient to
  identify the game.

What's still broken is concentrated in three areas:

1. **Text rendering / string-substitution.** No legible large text anywhere.
   The `%d%%` HUD literal in frames 10/11 is the smoking gun for at least
   one printf path. The "Will Name" placeholder is a string-table miss.
   Menu items don't read; song titles don't read.
2. **Venue / band / crowd rendering.** The gameplay highway exists but
   floats in a black void. The band-on-stage layer, the venue environment,
   and the crowd are all absent. Frames 08–11 show the highway but
   nothing above it.
3. **Camera / framing.** The intro (01/02) has a wrong camera (black
   slab over top half). The hub (04/05) is framed too tightly on the right
   billboard. These read as the documented "scene cam not selected,
   default cam used" issue.

**Confidence that this is recognisable as Rock Band 3 to a knowledgeable
player:** ~75%. The hub and song-select frames are clearly RB3. The
gameplay-highway frames are clearly Rock Band. A casual observer might
miss the intro and overshell because of the framing / background issues,
but a fan would recognise the diorama instantly.

**Compared to retail RB3, the build now passes the "silhouette test"** —
hand someone a single frame from 04, 06, or 08 and they'll identify the
game. That was emphatically not true of the prior build.

---

## Top 5 remaining visual issues

1. **HUD format-string `%d%%` literal (frames 10/11).** [COSMETIC but
   high-visibility] The scoring/HUD format string is being drawn as the raw
   printf template. Trivially fixable — find the call site, supply the
   right varargs. Most embarrassing single defect in the build because it
   *looks* like a debug build.
2. **Gameplay venue / band / crowd absent (frames 08–11).** [SHOW_STOPPER]
   The highway renders but everything above it is pure black. No band on
   stage, no venue environment, no audience. This is the biggest gap
   between "looks like RB3" and "looks like RB3 *playing*". This is plausibly
   downstream of the absent-.mid blocker (the song-execution loop never
   produces venue draw calls).
3. **Intro camera framing / black top half (frames 01/02).** [NICE_BUT_WRONG]
   The diorama is textured correctly but the camera doesn't seem to be the
   scene's named cam — half the frame is a black slab where the sky should
   be. This matches the render-fix agent's flagged follow-up.
4. **Text legibility (all frames).** [COSMETIC] No large readable text
   anywhere — menu items, song titles, splash wordmarks, "Press Start" cues
   are all absent or rendered too faint to read. Per the render-fix agent's
   follow-up: font texture upload works but the per-glyph UV / quad mesh
   may still be falling back to a placeholder. The frame-02 splash card has
   *no* wordmark; the frame-06 highlight row has near-invisible text.
5. **Stray red bar on the right edge of frames 01 / 02.** [COSMETIC] A
   vertical pure-red band roughly 1/8 the frame width pinned to the right
   side of the intro diorama. Doesn't look like a real RB3 element. Could
   be (a) a debug-colour viewport sliver, (b) an un-cleared previous-frame
   buffer fragment, or (c) a real billboard quad whose texture didn't bind
   and fell through to the missing-texture sentinel.

---

## Surprisingly correct

- **The tiger billboard in the main hub** (frame 04) is the single biggest
  visual victory. Yellow-diamond marquee bulbs, red+gold filigree scrollwork,
  the tiger illustration — all textured with correct colours and reasonable
  shading. This is RB3-quality asset rendering.
- **The gameplay highway** (frame 08+) is correctly drawn: three lanes,
  correct fret-line spacing, correct lane colours (red/yellow/blue),
  correct perspective. If you cropped this to just the highway and showed
  it cold, anyone who's played Rock Band would identify it.
- **The song-select gold highlight bar** (frame 06) is the right colour
  (warm amber), the right vertical position, and the right horizontal
  proportion. The blue/silver panel framing around it is also correct.
- **Particle / additive blending (frame 03)** appears to be working — the
  multicoloured confetti specks streaking across the overshell frame look
  like additive-blend sparks. The alpha-write fix landed.
- **Per-frame state changes are happening.** Frames 04 vs 05 are byte-distinct
  (last session they were identical); 06 vs 07 show a real selection state
  change. The scene-update loop is alive at higher fidelity than before.
- **No catastrophic geometric corruption.** No NaN-blown triangles, no
  z-fighting, no runaway transforms. The vertex pipeline is well-behaved.
- **Texture sampling is real.** The hub billboard, the streetlamps, the
  rooftop diorama buildings, the highway lane stripes — all sampling real
  texture data. The pipeline ships pixels to the GPU now.

---

## Render-fix agent's flagged follow-ups — calibration

The agent flagged three items; I verified each from pixels alone:

1. **HUD format-string substitution shows literal `%d%%`.** **CONFIRMED in
   frames 10 and 11.** Plain-as-day printf template visible in the HUD
   bracket above the highway. Severity: COSMETIC but very high-visibility
   (looks like a debug build).
2. **Intro frame 01 captured at default cam (before scene cam selected).**
   **CONFIRMED.** The black slab covering the top half of frame 01 (and
   the unchanged framing in frame 02) is exactly what "default cam, no
   scene cam bound" produces. Severity: NICE_BUT_WRONG.
3. **Font texture upload working but per-glyph UV / quad-mesh may fall back
   to placeholder.** **CONFIRMED across the set.** Text is consistently
   either absent or rendered at extremely low contrast / fidelity. Frame
   02's missing wordmark, frame 06's barely-legible highlight-row text, and
   the "Will Name" placeholder in frame 08+ all support this. Severity:
   COSMETIC (no single frame's *identity* depends on legible text, but
   every frame is worse for the lack of it).

**One additional independent observation the agent didn't flag:** the
**byte-identity pairs 08==09 and 10==11**. Two pairs of "consecutive"
captures (200–300 simulated frames apart each) produce pixel-identical
output. This is either (a) the gameplay loop genuinely produces no new
pixels (consistent with the .mid blocker), or (b) the capture script is
short-circuiting frame re-encodes. Worth checking the capture loop —
because if (b), we're losing fidelity on actual gameplay progression that
we could be seeing.

---

## Progression since prior review

Read after writing this review: `../song-load-2026-05-27/REVIEW_OPUS.md`
(the prior Opus pass when frames 08–11 were pure red).

### What improved (large wins)

- **Frames 04/05 (main hub).** Prior: *fully white framebuffer, no content
  visible*. Now: textured tiger billboard, streetlamps, menu container,
  selection bar. This is the single biggest improvement in the set — went
  from "looks like the game crashed at the title screen" to "looks like
  RB3's main hub with framing issues."
- **Frames 08–11 (gameplay / load).** Prior: *pure red full-frame, sentinel
  clear, no content for ~900 simulated frames*. Now: textured 3-lane
  gameplay highway with fret lines and correct lane colours. Went from "no
  visible output whatsoever" to "unmistakably Rock Band gameplay
  highway." The clear-to-red bug (or interstitial-quad stuck on screen)
  is gone.
- **Frames 06/07 (song-select).** Prior: *white clear, untextured character,
  faint halftone cells, recognisable layout*. Now: blue/silver panel
  framing, correct gold highlight bar, stat-cell tiles. Layout improved
  from "structurally suggestive" to "obviously song-select."
- **Texture pipeline.** Prior: *RndTex::SyncBitmap a no-op; one halftone
  cell was the only evidence anything sampled a texture*. Now: real
  textured artwork (tiger billboard, rooftop buildings, lane stripes,
  streetlamp bulbs) across multiple frames. The pipeline now ships pixels.
- **Clear-colour issues resolved.** Prior: *frames 04/05 cleared to white,
  frames 08–11 cleared to red, frame 01 had mixed colour planes*. Now:
  frames clear to black (correct), and visible content composites over
  the black backdrop. The alpha-write fix and texture binding fix
  together explain this.
- **Particles / additive blending.** Prior: not visible. Now: the
  multicoloured confetti streaks in frame 03 show additive-blend
  particles working.
- **Frame-to-frame uniqueness.** Prior: 04==05 and 08==09==10==11 byte-
  identical (4 of 11 frames were duplicates). Now: only 08==09 and
  10==11 (2 pairs, less ground lost to dupes); the song-select and hub
  frames show real per-frame deltas.

### What's still broken

- **Text rendering.** Prior diagnosis: "white-on-white text" / font-atlas
  zebra-stripes. Now diagnosis: font texture uploads but per-glyph UVs
  fall back. Different root cause, same symptom — text is illegible. The
  zebra-stripe artifact (prior frame 02) is gone, replaced by "text just
  isn't there." That's actually progress (no broken UV pattern) but the
  outcome to a player is the same.
- **Venue / band / crowd in gameplay.** Prior: gameplay frames showed
  nothing at all (red void). Now: highway renders, everything else is a
  black void. The highway-only state is genuine progress (we can see the
  game's *type*), but the venue scene is still absent.
- **Camera framing.** Prior: didn't really come up because nothing was
  rendering; the intro frame had mixed colour planes that I read as a
  clear-colour issue. Now: the diorama is textured and visible, and the
  camera issue is plainly the black-top-half framing. Same underlying
  bug (default cam used instead of scene cam), now more visible because
  the rest of the render works.

### New regressions (relative to prior session)

None visible. Every frame improved or stayed equivalent. No frame is
worse than its prior-session counterpart.

### Severity grade-shift summary

| Frame | Prior severity | New severity | Δ |
|---|---|---|---|
| 01 intro | SHOW_STOPPER | NICE_BUT_WRONG | Big improvement |
| 02 splash | SHOW_STOPPER | COSMETIC + SHOW_STOPPER (missing text only) | Big improvement |
| 03 overshell | SHOW_STOPPER | NICE_BUT_WRONG | Big improvement |
| 04 hub | SHOW_STOPPER | COSMETIC | Massive improvement |
| 05 quickplay | SHOW_STOPPER | COSMETIC | Massive improvement |
| 06 song-select | SHOW_STOPPER (cosmetic) | NICE_BUT_WRONG | Improvement |
| 07 song-picked | SHOW_STOPPER (cosmetic) | NICE_BUT_WRONG | Improvement |
| 08 meta→game | SHOW_STOPPER | NICE_BUT_WRONG | Massive improvement |
| 09 song-loaded | SHOW_STOPPER | SHOW_STOPPER (no progression) / NICE_BUT_WRONG (visuals) | Improvement |
| 10 playing | SHOW_STOPPER | COSMETIC + SHOW_STOPPER (no notes/band) | Massive improvement |
| 11 pre-exit | SHOW_STOPPER | COSMETIC + SHOW_STOPPER (no notes/band) | Massive improvement |

### Bottom line

The render-overhaul session moved the build from "skeletal — scene graph
exists but assets don't bind" to "convincing — scene graph + textures
both wired, with text and venue/band as the next big gaps." A first-time
viewer who saw both sets cold would identify the new set as Rock Band 3
in 4–5 frames out of 11; they would not have identified the prior set in
any single frame. That is the right kind of progression.
