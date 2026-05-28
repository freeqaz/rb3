# Opus subjective visual review — V16 (authoritative final, deep-song gameplay)

Reviewer: Claude Opus 4.7 (1M context)
Date: 2026-05-28
Build pin: rb3-native milestone (b) + V1..V15 wave (binary at `build-native/rb3-native`, no recompile)
Screenshots: `/home/free/code/milohax/rb3/docs/sessions/native/screenshots/v16-final-review/`
Song / track: `20thcenturyboy`, guitar (TrackType=1), Expert. Headless + real audio.

This is the final, authoritative pass commissioned after the V12→V15 fixes
(camera framing, gem descent, skinned-mesh, strike plate). Unlike every prior
review — which sampled the **sparse intro** (frames ~3200–5600, song-time
~3–5s) — this pass empirically mapped frame↔song-time and captured **DEEP into
the song** where the chart should be dense (~6–7 gems/sec by song-time
~22–28s, per V8-C). I captured all frames myself, then formed impressions from
the PNGs before reconciling with the V11 review (diff pass at the bottom).

---

## Headline finding (read this first)

**Two facts, and they pull in opposite directions:**

1. **The highway is now genuinely, unmistakably Rock Band 3.** Centered
   down-the-highway framing, 5-color GRYBO fret rails, a real strike plate at
   the bottom, yellow side-rails, animated beat-lines, and — in the early/intro
   section — **colored gem note-heads descending with sustain trails that move
   frame-to-frame.** This is a categorical leap past V11, which was a black
   screen with the real track edge-on off the right margin. The camera fix
   landed; the gems-descending fix landed; the strike plate is drawn. When
   gems are present, the frame reads as RB3 at a glance.

2. **The gem stream DIES mid-song.** Gems flow correctly through song-time
   ~9s, thin out around ~13–14s, and are **completely gone from ~18s onward.**
   The deep-song frames the task specifically targeted (song-time 23.6s and
   28.5s — the densest part of the chart) show an **empty highway**: rails,
   strike plate, and beat-lines render and animate, but **zero gems**. This is
   the inverse of the task's "sparse intro" hypothesis.

So the answer to "how close is this to real RB3 gameplay?" is honestly
**bimodal**: the first ~10 seconds look strongly like RB3 (~70% recognizable),
and everything after ~18s looks like an empty practice/calibration highway
(~35%). The mechanic provably works — it just stops.

---

## Frame↔song-time mapping (empirically measured)

The task's premise that "song-time advances slower than frame count" is **now
false.** With real audio streaming (`streamPlaying=1`, `audioTime` tracks
`songMs` within ~5ms), song-time is slaved cleanly to the audio clock at
~1.18 ms-of-song per rendered frame:

| Frame | song-time | gems on highway? |
|------:|----------:|:-----------------|
| 4000  | ~3.9 s    | YES — green/blue gems + sustains, scrolling |
| 8000  | ~8.8 s    | YES — red gems + green sustains, scrolling |
| 12000 | ~13.8 s   | BARELY — one faint green sustain stub |
| 16000 | ~18.7 s   | NO — empty highway |
| 20000 | ~23.6 s   | NO — empty highway (densest chart region!) |
| 24000 | ~28.5 s   | NO — empty highway |

The transport clock is healthy. The bug is in what feeds the gem window, not
in the clock.

---

## Per-frame impressions

### 00a — `00a_f3600_intro_gems_descending.png` (song ≈ 3.4 s) — supplementary
First impression: this is a Rock Band guitar highway. Green and blue gem
note-heads sit just above the strike plate; two long green **sustain trails**
run up the highway behind them. The strike plate shows the 5 GRYBO frets
(green/red/yellow/blue/orange) and a blue circular flare on the green-lane
gem at the plate. Expected RB3: exactly this — gems arriving at the strike
line with trailing sustains. Anomalies: none material at this zoom; the gem
shapes are slightly chunky vs retail's beveled prisms but read correctly.
Severity: clean. This is the single most "RB3" frame in the set.

### 00b — `00b_f4400_intro_gem_column.png` (song ≈ 4.9 s) — supplementary
First impression: a descending **column of blue gems** (three stacked) with a
green sustain trail at the top of the highway — a short blue run, which is
chart-accurate for a riff. Gems have visibly moved down vs 00a, proving real
scroll. Expected RB3: a multi-gem run mid-highway. Anomalies: none material.
Severity: clean. Confirms gems both descend AND advance with the clock.

### 01 — `01_f4000_00_intro_songMs4k.png` (song ≈ 3.9 s)
First impression: green and blue gems near the top of the highway, green
sustains down near the strike plate — gems mid-flight between top and plate.
This is the "intro comparison" frame. Expected RB3: a light intro phrase.
Anomalies: none material; this is the sparse-but-real intro. Severity: clean.

### 02 — `02_f8000_01_songMs9k.png` (song ≈ 8.8 s)
First impression: **RED gem note-heads** descending with green sustain trails
— the chart has moved from green/blue to red, so the gem-color pipeline is
correctly chart-driven. Three gems visible across the highway. The left meter
is gray/empty here (it fills red later). Expected RB3: mid-intro density, a
few gems in flight. Anomalies: none material — this is healthy RB3 gameplay.
Severity: clean. **This is the last frame with robust gem density.**

### 03 — `03_f12000_02_songMs14k.png` (song ≈ 13.8 s)
First impression: the highway is nearly empty — a single short green sustain
stub flickers just above the strike plate; otherwise bare rails. The left
meter is now fully red/filled. Expected RB3: by 14s into a song the highway
should have several gems in flight. Anomalies: **gem density has collapsed.**
This is the transition frame where the stream is dying. Severity:
SHOW_STOPPER (gem-stream attrition begins here).

### 04 — `04_f16000_03_songMs19k.png` (song ≈ 18.7 s)
First impression: completely empty highway. Rails, strike plate, beat-lines
all present and the beat-lines animate (frames are distinct), but no gems at
all. Expected RB3: this is well into the song; gems should be flowing.
Anomalies: **zero gems.** Severity: SHOW_STOPPER.

### 05 — `05_f20000_04_songMs24k.png` (song ≈ 23.6 s)
First impression: empty highway, identical anatomy to frame 04. Expected RB3:
**this is the densest part of the chart** (V8-C: ~6–7 gems/sec). It should be
a wall of gems. Anomalies: **zero gems in the exact region that should be the
busiest.** This is the decisive evidence frame for the gem-window bug.
Severity: SHOW_STOPPER.

### 06 — `06_f24000_05_songMs28k.png` (song ≈ 28.5 s)
First impression: empty highway, same as 04/05. Expected RB3: peak-density
gameplay. Anomalies: zero gems. Severity: SHOW_STOPPER.

**Distinctness note:** I also captured a tight burst of 10 frames spaced ~200
apart across song-time ~22–24s (frames 19000–20800). All 10 are md5-distinct
(the scene animates — beat-lines scroll), but **not one of the 10 contains a
gem.** This rules out "we just caught the unlucky gap between gems at 216 fps":
across ~1800 frames / ~2 seconds of the densest chart region, the highway is
continuously empty. The gem stream is genuinely absent there, not sampled-out.

---

## Step 2 — the gem-density question, settled

**Verdict: this is a genuine gem-window / transport bug, NOT intro-real
sparseness.** The evidence is the reverse of the "sparse intro" hypothesis:

- **Early (song ≈ 3.4–8.8 s): gems are PRESENT and flowing** — green→blue→red
  note-heads, sustain trails, and demonstrable frame-to-frame scroll. The
  mechanic works end to end here.
- **Deep (song ≈ 18.7–28.5 s): gems are ABSENT** — across ~2 continuous
  seconds of the chart's densest region (the burst), the highway is empty.

If "sparse" were intro-real, the deep frames would have MORE gems than the
intro. They have FEWER (zero). So the intro is not the problem — the intro is
actually the **best-looking** part of the run. Something stops feeding gems
into the visible window somewhere around song-time ~13–16s, and they never
return.

**Root-cause candidates (not confirmed — read-only pass; flagging for the
follow-up):**

- `GameGemList` and `GemTrackDir` are now fully compiled (the link-stubs file
  confirms their stubs were removed), so this is **not** a stubbed-TU issue.
  The list machinery is real.
- The `MidiParserMgr` ctor is **still a weak no-op stub** in
  `rb3/native/src/band3_link_stubs.s`
  (`_ZN13MidiParserMgrC1EP16GemListInterface6Symbol → __hmx_band3_noop_stub`).
  If the MIDI parser is what continuously feeds chart events into the gem
  window, a no-op parser ctor is a strong suspect for "a finite pre-seeded set
  of gems plays out, then nothing refills the window." This is the first place
  I'd look.
- Correlated symptom worth chasing: the left "crowd/energy" meter goes from
  empty (gray) at song ≈ 9s to **fully red/saturated** by ~14s and stays
  pinned — exactly as the gem stream dies. Whether that's the crowd-meter
  responding to "no gems being scored" or a shared upstream state going stale,
  the two events coincide in time and likely share a cause.

---

## Step 3 — comparison vs retail RB3 reference

Calibrating against retail RB3 guitar gameplay (5-lane GRYBO highway, gems
descending toward a strike line ~2/3 down, sustains as trailing tails,
vanishing point high-center, a lit venue/band behind the highway, numeric
score top-center, a colored multiplier ring, a star-power meter):

| Element | Retail RB3 | rb3-native V16 | Read |
|---|---|---|---|
| Highway framing | down-highway, fills center, high vanishing point | **matches** | WIN |
| Fret rails (GRYBO) | 5 colored lanes G/R/Y/B/O | **matches** (5 frets, correct order) | WIN |
| Strike plate | bottom, ~75% down, lane buttons + flares | **matches** (present, flares on hit-lane) | WIN |
| Gem note-heads | beveled colored prisms | present early, chart-colored; slightly chunky | mostly-right (early only) |
| Sustain trails | colored tails up the lane | **present and scrolling** (early) | WIN (early only) |
| Gem density mid-song | wall of gems at 6–7/sec | **EMPTY mid-song** | BROKEN |
| Venue / band / crowd | lit 3D stage behind highway | **black void** | missing (deferred) |
| Score (numeric) | top-center digits | absent (no `%d%%` leak now, but no value either) | missing |
| Multiplier ring | colored ring, left | a small disc icon by the left meter | partial |
| Star-power meter | top-of-highway fill | top-right pill gauge present | partial/on-model |
| Crowd/energy meter | left vertical | present, animates (fills red) | on-model |

So the **highway anatomy is retail-grade**; the **gem stream is retail-grade
for ~10 seconds then breaks**; the **surrounding stage (venue/band/crowd/score
digits) is still absent** (a black void behind everything), which is the
deferred Phase-2/venue work, not a regression.

Note also (log corroboration): the run loads `gem_smasher_real_guitar.milo`
and `_rg_string.mesh` real-guitar smasher assets alongside the standard
`tracksystem.milo` `gem_mash0..5` — but the on-screen strike plate is the
standard 5-fret GRYBO plate and the track is confirmed `TrackType=1`
(standard guitar), so the real-guitar assets are loaded-but-unused furniture,
not what's drawn.

---

## Authoritative recognizability verdict

The prior estimates bounced: V11 ~10%, V12 ~45%, V13 ~75% (flagged optimistic),
V14a ~50%, V15 ~65%. Reconciling them into one grounded number requires
splitting by song phase, because the run is genuinely bimodal:

- **Intro / early gameplay (song ≈ 0–10 s): ~70% recognizable.** A
  knowledgeable RB3 player shown frame 00a, 01, or 02 would say "yes, that's
  Rock Band guitar" without hesitation — correct framing, correct rails,
  correct strike plate, gems descending with sustains. It loses points only
  for the black void behind (no venue/band) and no score digits.
- **Mid/late gameplay (song ≈ 18 s+): ~35% recognizable.** Same player shown
  frame 04/05/06 would say "that's a Rock Band-style highway, but where are the
  notes? Is this a calibration screen or a paused/finished song?" The defining
  element — gems — is missing.

**Single honest blended number, weighted by how much of a real playthrough
each phase represents (the empty phase dominates a full song): ~50%.**

I am deliberately landing at **~50%, not the V15 ~65% or the V13 ~75%.** The
~65–75% numbers were sampled only in the intro window where gems are present;
they over-credited because they never looked deep. Now that the deep frames
are in hand and they're empty, the honest full-song read has to come down.
**~50%** is the number that survives looking at the whole song: a clear win on
highway anatomy and intro gems, dragged down by a gem stream that's absent for
the majority of the song's runtime and a still-black stage.

This is **not** a regression from V15 — it's the first number that was
measured against the deep song instead of the intro. The visual machinery V15
added is real; the V16 number is lower only because V16 is the first review
honest enough to capture where the chart is supposed to be dense.

### Recognizability test (would an RB3 fan ID this as RB3 gameplay?)

Counting the 8 captured frames (2 supplementary intro + 6 canonical):

- **Would ID as RB3 gameplay: 4 / 8** — 00a, 00b, 01, 02 (all the early/intro
  frames with gems). Strong, immediate yes.
- **Would ID as "an RB3-style highway but with no notes": 4 / 8** — 03, 04,
  05, 06 (the mid/deep frames). A fan would recognize the *furniture* as RB3
  but would not call it active gameplay.

So **4/8 read as live RB3 gameplay** — versus **0/8 in V11.** That is the real,
measured progress.

---

## Top 5 remaining gameplay-visual issues (ranked by impact)

1. **Gem stream dies mid-song (SHOW_STOPPER).** Gems flow through ~9s, collapse
   by ~14s, gone from ~18s on. The densest part of the chart renders an empty
   highway. This is the single highest-impact issue — it converts the back
   ~80% of every song from "RB3 gameplay" to "empty calibration highway."
   Prime suspect: the still-stubbed `MidiParserMgr` ctor (no-op) starving the
   gem window after an initial pre-seeded burst. Correlates with the
   crowd-meter pinning red at the same time.

2. **No venue / band / crowd / stage lighting (SHOW_STOPPER for atmosphere).**
   Pure-black background behind the highway on every frame. This is the biggest
   remaining "this isn't the real game" tell after the gem bug — retail RB3 has
   a lit 3D stage and animated band. Deferred Phase-2/venue work; tracked
   elsewhere, not a regression.

3. **No numeric score / multiplier value on screen (MAJOR).** The old `%d%%`
   format-string leak is gone (good — no debug string anymore), but there's no
   visible score digits or multiplier number in its place. The HUD furniture
   (star-power pill, crowd meter) is on-model but carries no live values.

4. **Gem note-head geometry is chunky / low-fidelity (MINOR).** When gems are
   present, the prisms read as slightly blocky vs retail's beveled gem prisms,
   and the strike-plate flare is a simple disc. Reads correctly, just not
   pixel-faithful. Low priority next to #1–#3.

5. **Real-guitar smasher assets loaded but unused (COSMETIC / cleanup).** The
   run loads `gem_smasher_real_guitar.milo` / `_rg_string.mesh` even though the
   track is standard `TrackType=1` guitar. Harmless to the visual, but it's
   wasted load and a sign the track-type→asset selection isn't fully pruned.

---

## What now reads unmistakably as Rock Band 3 (the wins)

- **The down-the-highway camera.** This is THE fix that mattered. V11's camera
  pointed at a nameplate card with the real track edge-on off-screen; V16 looks
  straight down a centered highway with a high vanishing point. One change,
  "debug harness" → "Rock Band."
- **The 5-color GRYBO fret rails + strike plate.** Correct lane count, correct
  color order, correct strike-plate position ~75% down the frame. Textbook RB3
  fretboard.
- **Gems descending with sustain trails (in the intro).** Colored note-heads
  arriving at the strike plate, sustain tails running up the lanes, and they
  **scroll frame-to-frame** with the clock. When present, this is the real
  thing.
- **Chart-driven gem colors.** Green/blue in the very-early phrase, red by
  ~9s — the gem colors track the actual chart, not a fixed pattern.
- **The HUD furniture is on-model.** Top-right star-power/overdrive pill, left
  vertical crowd/energy meter that animates (fills red). RB3 fans recognize
  these as RB3 HUD, even though they don't carry live numeric values yet.
- **Real audio + healthy transport clock.** `streamPlaying=1`, `audioTime`
  tracks `songMs` within ~5ms — the song actually plays and the visual clock is
  slaved to it correctly. (This also means the gem-stream bug is NOT a clock
  bug.)

---

## Honest bottom line

**Would a knowledgeable observer recognize this as Rock Band 3 guitar
gameplay? Partly — and it depends entirely on which second of the song you
freeze.**

- In the first ~10 seconds: **yes, immediately.** Centered GRYBO highway, gems
  descending with sustains, strike plate, animated HUD. That's Rock Band 3.
- After ~18 seconds (the majority of any song, including the densest part of
  this chart): **no** — they'd see a correct-looking but **empty** highway and
  ask where the notes went.

**At what % of frames would a fan ID it as live RB3 gameplay? About half of a
short capture window (4/8 here), but a much smaller fraction of a *full song*,**
because the empty phase runs from ~18s to the end while the gem-bearing phase
is only the first ~10–14s.

**Honest full-song recognizability: ~50%.** This is a real, large step up from
V11's ~10% (the camera and gem-descent fixes genuinely landed), but it is below
the V15 ~65% precisely because V16 is the first review to look where the chart
is supposed to be dense — and found it empty. The path from ~50% to ~80% is
clear and singular: **make the gem stream survive past ~14s** (start with the
stubbed `MidiParserMgr`). Fix that one thing and most frames of a full song
flip from "empty highway" to "RB3 gameplay," and the next ceiling becomes the
black-void stage (venue/band) rather than the gems.

---

## Progression V11→V16

(Read the V11 review last, for this diff. V12/V13/V14/V15 dirs contain PNGs but
no written REVIEW_OPUS, so V11 is the only prior prose to diff against.)

**V11 (~10%):** Camera pointed at the wrong object. A tri-color "Will Name"
nameplate card dominated center; the real gem highway was edge-on off the right
margin; the play-area was byte-frozen across 5600 frames; only the left meter
animated; a `%d%%` debug format string sat in the HUD. 0/8 frames read as RB3
gameplay. The review correctly called camera framing the single most important
fix.

**V12 (camera-fix):** The first visible payoff — the camera was re-aimed down
the highway (PNGs in `v12-camera-fix/` show the centered down-highway view
appearing). This is the fix that moved the needle most.

**V13 (gems-descending):** Gems began descending the now-correctly-framed
highway (PNGs in `v13-gems-descending/`). The descent mechanic appears.

**V14 (smasher + score):** Strike-plate / "smasher" work and score plumbing
(`v14-smasher/`, `v14-score/`).

**V15 (smasher-draw):** The strike plate is drawn with the full 5-fret GRYBO
buttons and flares; sustain trails render. The `v15-smasher-draw/` frames show
the complete highway anatomy the V11 review said was entirely absent.

**V16 (this review):** Confirms the cumulative V12→V15 wins are real — and adds
the one thing every prior pass missed by sampling only the intro: **deep-song
capture reveals the gem stream dies by ~18s.** So V16's contribution is
diagnostic, not cosmetic: it reframes the remaining gameplay problem from
"polish the gems" to "**the gems stop coming mid-song**," and identifies the
still-stubbed `MidiParserMgr` ctor as the prime suspect.

What each fix visibly added, in one line each:
- **V12 → camera:** black void with off-screen track → centered down-highway view.
- **V13 → descent:** static highway → gems moving down it.
- **V14 → smasher/score:** strike-plate machinery + score plumbing.
- **V15 → smasher-draw:** the strike plate and sustain trails actually render —
  complete highway anatomy on screen.
- **V16 → deep capture:** proves the anatomy + intro gems are real, and exposes
  the mid-song gem-stream collapse that the intro-only sampling had hidden.

The honest trend line, recalibrated against deep-song capture:
~10% (V11, mis-framed) → ~50% (V16, correct frame + working intro gems, but
empty mid/late song). The next inflection is entirely gated on keeping gems on
the highway past ~14s.

---

## Method note / data backing the claims

- Frame↔song-time map measured by interleaving the GAME_DBG `songMs=` log
  (Game::Poll, every 60 polls) with the `BandRnd: screenshot N` capture log in
  the same run: frame 4000→3.9s, 8000→8.8s, 12000→13.8s, 16000→18.7s,
  20000→23.6s, 24000→28.5s. `audioTime` tracks `songMs` within ~5ms and
  `streamPlaying=1` throughout — real audio, healthy clock.
- Canonical captures: `01_f4000` … `06_f24000` in this dir (one intro + the
  deep ladder), plus `00a_f3600` / `00b_f4400` supplementary intro frames
  showing the gem stream most clearly.
- Empty-deep-region confirmation: a separate 10-frame burst at frames
  19000–20800 (song ≈ 22–24s, spaced ~200 apart) — all 10 md5-distinct (scene
  animates), **none** containing a gem — rules out 216-fps aliasing past a
  momentary gap.
- Track type: `RB3 input: track set 'guitar' -> TrackType=1 diff=expert`
  (standard 5-fret guitar, Expert).
- Root-cause leads (not confirmed): `GameGemList`/`GemTrackDir` are strongly
  compiled (stubs removed per `band3_link_stubs.s`); `MidiParserMgr` ctor is
  still a weak no-op stub in the same file — prime suspect for the mid-song gem
  starvation.

Sources for retail calibration:
- [Rock Band 3 — Wikipedia](https://en.wikipedia.org/wiki/Rock_Band_3)
- [Rock Band 3 | Rock Band Wiki | Fandom](https://rockband.fandom.com/wiki/Rock_Band_3)
</content>
</invoke>
