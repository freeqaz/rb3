# Opus subjective visual review — V5 (post-V1+V2+V3-render+V4+V5a+V5c+V5d)

Reviewer: Claude Opus 4.7 (1M)
Date: 2026-05-28
Build pin: rb3-native milestone (b) + the full V1..V5 wave
Screenshots: `/home/free/code/milohax/rb3/docs/sessions/native/screenshots/v5-localize-fix/`

This is the fourth subjective review pass. I formed impressions from the PNGs
before reading any prior REVIEW_OPUS — the diff pass at the bottom retro-
cites the earlier ones.

---

## Per-screenshot impressions

### 01 — `01_f0007_01_intro.png` (frame 7, intro)

First impression: a chunk of recognizable RB3 chrome — the dressing-room /
rooftop diorama with a drummer behind a kit and the iconic geometric purple
city architecture — fills the lower-left third of the frame, while the rest
of the screen is hard, true black. Two glassy diamond rank-icons float
along the left edge.

What RB3 should look like here: the very-early "Harmonix presents" /
character-intro vignette runs over a full rooftop scene with HMX branding
overlaid; the rooftop fills the whole 16:9 viewport with sky, neon, and
sometimes silhouetted city skyline.

Anomalies:
- Top half of the frame is a hard black void — no skybox, no environment
  cards, no sky gradient.
- Right half of the frame is also pure black; the diorama is camera-cropped
  or stage geometry is missing.
- A pale-grey / light parallelogram shape sits in the bottom-mid-right —
  feels like an un-textured environment quad bleeding through.
- The diamond rank-icons are at sensible UI positions but seem to be the
  only stable overlay element.

Severity: COSMETIC for the diorama subset (which actually renders very
faithfully); NICE_BUT_WRONG for the absent skybox.

---

### 02 — `02_f0025_02_splash.png` (frame 25, splash)

First impression: still the same rooftop diorama in the lower-left quadrant
but now overlaid with the "Connect Controller" / "Choose Instrument"
welcome-flow chrome. A yellow-banded "NO PROFILE / SIGN IN" pill is in the
sign-in slot. Four "Choose Instrument" panel cards march across the screen
horizontally — slots 1-4, mostly empty as expected on a fresh boot with no
controllers.

What RB3 should look like here: this is the canonical sign-in / instrument-
selection screen. Four controller slots in a row, slot 1 highlighted with
the active profile, slots 2-4 empty stubs. Background is the rooftop
diorama softly blurred or dimmed behind the overlay.

Anomalies:
- "CONNECT CONTROLLER" / "CHOOSE INSTRUMENT" text on slots 2-4 is rendering
  in ghostly pale grey instead of the high-contrast white retail uses —
  feels like font alpha or font-color tinting is off, but text content is
  100% correct (not a token leak).
- Slot 4 (far right, off-screen-ish) is at or past the right edge — frame
  layout is squashed horizontally.
- Same black void in the upper half / right side.
- The "NO PROFILE / SIGN IN" pill is clean and properly localized.

Severity: COSMETIC (font contrast); ACTUALLY_CORRECT (V5d) for the 4-slot
chrome which a prior reviewer flagged as a "ghost stamp".

---

### 03 — `03_f0050_03_overshell.png` (frame 50, overshell)

First impression: this is one of the more atmospheric frames. A dark
magenta-to-black gradient mountain-silhouette dominates the upper two-
thirds; a tab-row of three button stubs sits at the bottom; a yellow
button pill on the far left. A faint horizontal text bar near the top
shows partial-character glyphs ("Mu..." / "....").

What RB3 should look like here: the "overshell" — the persistent always-on
HUD that wraps menus, showing the band-name banner at top and the
breadcrumb / button-hint row at bottom. Retail RB3 has bold legible band-
banner text and crisp button labels.

Anomalies:
- Resolution is markedly lower than other frames — this screenshot looks
  scaled down. That's a capture-pipeline quirk, not the game.
- Top text bar is largely illegible — glyph atlas is shipping but text is
  rendering at sub-pixel scale or the layout is wrong.
- The three bottom tab-stubs have no glyphs, just frame outlines.
- Atmospheric haze in the background is actually closer to RB3's "between-
  scenes" curtain look than I expected — surprisingly evocative.

Severity: COSMETIC.

---

### 04 — `04_f0120_04_main_hub.png` (frame 120, main hub)

First impression: stage-edge geometry is back — a stylized cabaret /
marquee-bulb arc on the right edge (yellow round lights forming a question-
mark / arch shape), what looks like the side of a "TIC" booth or marquee
sign with crumpled-paper texture in the mid-right, a stage-light truss on
the upper-left, and the persistent overshell tab-row at the bottom. A
small yellow pill in the upper-left.

What RB3 should look like here: the main hub is a centered, well-lit stage
"backstage area" with menu tiles (Quickplay / Road Challenges / Music
Library / Career / etc.) front and center. The marquee chrome and stage
lights are part of the diorama but the menu tiles are the primary focus.

Anomalies:
- No menu tiles — the central UI panel is just absent. Should be a vertical
  stack of glowing menu cards.
- The marquee-arch with the bulb-pattern is genuinely on-model — bulbs
  glow, geometry is right, this is very recognizable RB3.
- The "PER" text fragment near the bottom-center mid-right is a clipped
  glyph — likely "PERFORMANCE" or "PERFORM" cropped by a panel.
- Bottom overshell tab text is again ghostly pale.

Severity: SHOW_STOPPER for the missing menu panel (you cannot navigate);
ACTUALLY_CORRECT for the diorama.

---

### 05 — `05_f0200_05_quickplay.png` (frame 200, quickplay)

First impression: nearly identical to the main hub frame, but with a
vertical text stack appearing on the left: "PLAYNOW / QUICKPLAY / START A
ROAD CHALLENGE". The same marquee arch, the same truss, the same overshell.

What RB3 should look like here: classic RB3 menu-card stack — each option
in its own glowing-edged panel, the selected one highlighted with a tinted
overlay and a chevron / glow. Background diorama dimmed.

Anomalies:
- Menu labels are now legible and correctly formatted (no `%s` leaks) —
  the V5c locale fix is clearly doing its job.
- BUT the menu items render as raw text on transparent quads — none of the
  glowing card chrome, none of the selection-highlight tinting, no chevrons.
- "PLAYNOW" and "QUICKPLAY" overlap each other vertically at the top — a
  panel that should have stacked / spaced is collapsing.
- "START A ROAD CHALLENGE" is correctly localized — no token leak.
- Diorama side-pieces still on the same layout (no parallax or camera move
  between hub and quickplay, which is fine for these static caps).

Severity: NICE_BUT_WRONG (text right, chrome absent).

---

### 06 — `06_f0280_06_song_select.png` (frame 280, song select)

First impression: now we're into the Music Library. Top-left says "MUSIC
LIBRARY" in correct dark-on-light retail typography. Top-right says
"SAV..." (presumably "SAVE" or "SAVING"). There's a horizontal row of dark
glassy panels in the middle stripe with rating fractions "0/10", "0/5",
"0/30" right-aligned — these are aggregate completion stats.

But: a giant *grey filled rectangle* sits over the right half of the music-
library scrollbox, completely obscuring whatever song-row content should
be there. It looks like an un-textured / fallback-material UI quad that's
covering the listing.

What RB3 should look like here: a scrollable list of song cards along the
right side with title + artist + difficulty stars, a "now-playing preview"
strip, instrument stat columns on the right, and a coloured selection bar.
Top bar reads "MUSIC LIBRARY" with sort tabs.

Anomalies:
- The grey rectangle (mid-right, covering ~30% of the viewport) is
  catastrophic for usability — you can't see any song titles.
- The "Connect Controller" / "Choose Instrument" overlays from earlier
  frames are still bleeding through at the bottom — they should have been
  dismissed once a controller was assigned.
- "0/10", "0/5", "0/30" are correctly formatted — locale fix working.
- The "SAV..." / "Are y..." text upper-right looks like a "SAVE PROGRESS /
  Are you sure?" modal half-rendering; could be a phantom-dialog ghost.
- "ma_hb_detalls" cropped string near bottom is a token name leak.

Severity: SHOW_STOPPER for the grey-rect occlusion; MINOR for the residual
overshell ghost.

---

### 07 — `07_f0360_07_song_picked.png` (frame 360, song picked)

First impression: post-select detail page. Top reads "%S %I SONGS / SET
LIST (3 SONGS)" — the `%S %I` is unmistakably a printf format-string leak.
Below that, three darkened song-row panels with no song titles. A small
red-on-black "MIGHTY MODE" / "INSTAFAIL" pill on the left, a clean "BASS"
pill, and a "GUITAR" pill.

What RB3 should look like here: the song-confirmation / setlist screen
shows the queued songs by title/artist with a "ready to play" action
button.

Anomalies:
- `%S %I SONGS` is the most glaring text-rendering bug remaining in the
  menu chain — this is the format string itself, not a value. The
  "tour_stars_fraction_fmt"-style leak that V5c was supposed to flush.
  This one slipped through, likely because the substitution layer here
  takes uppercase `%S` (wide-string) rather than `%s`.
- The three song rows render as glass panels but no title text at all —
  song metadata isn't reaching the row template.
- "MIGHTY MODE" / "BASS" / "GUITAR" pills are crisp and on-model.
- "SET LIST (3 SONGS)" is correctly localized.

Severity: COSMETIC (`%S %I`); NICE_BUT_WRONG (empty rows).

---

### 08 — `08_f0500_08_meta_to_game.png` (frame 500, meta→game transition)

First impression: the game has fallen off a cliff. A pure-black field
with a small "%d%%" centered text box (literal format-string), and a tiny
faded "Wii Name" label below it, sitting on top of a narrow three-coloured
slab that flares from a single point at center-screen-bottom outward to
roughly 1/3 viewport width — red on the left, dark olive-yellow in the
middle, blue on the right. The slab IS the highway, but it's rendered as
a flat textureless wedge against pure black.

What RB3 should look like here: a polished song-loading screen with
percentage progress, song art, artist credit, and a hint of the upcoming
venue. Then the highway-rise transition spreads the fretboards across the
full viewport.

Anomalies:
- `%d%%` is the literal HUD score format-string. (Per user note: this is a
  scoring-callback wiring issue, not a locale issue.)
- "Wii Name" is the placeholder player tag — expected on Wii w/o profile.
- The highway is geometrically small (centered, narrow) and entirely flat-
  shaded — no lanes, no fret markers, no notes, no scrolling texture, no
  perspective camera spread.
- Zero environment / venue geometry.
- Aspect ratio of capture jumped from 640x360-ish to 1280x720 between
  frame 7 and frame 8 (resolution doubled mid-session) — suggests the
  rendering target was reconfigured at song-load time.

Severity: SHOW_STOPPER for gameplay rendering.

---

### 09 — `09_f0700_09_song_loaded.png` (frame 700, song loaded)

First impression: visually indistinguishable from frame 08. Same `%d%%`,
same flat tri-colour wedge, same black void. The hash differs slightly
from 08 but the eye can't see it.

What RB3 should look like here: by frame 700 / +200 frames after song-load,
the highway should be scrolling, notes should be visible coming down the
lanes, the venue / band should be animating in the background, and the
HUD should show a real numeric score, multiplier, star meter, and overdrive.

Anomalies:
- Highway is still tiny.
- No notes, no scroll, no animation.
- No band, no venue, no stage lighting, no crowd.
- HUD is a single literal "%d%%" string.

Severity: SHOW_STOPPER.

---

### 10 — `10_f1100_10_playing.png` (frame 1100, mid-song)

Byte-identical to frame 11 (md5 confirmed). Same flat highway, same
`%d%%`, same black void.

What RB3 should look like here: 1100 frames in, mid-song, with notes
flying, crowd reacting, lighting cues firing, score climbing.

Anomalies: gameplay is rendering as a static placeholder. No animation
loop is producing any motion.

Severity: SHOW_STOPPER.

---

### 11 — `11_f1400_11_pre_exit.png` (frame 1400, pre-exit)

Byte-identical to frame 10. Nothing has updated between frame 1100 and
frame 1400.

What RB3 should look like here: the song should be wrapping up or already
in the post-song stats / "Great job!" screen with stars awarded.

Anomalies: zero state advancement on the visible side. (The game logic is
running — boot-to-song already proved that. But the renderer is frozen
on a placeholder.)

Severity: SHOW_STOPPER.

---

## Overall verdict — recalibrated

**Recognizability: ~45-50% of RB3.**

The menu chain (frames 01-07) is genuinely on-model in its skeletal
structure: the rooftop diorama, the marquee-arch, the truss, the
overshell, the Music Library label, the localized menu items. A
knowledgeable RB3 observer would clock the menus as "yes that's
Rock Band 3" within a couple of seconds — they would be looking past
several broken layers (missing menu cards, grey occluder, residual
format-string leaks) but the *bones* are RB3 bones.

The gameplay (frames 08-11) is **0% recognizable as RB3 gameplay**.
A knowledgeable observer shown only frames 08-11 would not identify the
game. The flat tri-colour wedge against black, with a `%d%%` HUD, looks
like an early-prototype tutorial mock-up from a different rhythm game,
or like Clone Hero at first-launch with no chart loaded.

So you have a bimodal product: ~75% recognizable menus, ~0%
recognizable gameplay. Weighted across the 11 frames where menus are
7/11 and gameplay is 4/11, the headline number lands around 45-50%.

### Recognizability test (would an RB3 fan ID it?): 5 / 11

- Frame 01: PASS (rooftop diorama is unmistakable, even half-cropped)
- Frame 02: PASS (sign-in slot row is iconic RB3 chrome)
- Frame 03: FAIL (atmospheric haze, no clear ID hooks)
- Frame 04: PASS (marquee-arch + truss combo is RB3 backstage)
- Frame 05: PASS (PlayNow/Quickplay/Road Challenge text confirms RB3)
- Frame 06: PASS (the words "MUSIC LIBRARY" plus stat fractions)
- Frame 07: FAIL (could be any setlist screen)
- Frame 08: FAIL
- Frame 09: FAIL
- Frame 10: FAIL
- Frame 11: FAIL

5 / 11 passes. Up from prior reviews' menu-only passes but the
gameplay regression keeps the score down.

---

## Top 5 remaining visual issues

1. **Gameplay renderer is producing a static placeholder** (SHOW_STOPPER).
   The highway is a flat three-coloured wedge, no notes, no scroll, no
   venue, no band, no crowd, no lighting. Frames 10 and 11 are byte-
   identical despite 300 frames of song-time between them. This is the
   number-one issue.

2. **`%d%%` HUD score format-string** (SHOW_STOPPER).
   Visible in every gameplay frame. Per user note this is a scoring-
   callback wiring issue, not locale. It's the only on-screen "text" in
   gameplay and it's literally the printf format string.

3. **Grey occluder over Music Library song list** (SHOW_STOPPER for
   browsing). Frame 06 has a giant untextured grey quad covering ~30% of
   the music library — likely a missing texture / fallback material on a
   UI panel. You cannot see song titles.

4. **`%S %I SONGS` format-string in the setlist confirmation**
   (COSMETIC). Frame 07. Uppercase `%S` (wide-string) variant — V5c may
   have only fixed `%s` / `%d`-style narrow-string and integer leaks.

5. **Menu-tile chrome missing in main hub / quickplay** (NICE_BUT_WRONG).
   Frames 04, 05. Text is right but the glowing card backgrounds,
   selection highlight, and chevrons aren't rendering — these are
   probably rendered by a panel layer that's getting culled or alpha-
   zeroed.

---

## Surprisingly correct

- The **rooftop diorama** in frames 01-02 is *strikingly* on-model. The
  geometric purple city, the drummer + kit silhouette, the small props on
  the rooftop platform — this is real RB3 art rendering correctly.
- The **marquee-arch with the bulb pattern** in frames 04-05 is one of
  the most distinctive backstage props in retail RB3 and it's coming
  through cleanly.
- **"MUSIC LIBRARY" header** in frame 06 has the right typography, the
  right colour treatment, and the stat fractions ("0/10" etc.) are
  perfectly formatted — strong evidence that V5c's locale fix shipped.
- The **NO PROFILE / SIGN IN** yellow pill in frame 02 has the correct
  retail look and the correct localized copy.
- The **MIGHTY MODE / BASS / GUITAR** pills in frame 07 are crisp and
  on-model.
- Per V5d: the **4-slot Choose Instrument row** in frame 02 is *actually
  correct retail chrome*, not a "4x ghost stamp" as earlier reviewers
  (myself included, in V4) thought.

---

## Progression since V4

What visibly improved from v4-polish-pass/ → v5-localize-fix/:

- **Localized strings landed throughout menus.** Frame 05's "START A
  ROAD CHALLENGE", frame 06's "MUSIC LIBRARY" and "0/10" / "0/5" / "0/30"
  fractions, frame 07's "SET LIST (3 SONGS)" — these were format-string
  leaks or raw tokens in V4 and they're clean now. Big visible win.
- **NO PROFILE / SIGN IN pill** rendering is cleaner — no token-leak in
  the pill text.
- **OVERSHELL_BASS-style raw tokens** that V4 flagged in the bottom HUD
  are gone in V5.
- **Tour-stars fractions** in the overshell area are properly formatted
  (V4 had `tour_stars_fraction_fmt` showing).

What did *not* improve:

- Gameplay rendering (frames 08-11) is unchanged from V4 — same flat
  wedge, same `%d%%`, same byte-identical-across-300-frames freeze.
- The grey occluder over Music Library is unchanged.
- Main-hub menu cards are still missing chrome.

What got *re-classified* by V5d (not a regression, a re-read):
- The frame-02 "4x Choose Instrument" row is correct retail chrome.

---

## Cumulative progression since first review

The user has commissioned four Opus visual reviews now:

1. `song-load-2026-05-27/REVIEW_OPUS.md` — the initial pass when frames
   08-11 were red voids (catastrophic gameplay-phase clearcolor leak,
   nothing rendered).
2. `v3-gameplay-render/REVIEW_OPUS.md` — post-V1+V2+V3-render; landed
   roughly ~75% menu recognizability and got initial highway geometry
   into frames 08-11. Gameplay was no longer red but still incomplete.
3. `v4-polish-pass/REVIEW_OPUS.md` — post-V4 (blend/zmode work). Flagged
   the format-string leak (`tour_stars_fraction_fmt` etc.), the panel
   ghosting, and the supposed "4x ghost stamp" on Connect-Controller.
4. **This review** (V5+V5c+V5d).

### Status of issues raised in earlier reviews

Per-issue status (FIXED / STILL_BROKEN / WAS_NOT_A_BUG):

- **Red-void clearcolor leak in gameplay frames** (initial review)
  → FIXED (V1/V2/V3-render). Frames 08-11 are now black + highway-wedge,
  not red.
- **No highway geometry at all in gameplay** (initial review)
  → PARTIALLY_FIXED (V3-render). Highway wedge geometry exists but is
  flat, textureless, and doesn't animate or carry notes. Effectively
  STILL_BROKEN for "looks like gameplay".
- **`tour_stars_fraction_fmt` / `SET LIST TITLE` / `OVERSHELL_BASS` raw
  tokens** (V4) → FIXED (V5c).
- **Connect-Controller "4× ghost stamp"** (V4) → WAS_NOT_A_BUG (V5d
  confirmed it's correct retail chrome).
- **Menu-card chrome missing in main hub** (V3/V4) → STILL_BROKEN.
- **Grey occluder over Music Library** (V4) → STILL_BROKEN.
- **`%d%%` HUD format-string in gameplay** (V3/V4) → STILL_BROKEN
  (deliberately deferred per user note — scoring-callback wiring issue,
  not locale).
- **`%S %I SONGS` setlist header** (new in V5 visibility, may have been
  hidden behind other failures earlier) → STILL_BROKEN; uppercase-`%S`
  wide-string variant the locale fix didn't catch.
- **Gameplay frames freezing (10 == 11 byte-identical)** (V4 flagged
  "no animation") → STILL_BROKEN.
- **Resolution change between menu frames and gameplay frames**
  (640x360 → 1280x720 between frame 07 and 08) → unchanged from V4.

### Trend line

Menu chain: initial review ~25% → V3 ~75% → V4 ~75% with text-leak
regressions exposed → V5 ~80% (text leaks mostly cleaned, chrome still
incomplete).

Gameplay chain: initial review ~5% (red void) → V3 ~25% (highway
geometry appeared) → V4 ~25% (no change) → V5 ~25% (no change, format-
string leak unchanged).

**Honest answer to the implicit question — does this look like Rock Band
3 to a knowledgeable observer?**

For the menus, yes — distinctly. Drop a stranger in front of frames 01,
02, 04, 05, or 06 and they'd correctly name the game inside five seconds.

For the gameplay, no — not yet. Frames 08-11 look like a debug harness,
not Rock Band 3. The fact that the underlying game logic is real
(boot-to-song proved that) makes the gap feel narrower than it looks,
but visually the gameplay phase is still a placeholder.

The single highest-leverage next visual fix would be wiring whatever
populates the highway lane texture / fret markers / note-stream into the
already-present geometry. Everything else is polish on top of that.
