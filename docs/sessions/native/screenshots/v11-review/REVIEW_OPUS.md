# Opus subjective visual review — V11 (gameplay, gems-in-window capture)

Reviewer: Claude Opus 4.7 (1M context)
Date: 2026-05-28
Build pin: rb3-native milestone (b) + full V1..V10 wave (binary at build-native/, no recompile)
Screenshots: `/home/free/code/milohax/rb3/docs/sessions/native/screenshots/v11-review/`
Capture: 9000-frame run, frames 3200/4000/4800/5600/6400/7200/8000/8800,
`20thcenturyboy` guitar Expert, headless+audio.

This pass was commissioned specifically because every prior gameplay review
caught only the pre-gem lead-in. This capture targets song-time ~3.5s–9.7s,
deep into the song where the 842 Expert gems should be flowing through the
visible window. I captured the screenshots myself, then formed impressions
from the PNGs and pixel-diffs before re-reading the V4/V5 reviews (diff pass
at the bottom).

---

## Headline finding (read this first)

**The gameplay scene is visually frozen across the entire capture, and the
big red/yellow/blue trapezoid that four prior reviews called "the highway"
is NOT the gem highway — it is a band-member nameplate / track-template
placeholder card.** The actual gem highway is the thin, dark, near-edge-on
surface with yellow side-rails jammed against the **right edge** of the
frame, viewed at a grazing angle where nothing on it is legible. The camera
is not pointed down the highway at all.

Two hard pixel-level facts from this capture:

1. **Distinctness check (md5 + per-region AE diff):** frames 01 (f3200) and
   02 (f4000) are byte-identical. Frames 03–08 each differ from 01, but the
   bounding box of *every* changed pixel across the whole 5600-frame span is
   `67×215+59+286` — a tiny rectangle on the far LEFT edge. That region is
   the vertical star-power/energy meter. The entire gameplay play-area
   (center trapezoid + right-edge highway, a 700×540 crop) is
   **byte-identical (0 differing pixels) across all 8 frames.** Nothing in
   the highway moves. The only thing animating in the whole gameplay view is
   the left meter filling up.

2. The center trapezoid has **three** wide color blocks (red / olive-yellow /
   blue), faint vertical sub-dividers, a `Wil Name` ("Will Name") text label
   at the top, and a small oval logo disc — **not** five gem lanes, no
   descending gems, no strike-line, no fret buttons. This is a nameplate
   card, not a fretboard. (Zoomed 2× crop confirms the text and the logo.)

So the premise this review was meant to validate — "gems are now flowing in
the visible window" — is **not** visually true in this build/run. The data
layer may well be advancing (the left meter animates, so *some* clock ticks;
the V8 doc reports 842 gems set up), but on screen the gem stream is invisible
because the camera frames a placeholder card head-on and the real track
edge-on off to the side.

---

## Per-screenshot impressions

### 01 — `01_f3200_01_f3200.png` (song ≈ 3.5s)

First impression: pure-black field. Top-right: a horizontal pill gauge with a
hollow ring and a circled icon below it (the overdrive/star-power deploy
indicator). Far left: a vertical glassy meter with a small disc icon beside it
(energy/crowd meter). Dead center, floating, a `%d%%` boxed text. Below center,
a head-on red/olive/blue trapezoid with "Wil Name" and a logo. Right edge: a
dark angled surface with bright yellow rails running off-frame.

Expected RB3 here: deep into a song, the screen should be dominated by a near-
vertical 5-lane highway filling the center, colored gems streaming down toward
a strike-line ~2/3 down, a numeric score top-center/right, a colored multiplier
ring on the left, a star meter, and a lit venue with the band behind it.

Anomalies: the dominant central object is a 3-color nameplate card, not a
5-lane fretboard. The real highway is shoved to the right edge and viewed
edge-on (a thin sliver). `%d%%` is the unresolved HUD score format string. No
venue, no band, no crowd, no gems visible. Camera is pointed at the wrong
object entirely.

Severity: SHOW_STOPPER (camera framing + invisible gems + HUD format leak).

### 02 — `02_f4000_02_f4000.png` (song ≈ 4.4s)

First impression: byte-identical to frame 01 (same md5). Nothing has changed
in ~800 frames of song time.

Expected RB3 here: by now, several beats of gems should have streamed past.

Anomalies: identical frame. Zero scene delta.

Severity: SHOW_STOPPER (frozen scene).

### 03 — `03_f4800_03_f4800.png` (song ≈ 5.3s)

First impression: visually indistinguishable from 01/02 in the play area; the
only difference anywhere on screen is the small disc icon next to the left
meter has nudged position and the meter fill changed slightly.

Expected RB3 here: an active, scrolling, gem-dense highway.

Anomalies: highway/center/HUD all pixel-frozen; only the left meter moves.

Severity: SHOW_STOPPER.

### 04 — `04_f5600_04_f5600.png` (song ≈ 6.2s)

First impression: same composition; left meter slightly more filled. Center
trapezoid and right-edge highway unchanged.

Expected RB3 here: mid-song intensity, gems flying, multiplier climbing.

Anomalies: same as above — the gem highway is static. The "gameplay" the
player would watch (the gem stream) is simply not on screen.

Severity: SHOW_STOPPER.

### 05 — `05_f6400_05_f6400.png` (song ≈ 7.1s)

First impression: identical play area; left meter fill is a touch higher.

Anomalies: unchanged. No gems, no venue, no band, no score value.

Severity: SHOW_STOPPER.

### 06 — `06_f7200_06_f7200.png` (song ≈ 8.0s)

First impression: same. The left meter continues its slow climb; everything
else is frozen.

Anomalies: unchanged play area.

Severity: SHOW_STOPPER.

### 07 — `07_f8000_07_f8000.png` (song ≈ 8.8s)

First impression: same composition, left meter higher still.

Anomalies: unchanged play area.

Severity: SHOW_STOPPER.

### 08 — `08_f8800_08_f8800.png` (song ≈ 9.7s)

First impression: the only frame with a visibly different left meter — it is
now substantially filled with a red/textured pattern (looks like the energy
or crowd meter approaching a threshold). The play area is still byte-identical
to frame 01.

Expected RB3 here: late-song, the highway should be at peak density and the
star meter should reflect accumulated play.

Anomalies: the left meter animating is the *only* evidence on screen that the
song is progressing. The gem highway never changes.

Severity: SHOW_STOPPER (for gameplay legibility), but note: the animating
meter is genuine progress — it proves the transport clock IS ticking and is
driving at least one HUD element.

---

## Overall verdict

**Is the gameplay now recognizable RB3? No. Recalibrated gameplay
recognizability: ~10%.**

That is essentially flat versus V4/V5's ~25% — and I'd argue the V4/V5
gameplay number was over-credited, because it assumed the central trapezoid
was a (2-lanes-short) highway. Now that this capture proves the trapezoid is
a nameplate placeholder and the real track is off-screen-right and edge-on,
the honest read is that the gameplay *view* is mis-composed at a fundamental
level: the camera is pointed at the wrong object.

The small uptick to ~10% (vs. an arguable ~5% if the trapezoid were the
highway and just empty) is for one real reason: **the left HUD meter
animates across the capture.** That's the first time in the review series
that *anything* in the gameplay phase has been shown to move frame-to-frame.
The transport clock is ticking and is wired to at least one HUD widget. That
is a genuine, if narrow, advance over the V4/V5 "byte-identical across 300
frames" total freeze.

But the player's eye in RB3 lives on the gem stream, and the gem stream is
both (a) invisible (real highway is edge-on off-screen) and (b) apparently
not animating in the visible region anyway. A stranger shown these 8 frames
would not identify Rock Band 3 gameplay. They'd see a black screen with two
HUD gauges, a `%d%%` debug string, and a small tri-color card.

### Recognizability test (would an RB3 fan ID this as RB3 gameplay?): 0 / 8

None of the 8 frames would be identified as RB3 *gameplay*. (A fan might
recognize the top-right overdrive pill and the left energy meter as RB3 HUD
furniture, but the defining element — the 5-lane gem highway down the center
— is absent from the frame.)

---

## Camera-framing assessment (the focused Step-4 answer)

**The framing is wrong, and it is the single most important fix.** This is
not "gems are small/distant in an otherwise-correct framing." The camera is
looking at the wrong object. Concretely, from the pixels:

- The **dominant central object is a placeholder, not the highway.** It is a
  3-block red/olive/blue trapezoid carrying a `Wil Name` ("Will Name") label
  and a small logo disc. RB3's guitar highway has 5 lanes (G/R/Y/B/O), gems,
  and a strike-line; this card has none of those. This is almost certainly
  the band-member nameplate / "track template" graphic from `tracksystem.milo`
  (the run log shows `tracksystem.milo` skinned-mesh notifies for
  `gem_mash0..5`, and `mtv_overlay.milo` loading `artist.lbl`/`song.lbl`,
  i.e. the lower-third nameplate machinery is active). It is occluding the
  center of the frame where the real highway should be.

- The **real gem highway is the dark, yellow-railed surface against the right
  edge**, rendered nearly edge-on. Its yellow side-rails are the lane-boundary
  glow. At this grazing angle the lane faces and any gems on them present
  almost zero screen area — which is exactly why "gems flow but you can't see
  them." This matches V8-C's "small/distant in the current camera framing"
  concern, but the truth is worse than small: the highway is rotated ~90° away
  from the camera and pushed off-axis.

So there are two compounding framing faults:

1. **A track-template / nameplate overlay (the tri-color "Will Name" card) is
   sitting in the center of the frame and should be hidden (or moved to its
   intended small lower-third slot).** It is masquerading as the highway and
   has misled four prior reviews.

2. **`game.cam` is not aimed down the gem highway.** The highway renders
   off-screen-right at a grazing angle. V10 fixed the NaN runaway on the
   camera-rig transform (the rig no longer blows up), but the *resting* camera
   orientation/position relative to the track is wrong — the cam ends up
   looking past the track rather than down it.

**Concrete fix direction (in priority order):**

- **First, hide/disable the tri-color "Will Name" track-template card** and
  re-capture. There is a real chance the gem highway is rendering *behind* or
  *beside* this card and the card is simply the most prominent lit object.
  Find the widget that draws the `Wil Name` + logo trapezoid (start at
  `tracksystem.milo` track-template / nameplate, and the `mtv_overlay.milo`
  `artist.lbl`/`song.lbl` path that the log shows handling
  `set_artist_name_from_shortname` / `set_song_name`) and confirm whether it's
  drawn on the game cam in world space (occluding the highway) vs. a HUD
  overlay. If it's a world-space placeholder, suppress it.

- **Second, fix `game.cam`'s resting transform** so it looks down the highway.
  RB3's signature gameplay cam is a near-vertical look down a highway that
  fills the center of the frame, vanishing point high-center, strike-line
  ~2/3 down. The current cam has the track edge-on at the right margin, so the
  cam yaw/position is off by a large angle (roughly looking 90° off the track
  axis). Per the V8/V10 trace the cam parent chain is
  `track_0 → rig.grp → cameras_top.grp → cameras.grp → rotater.grp →
  rotater_roll.grp → scaler.grp → game.cam`. The right move is to verify the
  cam target/position the rig settles to (after the V10 anim-swap fix) against
  the matched-fork intended values — the V10 fix stopped the NaN blowup but
  the steady-state pose it lands in is the wrong one. Check whether the
  `camera_intro.tnm` / track camera TransAnims are sampling the intended frame
  now that the (frame, blend) arg order was swapped; a wrong steady-state pose
  is a strong symptom that the anim is still being sampled at the wrong frame
  (e.g. clamped to frame 0 / the intro pose, or to an end pose), even though
  it no longer diverges.

- **Only after those two**: assess gem size/scroll. Once the cam looks down
  the highway and the placeholder is gone, re-evaluate whether gems are the
  right size and actually scrolling. Given that the play-area is currently
  pixel-frozen even on the (edge-on) highway, there is likely *also* a
  gem-scroll/transport issue on the highway itself (the left meter ticks, but
  the highway Y-scroll `mult = mYPerSecond * Seconds(kRealTime)` may still be
  ~0 for the track group specifically — consistent with the V8 doc's
  "song-clock not advancing for the camera scroll" caveat that V10 left open).

**The single most important framing fix:** point `game.cam` down the highway
(and hide the tri-color nameplate card). Without that, gem size/scroll/HUD
polish is invisible — you cannot see the highway at all.

---

## Top 5 remaining gameplay-visual issues

1. **Camera not aimed down the highway; tri-color nameplate card occludes
   center (SHOW_STOPPER).** The real highway is edge-on at the right margin;
   the center is a `Wil Name` placeholder. Root framing defect. See assessment
   above.

2. **Gameplay play-area is visually frozen (SHOW_STOPPER).** The entire
   700×540 play region is byte-identical across frames 3200→8800. Only the
   left energy meter animates. Gems are not visibly scrolling even on the
   edge-on highway — suggests the track-group Y-scroll / transport is still
   not advancing for the highway (separate from the left meter, which IS
   clocked). This is the V8/V10 "song-clock for camera scroll ≈ 0" caveat,
   still open.

3. **`%d%%` HUD score format-string (SHOW_STOPPER).** Unresolved printf in the
   center HUD on every gameplay frame. Per prior notes this is a scoring-
   callback wiring issue, not locale. It's the only "text" in the gameplay
   view and it's the literal format string.

4. **No venue / band / crowd / stage lighting (NICE_BUT_WRONG → SHOW_STOPPER
   for atmosphere).** Pure-black background behind everything. Deferred per
   V5b's `PostLoadInlined` venue obstructions, but with the highway also
   absent, the black void makes the frame read as a debug harness.

5. **No 5-lane fretboard, no gems, no strike-line visible (SHOW_STOPPER).**
   Downstream of #1 — once the cam is fixed these should appear, but as
   captured, none of RB3's defining gameplay geometry is on screen. The
   `gem_smasher_*` / `prism_gem_stepped_*` meshes the V8 doc confirmed are
   drawing are not visible at this camera angle.

---

## Progression vs. v4-polish-pass and v5-localize-fix (gameplay frames only)

Read the V4 and V5 reviews last, for this diff.

- **V4 (`v4-polish-pass/REVIEW_OPUS.md`)** graded gameplay ~25%, describing
  frames 08–11 as "a bare wireframe-feeling three-lane track in red/yellow/
  blue … missing 2 of 5 lanes … no gems … `%d%%` HUD" and "frames 08, 09, 10,
  11 are pixel-identical." V4 explicitly called the central trapezoid "the
  gameplay highway" with "only three lanes."

- **V5 (`v5-localize-fix/REVIEW_OPUS.md`)** graded gameplay 25% (flat vs V4),
  "0% recognizable," described "a narrow three-coloured slab … red on the
  left, dark olive-yellow in the middle, blue on the right. The slab IS the
  highway" — again identifying the trapezoid as the highway, just empty/flat.

- **This review (V11)** corrects the central misreading shared by V4 and V5:
  **the tri-color trapezoid is not the highway** — it's a `Wil Name`
  nameplate/track-template placeholder, and the **real highway is the edge-on
  yellow-railed sliver on the right** that V4/V5 didn't call out (it was
  present in their gameplay frames too, but unremarked because the trapezoid
  dominated attention). This reframes the gameplay problem from "the highway
  is empty / missing 2 lanes" to "**the camera is pointed at the wrong object
  and the highway is off-axis/edge-on.**"

What genuinely improved since V4/V5:

- **The left HUD energy meter now animates** across the capture (frame 08
  shows it substantially filled vs. frame 01). V4/V5 had byte-identical
  gameplay frames across 300 frames with *zero* motion. So the transport clock
  now drives at least one HUD widget — the first frame-to-frame motion ever
  seen in the gameplay phase. Narrow, but real.
- The top-right overdrive/star-power pill and the left energy meter are both
  present and on-model as HUD furniture (V4/V5 noted no HUD at all beyond
  `%d%%`).

What did NOT improve:

- Gem highway still not legible (now understood to be a camera-framing
  problem, not an empty-highway problem).
- `%d%%` HUD format string unchanged (deliberately deferred).
- No venue/band/crowd unchanged.
- The center of the frame is still dominated by the same tri-color card V4/V5
  saw — only now correctly identified as a placeholder, not a highway.

Gameplay trend line (recalibrated): initial ~5% (red void) → V3 ~25% (geometry
appeared, but it was the nameplate card) → V4 ~25% → V5 ~25% → **V11 ~10%**.
The drop from 25% to 10% is a *correction*, not a regression: prior numbers
credited a placeholder card as a highway. The one true gain (animating HUD
meter) keeps it above the ~5% red-void floor.

---

## Honest bottom line

Would someone glancing at these screenshots recognize Rock Band 3 gameplay?
**No.** They would see a mostly-black screen with two HUD gauges, a `%d%%`
debug string, and a small tri-color card with "Will Name" on it. The gem
highway — the entire point of the game's visual identity — is not in the
frame; it's edge-on at the right margin.

The single most important framing/presentation fix: **aim `game.cam` straight
down the gem highway and hide the tri-color "Will Name" track-template card.**
That one change converts the gameplay view from "debug harness" to "recognizable
RB3 highway" in a single step — everything else (gem size, scroll, HUD value,
venue) is polish that's literally invisible until the camera looks at the right
object.

---

## Method note / data backing the claims

- Capture command: the V11 review command from the task (9000 frames,
  `20thcenturyboy` guitar Expert, `MILO_SCREENSHOT_FRAMES=3200..8800`). Run
  completed cleanly (exit 0, "9000 frames done").
- Distinctness: `md5sum` shows 01==02; 03–08 distinct. `magick compare -metric
  AE` shows the *only* changed-pixel bounding box across the run is
  `67×215+59+286` (the left meter). The 700×540 play-area crop is 0-pixel-diff
  (byte-identical) for all 8 frames vs. frame 01.
- Object identification: 2× zoom crop of the center (`-crop 360x320+460+420`)
  reads "Wil Name" + a logo disc over 3 color blocks with faint sub-dividers;
  zoom crop of the right edge (`-crop 280x500+1000+280`) shows a long dark
  surface with bright yellow side-rails at a grazing angle.
- Log corroboration: `tracksystem.milo` (`gem_mash0..5`) and
  `mtv_overlay.milo` (`artist.lbl`/`song.lbl`,
  `set_artist_name_from_shortname`/`set_song_name`) are active; track set to
  `guitar`/Expert, player ADDed. (This run had no `GEM_DBG`/`RENDER_DBG`, so
  no per-gem poll counts — but the 842-gem setup is documented in
  `../V8_GEM_STREAM_BLOCKERS.md`.)
