# W32-F7-CLIP-DIAG — plan

Rider (Sonnet, diagnosis-only, NO code changes). Song-select right-edge
clipping: backdrop character's thighs clip in at the right edge AND show
through the sidebar rows.

## Questions (from kickoff rider)

1. What clips the character — camera frustum / scissor / viewport /
   authored placement?
2. Does RETAIL have an opaque panel behind the per-song sidebar column, i.e.
   is our gap a missing panel draw or a missing depth/stencil mask?

## Method

- Evidence for this checkpoint was already on disk under `evidence/` from an
  earlier invocation of this same lane (song-select-capture.py style boot to
  `song_select_screen` with a REAL SONG ROW focused — "20th Century Boy",
  not a heading, per the W31 heading-row trap) at matched frame ~554-561:
  full-screen PNG, right-edge crop PNGs, `/api/uidump` (full + filtered),
  `/api/drawlog` (full 351 draws + a right-edge ROI query), and a dead-end
  `/api/dta/eval` camera probe.
- This session: read the evidence, cross-reference source
  (`src/system/rndobj/ScreenMask.h/.cpp`, `src/system/rndobj/Env.h`,
  `src/system/rndobj/Group.h`, `src/system/rndobj/Draw.h`,
  `native/src/rb3_uidump.cpp`) to validate which uidump fields are trustworthy
  oracles (`showing` is only valid for `RndDrawable`-derived classes — NOT
  `Environ`, which extends `Hmx::Object` directly) and to name the exact
  mesh/material/group responsible.
- Compared against `images/retail-screenshots/yt_qRagnZCIMzk_song_select_{diff_ratings,album_art}.png`.
- No code changes; no builds required beyond what the prior evidence capture
  already ran.

## Deliverable

Mechanism memo + candidate fix surface + W33 charter draft, this file's
sibling `STATUS.md`.
