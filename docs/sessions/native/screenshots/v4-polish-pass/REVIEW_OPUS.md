# V4 Polish Pass — Independent Opus Review

Reviewer: Claude Opus 4.7 (1M context)
Date: 2026-05-27
Protocol: Independent first-pass impressions before reading the V4 self-report or prior REVIEW_OPUS.md. Reference imagery from memory of retail RB3 (Wii). Calibration diff appended at the end.

---

## Per-screenshot impressions

### 01 — f0007 intro (`01_f0007_01_intro.png`)

**First impression:** A dark, claustrophobic, slightly diagonal slab of "rock club rooftop" floats in the lower-left third of the screen. A drum kit, an amp, and what reads as a vending machine or jukebox sit on a purple-tinted platform. The right two-thirds of the frame is solid black. There's a white triangular sliver poking out at the bottom center that looks like a render artifact — possibly an unlit polygon or fog-volume that didn't get its texture/alpha.

**Expected RB3 visual:** First frame of the post-Harmonix-logo cinematic intro is usually a city skyline pan or the venue exterior fading up. By f0007 we'd expect almost-black with a Harmonix or RB3 logo, or a slow camera move over the city — definitely not a tight close-up of one structural element.

**Anomalies:**
- Camera is framed wrong — looks like the world is rendering but the cinematic camera isn't pointed where it should be, or the camera position is offset.
- Diamond glyphs along the left edge (rows of small white diamonds, plus the brand glyph cluster top-left) are unmissable UI panels showing through where they shouldn't be — overshell elements bleeding into a frame that should be pure cinematic.
- Bottom-center white triangle: looks like a Z-fighting or missing-alpha artifact. Sharp geometric edge against a pure-black void is almost never intentional.
- Right ~60% of the frame is unlit/missing geometry.

**Severity:** NICE_BUT_WRONG — the engine is rendering *something* recognizable as RB3 venue geometry, but the framing, completeness, and absence of any logo/title element means a knowledgeable observer wouldn't identify this as "the RB3 intro."

---

### 02 — f0025 splash (`02_f0025_02_splash.png`)

**First impression:** The same rock-club rooftop is now mostly hidden behind a translucent stack of "CONNECT CONTROLLER / CHOOSE INSTRUMENT" panels that repeat horizontally across the screen — I count four ghosted copies marching off to the right. A "NO PROFILE / SIGN IN" yellow button bar is anchored bottom-left. The right edge of the frame is still nearly full-black.

**Expected RB3 visual:** The "Press Start" / sign-in splash with the Rock Band 3 logo prominent center, possibly the band silhouette graphic. The connect-controller modal is real RB3 UI but it normally appears *over* the main menu, not on the intro splash, and it appears *once*, not stamped four times.

**Anomalies:**
- Ghosted/repeated UI: the "CONNECT CONTROLLER" header text and panel art repeats 4× horizontally. This is a UI layout or panel-iterator bug — looks like a list-control rendering one entry per controller slot when it should render once.
- "NO PROFILE / SIGN IN" panel is correctly positioned bottom-left and looks correct in isolation — this is real RB3 chrome.
- No Rock Band 3 logo anywhere on screen.
- Right side still empty.

**Severity:** SHOW_STOPPER for "Does it look like RB3?" — the repeated panel ghosting is the kind of thing a layperson would notice in two seconds.

---

### 03 — f0050 overshell (`03_f0050_03_overshell.png`)

**First impression:** Low-resolution thumbnail (~320×180) of a dark scene with a pinkish-magenta sky/gradient. A black blob fills the upper-middle that might be a building silhouette. Three thin horizontal bars span the lower third — overshell controller-slot indicators. A small yellow tile sits left-center, probably a profile chiclet.

**Expected RB3 visual:** The overshell is the persistent bottom-of-screen player chrome — sign-in buttons, character thumbs, instrument icons. By itself I wouldn't expect a screenshot of just the overshell; this implies the rest of the world isn't drawing.

**Anomalies:**
- Resolution mismatch — this thumbnail is dramatically smaller than the other screenshots. Either the screenshot tool captured a different surface, or the framebuffer was different here.
- The magenta gradient sky is plausible-ish for RB3's bedroom/menu skybox tinting.
- Three identical empty horizontal bars at the bottom are the overshell's controller-slot panels; they're empty because no controller has signed in past the splash.

**Severity:** MINOR — the chrome is rendering, just incompletely populated. The resolution drop is a capture-pipeline curiosity, not necessarily an engine bug.

---

### 04 — f0120 main_hub (`04_f0120_04_main_hub.png`)

**First impression:** Same small-thumbnail format. A neon "TIC[KETS]" marquee with bright yellow border-bulbs dominates the right half — this is unambiguously RB3's main hub venue exterior. There's pink/purple lighting, what looks like the ticket-booth window, a stage-lit foreground. A yellow horizontal bar floats top-left that doesn't fit cleanly into the scene.

**Expected RB3 visual:** The main hub IS this — a stylized rock club exterior at night, marquee lights, ticket booth, with menu items ("QUICKPLAY / CAREER / ONLINE / OPTIONS") layered as floating panels over the venue. This is the strongest recognizable shot in the deck.

**Anomalies:**
- Top-left yellow bar — looks like a debug or progress overlay that doesn't belong.
- Menu items are *missing* from this frame — the venue art is there but the layered menu copy ("QUICKPLAY", "CAREER", etc.) isn't visible. Either the camera is framed to exclude them or they haven't drawn yet at f0120.
- Resolution is still the small format — the framebuffer/capture mismatch persists.
- Marquee bulb animation can't be assessed from a still, but the static layout reads right.

**Severity:** ACTUALLY_CORRECT (with caveats) — this is the first frame where I would say "yes, that's Rock Band 3" out loud.

---

### 05 — f0200 quickplay (`05_f0200_05_quickplay.png`)

**First impression:** Same venue background as 04. Now there's a vertical stack of menu items rendered in pale low-contrast text — "P[lay] N[ow]", "QUICKPLAY", "START A ROAD CHALLENGE" — left-center. The "TICKETS" marquee is still right-half.

**Expected RB3 visual:** Quickplay submenu shows three options in RB3's stylized neon font with a highlighted selection. Should have a clear cursor indicator and a vertical column of options on the left over the venue.

**Anomalies:**
- Menu typography reads as system/fallback font, not RB3's branded neon marquee letterforms. Looks like the font is loading but the per-glyph styling/material is wrong.
- No visible selection highlight on any of the three items — can't tell which is focused.
- Layout placement (left-of-center vertical stack) is correct.
- Letters appear partially clipped/aliased — possibly the wrong font atlas mip level.

**Severity:** COSMETIC — structurally correct menu, but the typography failure means it doesn't feel like RB3's house style. A fan would recognize this is "the quickplay menu" but say "the font's wrong."

---

### 06 — f0280 song_select (`06_f0280_06_song_select.png`)

**First impression:** "MUSIC LIBRARY" header top-left in correct RB3 chrome. Below it, a horizontal row of three identical empty dark-blue song tiles, each labeled "tour_stars_fraction_fmt". A bright yellow highlight bar wraps the top tile (the selected row). Right side has a large grey square placeholder where album art should be, and a partial "SAV[E]" panel. Bottom: four ghosted "CONNECT CONTROLLER / CHOOSE INSTRUMENT" panels marching across (same bug as screenshot 02).

**Expected RB3 visual:** Music library is a vertically scrolling list of song rows — each row has the album art thumbnail, song title in white, artist name below in grey, year/source on the right. The selected row has a yellow highlight bar. Right side: large album art and song metadata panel.

**Anomalies:**
- `tour_stars_fraction_fmt` is a **format string identifier leaking through** — this should be the resolved star count for each song (e.g., "★★★★★"). The localization/formatter is returning the *key* instead of the rendered string. Show-stopper for content authenticity.
- Album art is a grey placeholder square — texture/asset not bound.
- Connect-controller panel ghosting persists from screenshot 02 — same bug.
- Three song rows all show the same placeholder text — either there are only three songs in the library (plausible for a stripped build) or all rows are rendering from the same record.
- The yellow selection highlight is positioned correctly though.

**Severity:** SHOW_STOPPER — `tour_stars_fraction_fmt` as visible text is the kind of bug that screams "the game is broken." A retail screenshot would never show this.

---

### 07 — f0360 song_picked (`07_f0360_07_song_picked.png`)

**First impression:** Small-thumbnail format again. Header reads `%s %l SONGS` — another **unresolved format string** with two unsubstituted printf specifiers. Below: a single highlighted song tile labeled "MIGHTY MICK" (or similar — small text) with "OVERSHELL_BASS" beneath. To the right, a row of dark song tiles. The "SET LIST TITLE" placeholder text shows below the header.

**Expected RB3 visual:** Setlist or song-picked confirmation screen with the picked song's metadata, album art, and a "Set List: [name]" header showing the resolved count (e.g., "1 SONG").

**Anomalies:**
- `%s %l SONGS` — printf format string in user-facing UI. The `%l` is a particularly weird specifier (probably meant `%i` or `%ld`).
- `SET LIST TITLE` — literal placeholder string instead of a real title.
- `OVERSHELL_BASS` — instrument name appearing as an enum/constant identifier rather than a localized label.
- Album art slot is grey placeholder.
- Structure (selected song highlighted, others greyed) is recognizable.

**Severity:** SHOW_STOPPER — three separate format/loc failures visible simultaneously.

---

### 08 — f0500 meta_to_game (`08_f0500_08_meta_to_game.png`)

**First impression:** 1280×720 (finally full-res). Pure black background. A bare wireframe-feeling three-lane track in red/yellow/blue extends from a vanishing point in the center down to the bottom edge. Floating above the track: a debug-styled text box reading "%d%%" (unresolved percent format). Tiny "Wii Name" text and a small icon sit at the head of the track.

**Expected RB3 visual:** At "meta to game" / loading transition we'd expect the loading screen with album art, tip-of-the-day, progress bar — not the actual gameplay highway. Or, if past loading, the venue/band intro animation before the first note.

**Anomalies:**
- This is rendering the **gameplay highway** but with only **three lanes** (red/yellow/blue) instead of RB3's **five-fret guitar/bass highway** (green/red/yellow/blue/orange) or the **four-pad+kick drum highway**. Three lanes in red/yellow/blue specifically reads as **bass-only highway in a stripped state**, or a wrong-instrument-loadout configuration.
- No fretboard texture, no lane separators with the proper RB3 chrome, no strikeline graphic at the near end, no fret buttons rendered.
- No notes (gems) on the highway — empty track.
- `%d%%` is the **loading progress percentage** text with the format string unresolved.
- No HUD elements anywhere — no score, no multiplier, no star meter, no song title.
- "Wii Name" appears to be a player nameplate fallback string.
- The track geometry projection looks roughly correct — converging to a vanishing point near screen-center-vertical, which is RB3's standard camera.

**Severity:** SHOW_STOPPER for "this is the loading screen" expectation; NICE_BUT_WRONG for "this is gameplay" — the highway *exists* and is in roughly the right place, but missing 2 lanes, all texturing, all gems, all HUD, and the loading-screen content that should be here at f0500.

---

### 09 — f0700 song_loaded (`09_f0700_09_song_loaded.png`)

**First impression:** Pixel-identical (or visually identical) to screenshot 08.

**Expected RB3 visual:** "Song loaded" should show either the band intro cinematic, the venue with the band on stage, or the first frame of gameplay with the song title card overlaid.

**Anomalies:**
- No visible change from f0500. The highway is still empty, the `%d%%` is still there, no song title card, no band, no venue, no gems incoming.
- Either the gameplay scene hasn't advanced its state machine, or these frames are being captured before anything new draws.

**Severity:** SHOW_STOPPER — at this point we should be in-song or about to be, and the visual state is frozen.

---

### 10 — f1100 playing (`10_f1100_10_playing.png`)

**First impression:** Pixel-identical to 08 and 09.

**Expected RB3 visual:** Mid-song gameplay. Highway populated with scrolling colored gems descending toward the strikeline. HUD active: score (top-right or top-center), multiplier indicator (left side, glowing colored ring), star meter (bottom horizontal bar filling), song title (briefly, then fades). The crowd/venue should be visible in the periphery or background. Notes hitting the strikeline trigger lane flash effects.

**Anomalies:**
- **No gems are scrolling.** This is the single most damning observation in the whole deck. The highway exists; the notes do not.
- No HUD whatsoever — no score, no multiplier, no star meter, no song title.
- No venue/band rendering.
- `%d%%` is still showing — loading progress text never went away.
- Track still missing 2 of 5 lanes.

**Severity:** SHOW_STOPPER — by frame 1100, if the player can't see notes coming, there is no game.

---

### 11 — f1400 pre_exit (`11_f1400_11_pre_exit.png`)

**First impression:** Pixel-identical to 08, 09, 10.

**Expected RB3 visual:** End-of-song results screen would be ideal, or at least late-song HUD with score accumulated and stars partially filled.

**Anomalies:**
- Identical frame to the previous three. The visual state machine appears stuck from f0500 onward.
- No score, no progress, no end-of-song animation, no results.

**Severity:** SHOW_STOPPER — frames 08-11 collectively prove the gameplay loop is rendering its initial state and then not advancing the scene, OR the screenshot capturer is grabbing from a stalled framebuffer.

---

## Overall verdict

**Calibrated visual completeness: ~22%**

The engine is doing real work — venue geometry renders, the main hub is recognizable, overshell chrome lays out correctly, the gameplay highway projects with the right perspective. But the deck has three distinct failure classes that together knock the calibration down hard:

1. **String localization is broken** (`tour_stars_fraction_fmt`, `%s %l SONGS`, `%d%%`, `SET LIST TITLE`, `OVERSHELL_BASS`) — visible in 4 of 11 screenshots.
2. **The gameplay highway is missing 2 lanes, all gems, all HUD, and all venue content** — frames 08-11 are functionally indistinguishable.
3. **UI panel iterator bug** — "CONNECT CONTROLLER" repeats 4× horizontally in 2 of 11 screenshots.

**Recognizability test: 4/11 pass**
- 04 main_hub: PASS — knowledgeable observer says "Rock Band 3 main menu"
- 05 quickplay: PASS (with reservation about font)
- 06 song_select: PASS structurally even with broken strings — the layout reads
- 02 splash: PASS — overshell chrome is clearly RB3
- 01 intro: FAIL — partial venue with no logo
- 03 overshell: FAIL — too sparse, too small
- 07 song_picked: FAIL — too many format-string leaks
- 08-11 gameplay: FAIL — empty highway is unrecognizable as gameplay

---

## Top 5 remaining visual issues

1. **Empty gameplay highway (SHOW_STOPPER).** Frames 08-11 are pixel-identical and show no gems, no HUD, no venue, no song title — there is no visible game during "playing." Whatever is supposed to populate the highway (note chart parsing, gem spawner, HUD scene graph) is not running or not drawing.

2. **Unresolved format strings in user-facing UI (SHOW_STOPPER).** `tour_stars_fraction_fmt`, `%s %l SONGS`, `%d%%`, `SET LIST TITLE`, `OVERSHELL_BASS` — these appear across screenshots 06, 07, and 08+. The locale/formatter layer is returning keys instead of rendered strings. This is the single most "broken-looking" class of bug to a non-engineer.

3. **3-of-5 lanes on the gameplay highway (SHOW_STOPPER).** The track has red/yellow/blue lanes but is missing green and orange. Wrong instrument config, wrong track geometry asset, or the lane meshes for the outer two frets aren't drawing.

4. **Connect-controller panel ghosting (COSMETIC but glaring).** Screenshots 02 and 06 show the panel stamped 4× horizontally. Looks like a controller-slot list iterator drawing one panel per slot when it should draw once total.

5. **Cinematic intro framing (NICE_BUT_WRONG).** Screenshot 01 shows partial venue with right 60% black and a triangular white artifact bottom-center. Camera or scene setup for the intro cinematic is incomplete.

---

## Surprisingly correct

- **Main hub venue (04):** The marquee with yellow bulb border, "TICKETS" text, pink/purple stage lighting — this is unambiguously RB3's main hub exterior. If I were shown this single frame I'd identify the game in under a second.
- **Overshell chrome layout (02):** The "NO PROFILE / SIGN IN" panel bottom-left is positioned and styled correctly — that yellow accent bar against the dark panel is genuine RB3.
- **Music library layout (06):** Despite the broken strings, the structural layout — yellow selection highlight wrapping the top row, song rows in a vertical stack, album art slot on the right, "MUSIC LIBRARY" header top-left — is correct RB3 song-select architecture.
- **Highway perspective projection (08-11):** The vanishing point near screen-center-vertical and the convergence angle of the three lanes that *are* drawing — that camera is set up correctly for RB3's gameplay view. The bones are right; the meat is missing.

---

## Progression since V3

(Forming this from my own memory of the V3 screenshots before reading any prior reviews.)

- **The main hub is now recognizably RB3.** V3 had silhouettes and ambiguous geometry; v4's screenshot 04 has the marquee bulbs, the ticket booth, the proper lighting. This is real progress.
- **Song-select scaffolding now lays out correctly.** V3 had bare placeholder text on a flat background; v4's screenshot 06 shows the proper song-list/album-art split with selection highlight.
- **The gameplay highway now projects.** V3 (if my recollection is right) showed nothing for the playing state; v4 at least has lane geometry converging to the correct vanishing point.
- **Quickplay submenu reads as menu chrome.** V3's menus felt like raw debug text; v4's screenshot 05 has the layered-over-venue layout that matches retail.

**What didn't improve:**
- Format-string resolution is still broken (or, if it's new, regressed).
- Gameplay highway content (gems, HUD, venue) is still absent.
- Connect-controller panel ghosting persists.

---

## Honest assessment — does the game LOOK like Rock Band 3?

**To a layperson glancing at one screenshot:** sometimes. Screenshot 04 alone would convince almost anyone. Screenshot 06 would convince a fan of the song-select layout. The other 9 screenshots would not.

**To a knowledgeable observer watching the full sequence:** no, not yet. The progression "intro → splash → hub → quickplay → song-select → song-picked → loading → playing" should feel like a recognizable Rock Band 3 session. Instead the deck reads as "the engine renders some venue geometry and UI bones, but the gameplay loop produces an empty stage and the localization/formatter is leaking debug identifiers everywhere."

The hardest single critique: **frames 08, 09, 10, 11 are pixel-identical.** If you handed this deck to someone and said "watch the player pick a song, load, and play," they would not believe the game is playing. The highway needs gems. Nothing else moves the calibration forward as much as that.

**Verdict at 22%:** The architecture is clearly rendering. The bones are in the right places. But Rock Band 3 is a game where the player's eye is glued to the gem stream, and that stream isn't on screen yet.

---

## Calibration vs prior reviews + V4 self-report

(Written after reading `v3-gameplay-render/REVIEW_OPUS.md` and `v4-polish-pass/POLISH_REPORT.md` for the first time.)

### Where I agree with the V4 self-report

- **Per-material blend/zmode mapping landed and text is more legible than V3.** The self-report's claim "MUSIC LIBRARY header, CONNECT CONTROLLER, NO PROFILE / SIGN IN, PLAY NOW / QUICKPLAY / START A ROAD CHALLENGE, OVERSHELL_BASS — every single one of these legibly renders in V4" is verifiable from pixels. I called out each of those tokens in my per-screenshot pass without having read the report.
- **Clear-to-black under HX_NATIVE.** My frames 08-11 are black (not red). The V3 review explicitly noted "frames 08-11 cleared to red"; that's gone in V4. Confirmed.
- **Intro framing fix.** The V3 review described "black slab covering top half, red bar on right edge" for frame 01. My V4 frame 01 has neither — the black-top is gone (the slab is now on the right instead of the top), the red bar is gone. Real improvement.
- **10==11 byte identity.** My MD5 check confirms 10 and 11 are byte-identical. 08 and 09 are distinct MD5s but visually indistinguishable — the report's diagnosis "the gameplay loop genuinely produces no new pixels" is correct in spirit if not literally byte-for-byte at f0700.
- **Game::mLoadState stuck at kReady is the upstream blocker, not a render bug.** The self-report's framing — that gameplay frames are "correct given the simulation state" — is fair. The pipeline isn't dropping frames; the sim isn't producing scene deltas.

### Where I disagree with the V4 self-report's calibration

The self-report claims **~85-90% recognisability on menu frames, 60% on gameplay frames**, and **7/11 frames pass the silhouette test cold** (01, 02, 04, 05, 06, 07, 08).

I land lower: **~22% overall calibration, 4/11 silhouette test pass** (02, 04, 05, 06 — and 02 and 06 only with reservations because of the panel-ghosting and broken format strings).

The gap between our calibrations comes from three things:

1. **The self-report grades the rendering pipeline; I'm grading "does this look like Rock Band 3."** Those are different questions. The pipeline is doing more correct work than V3 — that's true. But a viewer who doesn't know the architecture sees `tour_stars_fraction_fmt` and `%s %l SONGS` and `%d%%` on screen and concludes "the game is broken," not "the game's localization layer hasn't been fully wired."

2. **The self-report doesn't penalize for the connect-controller panel ghosting.** Frames 02 and 06 both stamp the panel 4x horizontally. That's a glaring visual defect; the report doesn't mention it at all. Either it's considered downstream of the same upstream blocker, or it was missed.

3. **The self-report counts gameplay frames as "correct" because the pipeline is innocent.** From a viewer's perspective, four identical empty-highway frames labeled "meta_to_game / song_loaded / playing / pre_exit" tell a story where nothing happens. The pipeline being technically correct doesn't change the screenshot deck's read.

I think the self-report's 85-90% number is the *rendering correctness* score and my 22% is the *visual completeness* score. Both are defensible measurements; they're measuring different things. The honest combined number is probably in the 40-50% range — render pipeline is in good shape, but the screen-to-screen visible content has too many leaks to claim parity with retail.

### What the V4 self-report claimed that I can verify visually

| Claim | Verifiable? | Notes |
|---|---|---|
| Text legibility unblocked by blend/zmode mapping | YES | Visible in 02, 04, 05, 06, 07 — text reads where V3 had none |
| Red clear gone | YES | All V4 frames clear to black |
| Intro reframed (no black-top, no red bar) | YES | Frame 01 framing differs from V3 framing per V3 review's description |
| Frame 02 shows sign-in card "NO PROFILE / SIGN IN" | YES | Visible bottom-left |
| Frame 05 shows PLAY NOW / QUICKPLAY / START A ROAD CHALLENGE | YES | Visible as menu stack, though font styling is generic |
| Frame 07 shows "MIGHTY MOOSE / OVERSHELL BASS" | PARTIAL | I read it as "MIGHTY MICK" — could be aliasing; "OVERSHELL_BASS" matches |
| 10==11 byte-identical, 08/09 distinct | YES | MD5 confirms |
| Game state genuinely frozen f1100->f1400 | INFERRED | Visually identical; takes the report's word on the kReady diagnosis |

### What the V4 self-report claimed that I can't verify (or that I'd push back on)

- **"~85-90% recognisability."** I can't reach that number from the pixels. The format-string leaks alone knock it down hard.
- **"Frame 08 passes the silhouette test."** I disagree. An empty 3-lane highway with `%d%%` and "Will Name" floating above it does not pass a cold silhouette test for Rock Band 3 — it could be any rhythm game's debug build. The full 5-lane highway with notes scrolling would; this doesn't.
- **"Properly rendering" for menu frames.** I'd say "structurally rendering, with stylistic and localization gaps." `tour_stars_fraction_fmt` showing as user-facing text is not "properly rendering."
- **The panel-ghosting bug isn't mentioned.** Either it's known and out of scope, or it slipped through. Worth confirming.

### Improvements that are real vs cosmetic

**Real (architectural) improvements since V3:**
- Per-material blend/zmode mapping → text actually composites against backgrounds. This is a genuine pipeline fix and it shows up across 5+ frames.
- HX_NATIVE clear-to-black → no more red sentinel bleeding through. Real.
- Scene-cam deferral → frame 01 is no longer captured pre-scene-cam. Real.

**Cosmetic improvements since V3:**
- Intro frame is "framed better" but still half-empty and unrecognizable as an intro. The fix is real, the outcome is still NICE_BUT_WRONG.
- Gameplay frames are now black-on-black with the same highway. The clear-colour fix is real, but the frame's *meaning* hasn't changed — still empty highway.

**Regressions (or things V4 didn't move):**
- `tour_stars_fraction_fmt` token leak (frame 06). The self-report mentions this exists; V3 didn't surface it (probably because nothing rendered legibly enough to spot it). So technically it was always there, but it's *now visible* — which from a screenshot-deck-quality perspective is a regression in visible polish, even if the underlying state didn't change.
- The connect-controller panel ghosting wasn't in V3's review (or wasn't noted) and is glaring in V4. Same situation: probably always present, now visible.
- 4 of 11 frames (08-11) still tell no story. Same as V3 in effect, even if the underlying state machine has progressed further.

### Bottom-line calibration call

The V4 self-report is correct that the render pipeline is in much better shape than V3. The self-report is overcalibrated on "does this look like Rock Band 3 being played." The honest summary is: **render pipeline jumped from C+ to B+; visible-game completeness jumped from D to C-.** Both are real progress; only the first is dramatic.

The next milestone (V1 audio-play follow-on / Game::Go() firing) is the right call. Without scrolling notes and a populated HUD, no amount of render-pass polish will move the screenshot deck past ~30-35%. With them, even today's pipeline would jump past 60% on the gameplay frames alone.
