# W32-F7-CLIP-DIAG STATUS — song-select right-edge clip / sidebar bleed-through

Rider (Sonnet, diagnosis-only, NO code changes). Checkpoint:
`/tmp/wave32-checkpoints/W32-F7-CLIP.json`. Claims file:
`/tmp/wave32-claims/W32-F7-CLIP.txt` — empty (this lane owns no TUs, no
writes). Base SHA `30546499` (W32 coordinator acceptance commit).

## Evidence base

All evidence was already on disk under `evidence/` from an earlier
invocation of this lane (native `song-select-capture.py`-style boot to
`song_select_screen` with a REAL SONG ROW focused — "20th Century Boy",
not a heading, per the W31 heading-row trap), captured at matched frames
~554-561: full-screen PNG (`c_realsong_full.png`), right-edge crop
(`c_crop_rightedge.png`), sidebar-overlap crop (`c_crop_sidebar_overlap.png`),
`/api/uidump` full (`c_uidump_full.json`) + filtered probes (dead ends, see
below), `/api/drawlog` full 351-draw dump (`c_drawlog_full.json`) and a
right-edge ROI query (`c_drawlog_roi_rightedge.json`), and a dead-end
`/api/dta/eval` camera probe (`c_camera_dta.json`). This session re-verified
and cross-referenced everything against source
(`src/band3/meta_band/SongSelectPanel.cpp`, `src/system/rndobj/ScreenMask.h`,
`native/src/rb3_uidump.cpp`, `.../RB3DrawLogDebug.h` in the engine repo,
read-only) and against `images/retail-screenshots/`. No builds run this
session (read existing evidence + source + docs only); no code written.

**Prior work found and reused, not re-derived:** `execution/W4.3-C2/STATUS.md`
(Wave 12) and `execution/W4.3-C2a/STATUS.md` (Wave 13) already diagnosed the
"missing sidebar backing" half of this exact defect in exhaustive, evidenced
detail. This STATUS confirms their conclusion still holds against fresh W32
evidence, adds the previously-undiscovered full-screen-dimmer mechanism that
explains *why* the left list column looks fine while the right sidebar
doesn't, and answers rider question (1) (clip mechanism) that C2/C2a didn't
address.

## Dead ends (already run, not re-run)

- `c_camera_dta.json`: `{{music_library get_cur_cam}}` → `0`,
  `{{{music_library get_cur_panel} get_cam}}` → `0`, `{game get_gfx_cam}` →
  `null`. No usable camera object via DTA eval on this panel — dead end,
  consistent with the PLAN.md note.
- The four filtered `/api/uidump?filter=...`-style probes
  (`c_uidump_{backdrop,sidebar,camera,album}.json`) all returned
  `"panels": [ ]` — empty, dead end; the useful data is in the unfiltered
  `c_uidump_song_select.json` (used below).
- `c_drawlog_roi_rightedge.json` ROI `[860, 0, 420, 720]` covers the whole
  right 420px column top-to-bottom (header + sidebar + bottom bar), so its
  `lastWriter` (draw index 350, `bottom_bg.mesh` on `overshell.cam`, the
  bottom "CONNECT CONTROLLER" bar) is just the chronologically-last draw
  touching that huge ROI, not a per-pixel topmost-surface answer for the
  sidebar band specifically — not useful on its own; the mesh-by-mesh
  drawlog walk below is what actually answers the question.

## Q1 — What clips the character?

**Verdict: nothing clips it.** It is not a camera-frustum, scissor, or
viewport truncation. It is an authored-placement + missing-occlusion gap:
the 3D backdrop scene (drawn on `world.cam`) renders the character meshes at
their full, correct, un-truncated screen-space extents, and a full-screen
dimmer overlay is drawn on top of the *entire* screen — but it isn't dark
enough, and only the list column gets a second, fully-opaque layer on top of
it. The boundary between "double-covered" (list) and "single-covered"
(sidebar) reads as a clip line but isn't one.

Evidence, from `c_drawlog_full.json` (351 draws, frame 560) and
`c_uidump_song_select.json` (frame 558):

- **Character draws are unclipped.** All `world.cam` skinned-mesh draws
  (indices 41-179, four background characters) have screen rects fully
  inside `[0,0]-[1280,720]` — e.g. the rightmost character's
  `hands_naked.mesh` rect is `[996.3, 401.3, 252.4, 139.7]` (right edge
  996.3+252.4=1248.7, still <1280) and its `femalerings_jem.mesh` rect is
  `[996.3, 498.3, 242.6, 25.6]` (right edge 1238.9, still <1280). No draw in
  the world.cam set is truncated at a boundary; the renderer is drawing full
  geometry, not clipping it.
- **A full-screen dimmer exists and is authored, not a runtime bug.**
  `header_list_bg.mesh` (draw `i=180`, `[ui.cam]`, pass 1, `trans:
  "background.grp"`) has `rect: [0.0, 0.0, 1280.0, 720.0]` — the entire
  screen — with `"matColor": [0,0,0,0.5]` and `"boundColor": [0,0,0,0.5]`.
  Per the engine's own field comments
  (`../milo-native-engine/src/platform/RB3DrawLogDebug.h:79-80`): `matColor`
  = "authored mat->GetColor() at draw time", `boundColor` = "effective
  post-binder mu.color (UI floor applied)". Both are identical
  `(0,0,0,0.5)` — the UI-floor / text-contrast machinery (the W4.2 family,
  lint-4-relevant) did **not** touch this value; alpha=0.5 is the raw
  authored material color from `header_list_bg.mat` in the ported
  `song_select.milo_xbox`. This mesh draws *after* the world.cam character
  draws (pass 1 vs pass 0/i=0-179), so compositing order is correct — it's a
  50%-black wash over the whole backdrop, uniformly, both columns.
- **The list column gets a second, opaque cover; the sidebar does not.** The
  visible difference between "list looks solid" and "sidebar shows legs" is
  that the list rows (`song.lst`/`setlist_main.lst` entries) paint their own
  additional opaque/near-opaque row-highlight quads on top of the 50% dimmer
  (the alternating blue-gray/dark bars visible in `c_realsong_full.png`),
  while the sidebar's difficulty-grid area (`live_diffs.grp`) has **no
  member mesh that draws anything** — see Q2 for the census.
- **Pixel-level corroboration** (`docs/native/.../evidence/c_realsong_full.png`
  vs retail): sampling five points in the sidebar band (x=950-1100,
  y=430-530) gives native RGB values including `(169,167,169)` and
  `(179,179,179)` (bright, skin/highlight-toned) against retail
  (`images/retail-screenshots/yt_qRagnZCIMzk_song_select_diff_ratings.png`)
  values at the same coordinates of `(3,0,8)`, `(46,39,53)`, `(3,0,16)` —
  uniformly dark, no bright/skin-toned pixels. Quantitative confirmation
  that native's sidebar band is a dim-only 3D render while retail's is a
  near-opaque dark panel.

## Q2 — Does retail have an opaque panel behind the sidebar? Missing panel draw or missing depth/stencil mask?

**Verdict: missing panel draw (asset gap), not a depth/stencil mask issue.**
Retail visibly has a near-opaque dark panel there (photographic evidence,
both a 360-tier press screenshot and an actual Wii capture); our ported
360-ARK `song_select.milo_xbox` asset has no valid backing geometry that
composites in that screen position for the quick-view (non-drill-in) layout.
This is a **re-confirmation with fresh W32 evidence** of Wave-13's
`execution/W4.3-C2a/STATUS.md` finding (quoted, still true against current
HEAD):

> "NEVER-SUBMITTED. The backing meshes are authored show=1 but their
> container PanelDir (`song_select_details`) is show=0, so nothing in it is
> drawn in quick-view. Not a render guard/alpha/depth drop — the whole
> sub-panel is hidden." ... "force-showing the PanelDir (whole OR a surgical
> backing-only per-mesh show) changed the difficulty-grid ROI
> (x905-1270,y385-600) brightness by <1%" ... "force-showing
> `leaderboards_bg.mesh` + `help_bg_rating.mesh` draws a light GLASS
> leaderboard frame low-right (ROI 68.75→72.07 — BRIGHTER, not a dark
> backing) — it's the mini-leaderboard's frame, wrong position and wrong
> tone."

Fresh W32 `c_uidump_song_select.json` (frame 558) independently reproduces
the same census C2a ran in Wave 13:

```
difficulty_bg02.mesh  showing=True  draws=0   (member's own flag=1, container hidden)
raitings_bg.mesh      showing=True  draws=0
difficulty_bg01.mesh  showing=True  draws=0
difficulty_bg.mesh    showing=True  draws=0
difficulty_bg07.mesh  showing=True  draws=0
difficulty_bg06.mesh  showing=True  draws=0
difficulty_bg05.mesh  showing=True  draws=0
difficulty_bg04.mesh  showing=True  draws=0
difficulty_bg03.mesh  showing=True  draws=0
help_bg_rating.mesh   showing=False draws=0
leaderboards_bg.mesh  showing=False draws=0
right_side_song.grp   showing=True  draws=0   (Group container, no self-draw — normal)
right_side.grp        showing=True  draws=0
live_diffs.grp        showing=True  draws=0   (the quick-view grid's own container)
```

`draws=0` on every candidate backing mesh, on this build, at this frame —
none of them are in the draw log at all. (Caveat carried from the Wave-13
census and re-verified against `native/src/rb3_uidump.cpp:138`: the
`"showing"` field is only meaningful for `RndDrawable`-derived classes —
`right_side_setlist.env` (`class: "Environ"`) reads `showing=False` in the
dump, but `Environ` extends `Hmx::Object` directly, so the `dynamic_cast<
RndDrawable*>` at that line fails and the field is a hardcoded false, not a
real signal — it is **not** evidence the Environ itself is hidden.)

**Depth/stencil is not implicated.** `header_list_bg.mesh`'s draw record has
`hasDepth: true`, a normal blend mode, and correct pass ordering (drawn after
the 3D scene, before the list-row opaques) — the dimmer that IS present is
compositing correctly, just too transparently for that region, and nothing
in the drawlog shows a stencil/scissor operation truncating any candidate
backing mesh's rect. The gap is that **no second, more-opaque layer exists
for the sidebar column** in the currently-ported asset, the same conclusion
W4.3-C2a reached from the asset-authoring side; this session adds the
runtime-compositing side (the dimmer's alpha and pass order) as independent
corroboration.

## Mechanism summary (both questions, one root cause)

1. `header_list_bg.mesh` dims the *entire* screen 50% (authored, not a
   runtime regression) over the raw `world.cam` backdrop scene, which
   includes several skinned NPC/scenery characters (`escapeartist_resource`,
   `greaserjacket_resource`, `hippyfringe_resource`, etc. — costume-preset
   background dressing, not the user's own band) rendered at full,
   unclipped screen extent.
2. The list column additionally gets opaque per-row backing quads (part of
   the list-item widgets), so 50%+opaque = solid there.
3. The sidebar/difficulty-grid column has **no** second layer — the only
   candidate backing meshes in the ported 360-ARK milo belong to a sibling
   `song_select_details` drill-in sub-panel that is never shown in
   quick-view (W4.3-C2a, re-confirmed fresh this wave) — so 50%-only there
   lets the backdrop character read through clearly, which the user
   perceives as the character "clipping in" at the screen's right side.

This is **not** the SKEL/CROWD/F5 family and does not touch
`BandCharacter.cpp`/`BandDirector.cpp`/`OvershellDir.cpp`/`CharDriver*` — no
STOP memo required; this is a UI-compositing/asset gap, out of scope for
this diagnosis-only rider to fix.

## Candidate fix surface (for W33, not landed here)

Two options, both asset/UI-side, neither is a decomp-correctness bug (no
source is "wrong" relative to retail's asset graph — the retail Wii disc's
`song_select.milo` for the *Wii* build may simply carry a sidebar backing
element that never made it into the ported 360-ARK extraction used here;
this is an asset-completeness gap, not a code defect):

- **(a) Native-only synthesized quad (three-tier: retail-proven faithful
  restoration → default-ON + opt-out).** Draw an additional near-opaque dark
  quad sized to the sidebar/difficulty-grid bounding box — C2a's own
  measured ROI is `x905-1270,y385-600`, though the true region (from this
  session's screenshot inspection) should extend from just below the album
  art down to the bottom bar, not just the drill-in ROI — behind
  `live_diffs.grp` on `ui.cam`. This is the same class of fix as F2/F4 this
  wave (Lane C, `native/src/rb3_render_hook.cpp`, append-only
  predicate/policy addition) and would need retail-paired before/after
  crops + a hit-count per lint 8. Simple, self-contained, does not touch any
  claimed TU.
- **(b) Author a new backing element in the milo asset.** W4.3-C2a's own
  follow-up: "Authoring a new opaque/tinted backing quad in `right_side.grp`
  behind `live_diffs.grp` (size/pos matched to the grid) is the only route
  to the retail look — an asset/UI edit, not an engine or asset-borrow fix."
  Out of scope for a code-only lane; requires milo-authoring tooling this
  campaign doesn't currently exercise.
- Do **not** attempt to raise `header_list_bg.mesh`'s global alpha — that
  mesh covers the *entire* screen including the list column, which is
  already correctly composited; raising it globally would over-darken the
  venue backdrop visible in other panels/transitions that rely on the
  current 0.5 value being unchanged (no evidence gathered this session on
  what else reads that value, but the blast radius argument alone rules it
  out as a targeted fix).

## W33 charter draft

**W33-F7-SIDEBAR-BACKING** (candidate title). Scope: land candidate (a)
above — a native-only opaque backing quad behind the song-select difficulty
grid, matching the retail dark-panel look
(`images/retail-screenshots/yt_qRagnZCIMzk_song_select_diff_ratings.png`).

- STEP-0: confirm the exact retail panel bounds/color via a second retail
  reference if available (the two on hand are enough to establish "near
  opaque dark", not enough to pin an exact hex/alpha — this session's pixel
  samples give a reasonable starting range, `RGB ~(3-77, 0-69, 8-78)`,
  effectively near-black).
- Owned surface: `native/src/rb3_render_hook.cpp` (same append-only grant
  pattern as W32 Lane C) — coordinate with whichever lane holds that TU's
  exclusive-write grant that wave (A2-class fence).
- Fix-tier: three-tier rule → this is a **retail-proven faithful
  restoration** (photographic evidence from two independent retail sources
  this session), so default-ON + `RB3_NO_*` opt-out is the right target
  tier, not default-OFF opt-in — earn it with ON-vs-OFF before/after crops
  per the standing rule.
- Acceptance: crop-paired before/after at a real-song-row focus (not a
  heading row — W31 trap), retail-paired; drawlog-golden PASS (no regression
  elsewhere on the panel); hit-count on the new draw call; confirm the list
  column (already correct) is visually unchanged.
- Non-goals: do not touch `header_list_bg.mesh`'s global alpha (see above);
  do not attempt candidate (b) (asset authoring) unless a milo-authoring
  path exists by W33.

## Files read this session (no writes)

`src/band3/meta_band/SongSelectPanel.cpp`, `src/system/rndobj/ScreenMask.h`,
`native/src/rb3_uidump.cpp`,
`/home/free/code/milohax/milo-native-engine/src/platform/RB3DrawLogDebug.h`
(read-only, engine repo), `execution/W4.3-C2/STATUS.md`,
`execution/W4.3-C2a/STATUS.md`, `images/retail-screenshots/
yt_qRagnZCIMzk_song_select_{diff_ratings,list,album_art}.png`,
`images/retail-screenshots/yt_qSRJ8HHPXzM_song_select_wii.png`, and all
`evidence/c_*.{json,png}` files listed above (pre-existing, this lane, prior
invocation).
