# verify-gems — wave-3 composed-build verification (gem-polish: tails / colors / flicker / held)

Wave-3 verifier, 2026-06-11. Judged on the COMPOSED build: rb3 master `c011c886`
(pin bump `cca1869a`), engine pin `469c550`, binary
`native/build-native/rb3-native` (built Jun 11 21:00). Read-only verification;
all runs headless via the scout's `gem_probe.py` + a by-shortname variant
(`/tmp/rp3-gems/song2_probe.py`). Evidence under `/tmp/rp3-gems/`. Ports 8821-8829,
all instances cleaned up.

## VERDICT: PASS on all four assigned sub-checks, with ONE NEW wave-2 regression
found in passing (held-fret glow renders as a giant white sphere occluding gems —
attribution proven, see "New issue" below).

| sub-check | verdict | decisive evidence |
|---|---|---|
| (1) approach tails | **PASS** | `runA-default-early/a_012.png` (19297ms, default env: 3 colored Y/B/O tails to track top) ≈ scout's cache-off truth `nm7`; cross-song + cross-lane confirmed |
| (2) colors + halo | **PASS** | default ≈ bloom-off saturation (body RGB identical to ±2); halo measurably present + `[RB3_HALOBLOOM] … blend=0.70 halo=1` fires |
| (3) flicker re-triage | **PASS** | 160 consecutive engine frames: 0 gem-dropout frames, 0 full venue blackouts (wave-1 class GONE, incl. the exact 67.3-68.9s window) |
| (4) held sustain | **PASS** (whammy visual untestable headless) | tails render held; tail-top descends smoothly EVERY frame (vert-refresh fixed) |

---

## (1) Approach tails — PASS

Protocol: Antibodies (`--song-downs 4`), expert guitar, jump 15500, default env
(no cache opt-outs), autohit. The documented 19.68s Y/B/O 1.44s chord.

- `a_012.png` (songMs 19297) / `a_013.png` (19592): three colored tails (yellow,
  blue, orange) extend from the approaching chord gems to the top of the track —
  the EXACT window that was empty pre-fix (`/tmp/rp-gem-polish/ea_c012_approach_NO_tails_baseline.png`).
  Visual match to the cache-off truth `nm7_approach_TAILS_meshcache_off.png`
  (its blinding-white gem caps are the OLD bloom wash, separately fixed).
  Crops: `zoom_A12.png`, `A12tight.png`, `A13tight.png`.
- Other lanes, no autohit (`runD-noauto-69s`, jump 64000): B/G/R chord (69122ms)
  approach tails present + colored at 68909 (`D13big.png`: green/salmon/cyan on
  correct lanes); R/Y/O chord (70082ms) tails present + correct
  (programmatic stripe scan of `d_017.png` @ y=450: R=(255,146,136) @x563-600,
  Y=(255,255,124) @x621-658, O=(255,232,124) @x737-772 — lane-x positions match
  the smasher row at y=600).
- DIFFERENT SONG (adversarial): 20th Century Boy via shortname targeting
  (`runE-song2`): the 53856ms 2.98s B+G sustain shows approach tails at 53202
  (654ms out, `E10top.png`). Tails/gems in that window are WHITE — verified
  retail-CORRECT styling, not a bug: MIDI note-116 parse puts an overdrive phrase
  at 51878-53966ms exactly covering it (same for Antibodies 76809-77890 — the
  wide white tails at 76.5s in run D are OD styling too; `midi_sustains.py`).
- A false alarm self-debunked: a "black tail with white rails" in low-res crops
  (`D17big.png`) is the dark unoccupied B lane between two bright tails + their
  edge highlights — the stripe scan above proves no black tail exists.

## (2) Colors + bloom halo — PASS

- **Body wash gone**: held-tail horizontal profile (y=400, x500-800, matched
  songMs 20487 vs 20419) — tail-body luminance buckets IDENTICAL default vs
  `RB3_HIGHWAY_BLOOM_OFF=1` (180/243/242 vs 182/243/242). Approach-window top-2%
  brightest highway pixels: default sat 0.13 vs bloom-off 0.14 (pre-fix default
  was the outlier). BEFORE truth re-captured on a pre-wave-2 binary
  (task-highway-offset worktree, engine 8fb669d): gems at the now-bar are
  featureless BLOWN-WHITE rectangles (`Fblob06.png`) — that class is gone.
- **Per-lane saturation** ≈ retail: hue clusters in gameplay frames —
  G=(83,166,94)/(54,137,24), R=(166,59,59), Y=(198,177,53), B=(58,114,174),
  O=(147,100,3); avg sat 0.5-0.98, comparable to retail fandom ref clusters
  (sat 0.5-0.77). Visual: `A24z.png`/`A26z.png` (sat. green/blue/yellow gems),
  `E21*` (green/blue post-OD).
- **Halo still present** (it should not vanish): now-bar side-ring luminance
  default-minus-bloomoff = +9.7 (left) / +5.6 (right); inter-tail gaps +4-5.
  Positive control: `RB3_RENDER_DBG=1` run (`runI-renderdbg/engine-8829.log`):
  63× `[RB3_HALOBLOOM] … draws=3 thresh=0.55 blend=0.70 halo=1` during gameplay.
  The halo is SUBTLE — if more glow is wanted, `RB3_HIGHWAY_BLOOM_BLEND` is live.

## (3) Flicker re-triage — PASS (user-symptom class not reproducible)

`runC-flicker`: 160 consecutive engine frames (headless ≈2.8fps ⇒ every rendered
frame captured), songMs 60283→115461, autohit, default env.

- **Gem/tail dropouts: ZERO.** Per-frame bright-blob count vs MIDI-expected
  on-screen notes (lookahead 1.5s): no frame with expected≥1 and blobs=0; no
  anomalous collapse of track bright-pixel counts between populated neighbors.
- **Wave-1 venue blackouts: GONE.** 0/160 frames with both venue crops
  (60,300,300,560)+(1000,300,1240,560) < 6 luminance. The scout's exact blackout
  window (songMs 67574/67944/68872) re-covered at i=20-24: venue luminance
  normal (41.9-45.5 / 28.9-30.7).
- Residual dark-venue frames are ATTRIBUTABLE, not flicker: f_091/f_092
  (92502/92823) = one camera shot whose right half is genuinely dark (stable
  across both frames, recovers on the CUT at f_093, `F091small.png`); f_104/f_106
  (96271/96862) = close-up shots with a dark character mass filling the right
  crop. Track+gems stay correct in all of them.

## (4) Held-sustain behavior — PASS (whammy = headless-untestable)

- Held tails render (a_014-018; colored bodies + bright whitish cores — cores are
  NOT bloom: identical with bloom off, b_015).
- **Vert-refresh fixed** (the path the whammy wiggle uses): held-tail top y
  descends EVERY frame — 317→317(horizon-clamped)→338→384→452→552 over
  consecutive frames to the sustain end at 21120ms. Pre-fix this froze between
  section-count boundaries.
- Whammy visual itself: NOT verifiable headless — the HTTP API has no whammy/axis
  verb (whammy is GLFW Space / gamepad RT only, `rb3_joypad_native.cpp:524-531`).
  Residual: needs a windowed/manual check or a new `/api/input` axis verb.

---

## NEW ISSUE (wave-2 regression): held-fret smasher glow = giant white sphere

A large shaded WHITE BALL (~110px wide at 1280x720, bigger than a gem) hovers on
the highway just above the now-bar, occluding gems behind it. Money shot:
`Eblob05.png` (sphere occluding a blue gem); A/B: `EVIDENCE_sphere_AB.png`
(top=default, bottom=`RB3_FRET_GLOW_OFF=1`, same window).

Four-way attribution (20th Century Boy, jump 49500, B-heavy chord window):

| run | env/binary | sphere frames |
|---|---|---|
| E | composed build, autohit | ~7+/24 early-window frames, fixed pos ≈(700,510), pulsing 3.4k→10.9kpx |
| G | composed build, NO autohit | 0 (one marginal hit = white OD gem at the bar) |
| H | composed build, autohit, `RB3_FRET_GLOW_OFF=1` | **0/16** |
| F | PRE-wave-2 binary (task-highway-offset worktree, engine 8fb669d) | 0 (its round-blob hits are the old white-washed gems) |

⇒ It is the wave-2 held-fret glow (engine `8874e77`: white-tint emissive fallback
for black-base mats + the pre-existing ×2 multiplier on `gem_smasher_glow.mat`).
In this song/venue the glow renders WHITE (not the per-slot color the impl doc
reports) and far oversized, and pulses on every autohit hit/hold of the B lane.
It also plausibly contributes to the extra white mass at the now-bar during
Antibodies holds (a_014-016). Suspects for the white/oversize: per-slot
`square_smasher_bright_*.tex` not bound in this venue path → fallback tint
dominates; interplay with `7acc22a` (emissive on all cameras) or venue lighting.
Follow-up should re-verify the fret-held feature per-venue and tune scale/color;
the opt-out `RB3_FRET_GLOW_OFF=1` is a clean mitigation.

## Other residuals / passing observations

- **Miss-state tails stay colored** after crossing the now-bar un-hit
  (d_015-017). Retail truth unknown (the scout's requested "gray tail_miss" ref
  was never delivered) — flagging as unadjudicated, not a fail.
- **Venue red-wash**: run A's 15.8-21s window renders the whole venue saturated
  deep red while run B (separate boot) showed a detailed lit venue in the same
  songMs window (`a_012` vs `b_012` backgrounds). Already queued in PLAN as the
  "venue pink-wash adjudication" follow-up; camera-shot desync between runs makes
  cross-run attribution unsafe — noting only.
- Tiny green specks on the orange held tail in `blobA15.png` — possible sparkle
  FX artifact, cosmetic, low priority.
- Approach-tail dim-alpha calibration vs retail remains uncalibrated (scout's
  requested approach-tail close-up retail ref still missing); current look is
  plausible (dim colored tube).

## Evidence index (`/tmp/rp3-gems/`)

- runA-default-early/ (48f, 15.8-30.9s) + runB-bloomoff-early/ (48f) — tails/colors A/B
- runC-flicker/ (160f, 60.3-115.5s) — flicker scan (+ `runC.log`)
- runD-noauto-69s/ (60f) — other-lane approach + miss state, no aids
- runE-song2/ (40f, 20thcenturyboy 50-63s) — second song + sphere discovery
- runF-old-song2/ (24f, PRE-wave-2 binary) — sphere absent + old white-wash BEFORE
- runG-noauto-song2/ (20f) + runH-glowoff-song2/ (16f) — sphere gating A/Bs
- runI-renderdbg/ — HALOBLOOM positive control
- Key stills: zoom_A12/A13, A12tight/A13tight/B12tight, A16held/B15held,
  A24z/A26z, D13big/D16big/D17big, E10top, Eblob05, EVIDENCE_sphere_AB,
  Fblob06, F091small/F092small/F104small/F106small
- Tools: `midi_sustains.py` (SMF sustain/OD parser), `song2_probe.py`
  (by-shortname gameplay probe; `song2_probe_bin.py` adds `RB3_BIN` override)
