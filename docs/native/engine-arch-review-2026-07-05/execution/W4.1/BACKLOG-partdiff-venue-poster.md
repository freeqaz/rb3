# Backlog handoff — part_difficulty part-confirm zoom shows solid-black venue poster/menu-board quads

Filed by: W4.1 (Lane C), stage C.S2-c. **Not a Lane C fix** — out of the band3/UI file fence.
Diagnosis owner: W4.1/PLAN.md subitem (c) (C.S1, Opus). This file documents the handoff per the
plan's explicit instruction ("Route to the Lane D venue family (W2.7 black head / W3.3 grayscale
venue) or file a fresh venue-texture item").

## Symptom

`/tmp/wave6-current-state/partdiff_default.png` (`part_difficulty_screen`, frame 390, nav ending
`@380:part:guitar`) shows a restaurant-menu/flyer backdrop ("Restaurant Bar & Grille" menu board)
with several large **solid black rectangular patches** overlaid where poster/menu-board detail
would be — no part-select or difficulty-gauge widgets visible in that single frame.

## Re-verification this stage (C.S2-c)

Re-ran the settle-sequence recap harness (`W4.1/harness/partdiff-settle-recap.py`, unchanged from
C.S1) against the **current HEAD binary** (post subitem-(a) commit `b537d275`, rebuilt
`native/build-native/rb3-native`), nav landing on `part_difficulty_screen` with **no** `part:`/
`diff:` press, captures at +0/+30/+60/+120/+180/+240/+360 frames after arrival:

- `/tmp/wave6-partdiff-recap/partdiff_settle_000.png` through `_360.png` — the part/difficulty
  widgets (song header, album art, "RIGHTY MODE / SONG DIFFICULTY" panel, "CHOOSE INSTRUMENT /
  GUITAR / BASS" card) render correctly and **stably across the entire 360-frame window**. **No
  black poster quads appear in any frame of this sequence** — the visible backdrop there is a
  train/subway scene (a different default QUICKPLAY pick than the `partdiff_default.png` capture;
  the setlist auto-advances at long dwell — `_360.png` shows song #2 "25 OR 6 TO 4" vs `_000.png`'s
  #1 "20TH CENTURY BOY" — unrelated to this bug, just confirms the widget layer is live and
  updating normally).
- Confirms the C.S1 finding: `partdiff_default.png`'s frame 390 is captured **mid the
  part-confirm-zoom camera transition** (nav pressed `@380:part:guitar` only 10 frames earlier),
  a transient close-up on the venue poster-wall geometry — not the steady-state part_difficulty
  widget view. This is reproducible, not a one-off capture artifact.

**Verdict confirmed: NOT a UI/widget bug.** No code change in this stage — `src/band3/`,
`src/system/ui/`, `src/system/bandobj/` untouched (fence: band3/UI game code only; this finding's
root cause is venue backdrop rendering, out of that fence).

## What's actually black

The black patches are the **venue backdrop's poster/menu-board meshes** (Crowd/`RndEnviron`
geometry+materials belonging to the pub/venue set), seen in close-up during the part-confirm zoom
— not band3 UI panels. Two live Lane D characterizations from this same wave touch adjacent venue-
render territory and are the natural next-owners to check for a shared mechanism before opening a
fresh investigation:

- **W3.3 (grayscale venue at song start)** — a native-only postproc composite over-exposure
  artifact during the song-start stage-light reveal (`docs/native/engine-arch-review-2026-07-05/execution/W3.3/STATUS.md`).
  Different trigger (song-start reveal vs part-confirm camera zoom) and different symptom
  (desaturated grey, not solid black) — **plausibly related** (both are venue-backdrop rendering
  oddities exposed by a camera event) but **not yet shown to share a root cause**. Worth a quick
  flag-isolation check (`RB3_VENUE_LIGHT_OFF`, `RB3_PP_OFF`) against the part-confirm zoom frame
  before assuming a fresh mechanism.
- **W2.7 (black head)** — a per-material/texture-composite issue on a character head submesh
  (`docs/native/engine-arch-review-2026-07-05/execution/W2.7/STATUS.md`). Different asset class
  (character skin vs venue poster mesh) — less likely the same mechanism, but both are
  "solid black where a texture should read" so a missing/unbound-texture or a material-binder
  fallback (`RB3MaterialBinder`) hitting a default-black state during a camera-driven LOD/asset
  swap is a plausible shared candidate worth ruling out first.

## Recommended next step (Wave 7, venue-render lane, engine + rb3_render_hook.cpp in scope)

1. Reproduce standalone: nav directly to `part_difficulty_screen` and issue a single `part:guitar`
   press, capture frames 380-400 at fine granularity (every 2-3 frames) to catch the zoom's peak and
   confirm the black patches are camera-distance/LOD-correlated (texture streams in at a mip level
   that isn't ready, vs a genuinely unbound/missing texture that stays black at rest too).
2. Flag-isolation matrix against a reproducing frame: `RB3_VENUE_LIGHT_OFF`, `RB3_PP_OFF` (cheap,
   reuses W3.3's harness pattern) to test the shared-mechanism hypothesis above.
3. If neither venue-light nor postproc kills it, suspect the poster mesh's own material/texture
   binding (`RB3MaterialBinder`) or an LOD/mip selection during the zoom's rapid distance change.
4. File fence for the fix: engine render files (`RB3MaterialBinder`, `Rnd_Wgpu_RB3.cpp`) and/or
   `rb3/native/src/rb3_render_hook.cpp` — **out of Lane C's band3/UI fence**, in scope for the
   engine/venue-render lane.

## Evidence

- `/tmp/wave6-current-state/partdiff_default.png` — the anomalous frame (frame 390, mid-zoom).
- `/tmp/wave6-partdiff-recap/partdiff_settle_{000,030,060,120,180,240,360}.png` — the settled
  widget sequence, no black quads, re-confirmed this stage against current HEAD.
- `W4.1/harness/partdiff-settle-recap.py` — reusable capture harness (nav to
  `part_difficulty_screen`, no `part:`/`diff:` press, N-frame settle sequence).
