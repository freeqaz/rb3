# W4.3-C2b-ASM PLAN — Wave 14, Lane A (album-art whole-assembly fix)

## Task (per WAVE14_KICKOFF.md, binding acceptance A7)

Coordinator E1 held the W4.3-C2b4 `RB3_SS_ART_YFIX` fix (a -120u local-Z nudge
on `album_art.grp`): it clears the header-row overlap but reveals a grey
ornate bezel/frame element that does not move with the group, plus a new
overlap with the left-column score/star readout.

A7 refuted the C2b4 STATUS's "both are children of album_art.grp" framing
from source: dirty propagation to trans-children works fine
(`Trans.cpp:99-107,127-140`); the revealed element must be identified by
TransParent-chain evidence, not assumed to be a non-propagating child.

Three tasks:
1. Identify the revealed element and why it sits there.
2. Re-implement the fix as one whole-assembly move (extending
   `RB3_SS_ART_YFIX`).
3. Fix the left-column overlap (X component or smaller Z), calibrated
   against `images/retail-screenshots/yt_qRagnZCIMzk_song_select_diff_ratings.png`.

Scope: game-side only, no engine TU edits.

## Approach

1. Build isolated agent build dir (`native/build-agent-W4.3-C2b-ASM`,
   Clang + `CMAKE_BUILD_TYPE=Debug` — Release optimizes away the
   `TimeConversion.cpp` out-of-line `inline` symbols other TUs need).
2. Add a `RB3_C2B_ASM_DBG` diagnostic to `SongSelectPanel::FinishLoad()`
   that walks `RndTransformable::TransParent()` chains (not `RndGroup`
   draw-membership) and dumps `WorldXfm()` for every candidate revealed-frame
   element, plus a `RB3_C2B_ASM_HIDE=<name>` probe to force-hide one node by
   name for empirical A/B confirmation.
3. Identify the element, confirm by hide-test, and measure the actual
   world-space delta the -120 fix produces on `album_art.grp` vs. what
   would be needed on the true element to move it in lockstep.
4. Add a dual-node calibration probe (`RB3_C2B_ART_NUDGE=<art_x>,<art_z>,
   <frame_x>,<frame_y>,<frame_z>`), sweep values, screenshot each candidate
   (frame-count-settled, not wall-clock), settle on final calibration.
5. Fold the calibrated values into the production `RB3_SS_ART_YFIX` block,
   rewrite the stale comment block to reflect the corrected mechanism.
6. Gates: E1 full-screen captures (flag-off / flag-on), drawlog-792 flag-off
   unchanged, classification.json update (extending the existing entry,
   not adding a new flag).

## Files touched
- `src/band3/meta_band/SongSelectPanel.cpp` (rb3) — fix + diagnostics.
- `src/platform/NativeCompatFlags.classification.json` (milo-native-engine)
  — updated `RB3_SS_ART_YFIX`'s `faithfulStatus` in place (surgical
  single-string replace, no reformatting of the rest of the file).
- This directory — PLAN.md, STATUS.md, evidence screenshots.
