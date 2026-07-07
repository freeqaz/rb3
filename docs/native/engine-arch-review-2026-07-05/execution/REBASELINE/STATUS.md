# Wave 9 — Stage C.S1 (Lane C): Current-State Re-baseline Sweep — STATUS

**Owner**: Lane C (Sonnet, capture-only). **Scope**: capture + compare only —
no source edits, no engine/game commits, no diagnosis beyond what's directly
visible on screen.

**Build**: HEAD defaults, plain `rb3-native` build (no lane env overrides).
Engine consumed live via `add_subdirectory` against `../../milo-native-engine`
(current checkout HEAD `a320f9d`); a stale `MILO_ENGINE_PIN` cache-string
warning appeared at configure time but does not affect the compiled binary
(confirmed `ninja: no work to do` — binary already current). All Wave-9
default-ON flags (placement contract, black-head fix, UI text floor relaxed,
hub-quad-hide, chroma-preserve composite) left at their shipped defaults;
refuted flags (`RB3_PP_LUMA_CEILING`, `RB3_HANDS_POSEAWARE`,
`RB3_HANDS_PERFRAME_CONJ`) left unset.

**Captures**: 21 PNGs across main_hub, song_select, part_difficulty (7-frame
settle sequence per A6), and gameplay early/mid/late/band-wide across N=3
independent boots (A6 director-shot-RNG protocol). Full manifest with
per-file notes: `/tmp/wave9-current-state/MANIFEST.md`. Raw captures live in
`/tmp/` only (not committed, per task scope — "not all raws").

## (a) Confirmed-fixed-on-screen items

Directly compared against the archived Wave-6 baseline
(`execution/baselines/wave6-current-state/`):

- **Main-hub grey ticker-quad artifact — GONE.** Wave-6's `mainhub_default.png`
  shows a grey rectangular quad over the hub background; Wave-9's
  `mainhub_default.png` does not. Consistent with the hub-quad-hide fix
  (W4.1, default-ON, opt-out `RB3_HUB_MENU_QUAD_OFF`).
- **Main-hub / menu text legibility — FIXED.** PLAY NOW / QUICKPLAY / START A
  ROAD CHALLENGE render crisp in Wave-9 vs visibly blurred/clipped in Wave-6.
  Consistent with the UI text floor relaxation (W4.2).
- **part_difficulty frame-390-style "anomaly" — RECONFIRMED NOT A BUG.** My
  own independent 7-frame settle sequence (offsets +0..+360 after arrival, no
  `part:`/`diff:` press) reproduces the same odd mid-transition framing at
  early offsets, which fully resolves by +240/+360. This independently
  corroborates W4.1's prior conclusion using a fresh capture, not a re-read
  of the old evidence.
- **Black-head bug (Wave-6 `gameplay_default_2`) — NOT observed.** Checked
  across all 6 mid/band-wide gameplay frames spanning 3 independent boots;
  no black/missing head texture in any of them. Consistent with the
  black-head fix holding under repeated random camera framing.

See `montage_mainhub.png`, `montage_partdiff.png`,
`montage_gameplay_blackhead.png` for the visual evidence.

## (b) NEW anomalies (not on the known-open list)

**Disconnected/floating forearm-and-hand mesh piece**, observed in two
independent contexts:

1. During the `part_difficulty_screen` transition-wipe settle sequence: at
   +060 and +120 frames after arrival, a forearm+hand mesh renders mid-screen
   with no attached body visible (the diagonal wipe mask hasn't yet revealed
   the rest of the character). It is absent at +000 and gone/resolved by the
   fully-settled +360 frame. See `montages/montage_partdiff_floatinghand.png`
   and the zoomed crop `/tmp/partdiff_120_crop.png`.
2. In `gameplay_late_run2.png` (one of 3 independent gameplay boots): a fully
   disembodied forearm/hand hangs from the ceiling, top-left of frame, during
   a singer close-up shot, with no visible attachment to any character body
   in frame. Crop: `/tmp/gameplay_late_run2_crop.png`.

I am **not** diagnosing root cause (out of Lane C scope) but flagging two
competing, undecided hypotheses for the coordinator / Lane A:

- **(H1) Genuine detached-mesh/skinning bug** — possibly the same underlying
  issue as the known finger/hand-shard family (W2.8d, "rotation basis wrong,
  translation correct") but manifesting as a *fully separated* limb rather
  than a shattered/radiating shard on an otherwise-attached hand. If so, this
  may be worth folding into Lane A's bone-level attribution work rather than
  filing as a wholly separate bug.
- **(H2) Transition-wipe visual illusion, not a real detachment** — the
  `part_difficulty` case in particular occurs while the diagonal tv3_a-style
  wipe mask is still progressively revealing the frame; a walking/swinging
  arm could appear "detached" simply because the mask hasn't yet revealed the
  torso it's attached to. W5.1 previously found "not a bug" for a similarly-
  shaped illusion involving this same transition vignette. The gameplay_late
  case (no wipe active, straight camera shot) argues against this hypothesis
  for occurrence #2, but I have not ruled out a similar off-camera occlusion
  effect (e.g. limb attached to a character standing outside the visible
  frame, appearing to hang unattached from the ceiling due to perspective).

Recommend the coordinator route this to whichever of Lane A (hands
attribution) or a fresh triage decides which hypothesis holds — I have
deliberately not tried to resolve it myself, per Lane C's capture-only scope.

No other new anomalies were found. All other deviations from the Wave-6
baseline are on the known-open list (finger/hand shard, engaged-venue WHITE
over-exposure, grey/green character skin) and are not re-filed here.

## (c) Representative montages for coordinator review

All in `/tmp/wave9-current-state/montages/` (committing copies alongside this
file — see below):

| Montage | What it shows |
|---|---|
| `montage_mainhub.png` | Wave-6 vs Wave-9 main_hub — quad-hide + text-floor fixes |
| `montage_songselect.png` | Wave-6 vs Wave-9 song_select — no change, sanity check |
| `montage_partdiff.png` | Wave-6 anomalous frame-390 vs Wave-9 fully-settled part_difficulty |
| `montage_partdiff_floatinghand.png` | Wave-9 +060 vs +120 settle frames — **NEW finding, floating forearm** |
| `montage_gameplay_early.png` | Wave-9 gameplay_early run1 (clean) vs run3 (WHITE over-exposure, known-open corroboration) |
| `montage_gameplay_blackhead.png` | Wave-6 black-head anomaly vs Wave-9 mid_run1/run2 (clean) — fix holding |
| `montage_gameplay_band.png` | Wave-9 gameplay_band run1/run2/run3 — director-shot RNG demonstrated (A6) |

Full per-file capture notes: `/tmp/wave9-current-state/MANIFEST.md` (not
committed — raw captures + full manifest remain in `/tmp` per task scope).
Capture harness (for reproducibility, not committed):
`execution/REBASELINE/harness/rebaseline-capture.py`.
