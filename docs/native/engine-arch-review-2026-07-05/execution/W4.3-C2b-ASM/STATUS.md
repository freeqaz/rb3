# W4.3-C2b-ASM STATUS — Wave 14, Lane A: album-art whole-assembly fix

VERDICT: **FIXED.** `RB3_SS_ART_YFIX` (`src/band3/meta_band/SongSelectPanel.cpp`,
extended not replaced — same flag name, corrected mechanism + calibration) now
moves the album-art picture AND its ornate bezel together, clears the header
row, and clears the left-column score/star readout. Default-OFF, pending
coordinator E1 re-review.

## 1. Identifying the revealed element (TransParent-chain evidence)

The C2b4 STATUS claimed `album_art.pic` + `album_frame01.mesh` were "both
children of the group" (`album_art.grp`) and that a Z-only nudge moved them
"as one rigid unit." **This is refuted.** `RndGroup` membership (draw/show-
hide grouping — census `W4.3-C2a/census.txt:364-375` lists
`album_frame01.mesh` as an `album_art.grp` *member*) is a separate concept
from `RndTransformable::TransParent()` scene-graph parenting. Dumping the
actual TransParent chain (`RB3_C2B_ASM_DBG`, walks `p->TransParent()` up to
depth 12) gives:

```
album_art.grp        world.v=(292.57,-0.00,206.33) chain: album_art.grp <- bone_album_group.mesh <- all.grp <- song_select
album_art.pic         world.v=(488.94,31.91,206.33) chain: album_art.pic <- album_art.grp <- bone_album_group.mesh <- all.grp <- song_select
album_frame01.mesh    world.v=(277.03,0.09,117.54)  chain: album_frame01.mesh <- header_goals.grp <- header.grp <- all.grp <- song_select
```

`album_frame01.mesh`'s real trans-parent is `header_goals.grp` — an authored
sibling of the CAREER header assembly, not a child of `album_art.grp` at all.
This is an asset-authoring quirk (grouped-for-visibility, parented elsewhere)
that the C2b4 lane never actually verified before writing its STATUS.

**Empirical confirmation**: `RB3_C2B_ASM_HIDE=album_frame01.mesh` (forces
`SetShowing(false)`) made the revealed grey bezel disappear completely from
the flag-on screenshot — proof positive this is the element, not a "separate
unrelated always-present background decoration" as C2b4 speculated.

Reproduced the exact coordinator-observed defect
(`c2basm_original_single_axis_bug_repro.png`, `album_art.grp` local Z -=120
only, `album_frame01.mesh` untouched): the picture moves down-right leaving a
bare grey ornate frame sitting at its old position, empty.

## 2. Why a naive "move both by -120 Z" doesn't work either

The two nodes don't share a coordinate frame. Measuring `WorldXfm()` deltas
for a unit offset on each node's *local* axes:

- `album_art.grp` (`bone_album_group.mesh <- all.grp` chain): local Z rotates
  almost entirely into world Y (`-120 local z -> +107.17 world y`, world x/z
  **unchanged**). Local X maps into world X at ~0.915 scale (no cross-terms:
  `+60 local x -> +54.9 world x`, y/z unchanged).
- `album_frame01.mesh` (`header_goals.grp <- header.grp <- all.grp` chain):
  no rotation at all. Local Y maps 1:1 to world Y (`+120 local y -> +120.09
  world y`... using `+107.17` gives `+107.26 world y`, matching
  `album_art.grp`'s shift almost exactly). Local X maps 1:1 to world X
  (`+60 local x -> +60.0 world x` exactly).

An initial attempt applying `-120` to *both* nodes' local **Z** compiled and
ran but was wrong: `album_frame01.mesh`'s local Z maps 1:1 to world **Z**
(depth), which barely moves the on-screen position for this camera — the
grey box stayed roughly in its original screen location, just re-sorted in
depth. Confirmed via `RB3_C2B_ASM_DBG` dump and a fresh screenshot before
discarding this variant.

## 3. Final calibration (one whole-assembly move, matched world-space delta)

```cpp
if (getenv("RB3_SS_ART_YFIX")) {
    if (RndTransformable *g = mDir->Find<RndTransformable>("album_art.grp", false)) {
        Transform &t = g->DirtyLocalXfm();
        t.v.x += 45.0f;
        t.v.z -= 120.0f;
    }
    if (RndTransformable *f = mDir->Find<RndTransformable>("album_frame01.mesh", false)) {
        Transform &t = f->DirtyLocalXfm();
        t.v.x += 41.2f;
        t.v.y += 107.17f;
    }
}
```

- Y/Z pairing (`album_art.grp` Z=-120 / `album_frame01.mesh` Y=+107.17) is the
  measured world-Y-matching pair — both nodes now end up shifted by the same
  ~+107u in world Y, so the bezel and picture move together (verified: flag-on
  screenshot shows the picture sitting inside its own moved bezel, no
  separate empty grey box).
- X pairing (`album_art.grp` X=+45 / `album_frame01.mesh` X=+41.2, ratio
  matching each node's local->world X scale) fixes the coordinator's second
  E1 note: the Y move alone also produces a small screen-**leftward** shift
  (a perspective-projection parallax side effect of this camera on the
  rotated `album_art.grp` chain — not a separate bug), which pushed the box
  further into the left-column "N/M ★" score readout. Swept X in
  {0, 30, 45, 60} at fixed Y; 30 still clipped the top-row star icon, 45 and
  60 both cleared cleanly — chose 45 as the smaller of the two clean values
  (closer to the retail box's on-screen horizontal position).

## 4. Gates

- **E1 captures** (this directory):
  - `c2basm_flagoff.png` — flag unset, full screen, pixel-identical to the
    pre-fix defect (art box overlaps header icon + score/star readout,
    `album_frame01.mesh` sits in its normal — currently correct-looking
    because untouched — position).
  - `c2basm_flagon.png` — flag set, full screen: art sits cleanly below the
    header row (icon, "0", star all fully visible, no clipping), bezel moves
    with the picture (no revealed grey box), left-column score/star readout
    (`0/10 ★`, `0/5 ★`, `0/30 ★`) fully clear, no new overlaps.
  - `c2basm_original_single_axis_bug_repro.png` — reproduction of the
    original single-node-only fix for direct comparison, showing exactly the
    defect the coordinator's E1 hold described (bare grey frame + picture
    detached from it).
  - Compared against `images/retail-screenshots/yt_qRagnZCIMzk_song_select_diff_ratings.png`
    (art below header, right side, no reveal, no overlap) — matches
    structurally (art box sits right of the list, below the header row,
    frame encloses the picture cleanly).
- **Drawlog 792 flag-off unchanged**: `python3
  scripts/native/drawlog-golden.py --fixed-clock --canonical-order --bin
  native/build-agent-W4.3-C2b-ASM/rb3-native` → **PASS** (792 draws, 268
  known-residual divergences within bound) with `RB3_SS_ART_YFIX` unset (the
  fix code is unreachable with `getenv()` returning null, so this also
  confirms flag-off is byte-identical to the C2b4 lane's baseline).
- **Frame-count-settled captures throughout** (never wall-clock `sleep`) —
  same `entry_frame + 200` methodology as C2b4, reused via
  `/tmp/c2basm_capture2.py` (frame-count poll of `/api/health`, pgid-only
  process cleanup).

## 5. Pre-existing bug revealed but explicitly OUT OF SCOPE

With the album-art overlap now fixed, a `(null)` text label is visible next
to the header's guitar-controller icon (top-right, previously hidden because
the mispositioned art box physically covered that screen region — confirmed
by cropping the same region in `c2basm_flagoff.png`, where the art box's
top-left corner sits directly over it). This looks like a gamertag/profile
display stub printing the literal string `"(null)"` when no profile is
connected (headless test harness has no profile), vs. retail which shows a
real gamertag (e.g. `ViniciusG5` in
`yt_qRagnZCIMzk_song_select_diff_ratings.png`). This is a **separate**
subsystem (profile/account text formatting, not album-art layout) and is
noted as a follow-up, not fixed here (game-side scope for this task was the
art assembly specifically).

## 6. Diagnostic-only probes (left in, harmless, not catalogued)

Per the W4.3-C2a precedent (scratch/calibration-only probes are not added to
`classification.json`):
- `RB3_C2B_ASM_DBG` — TransParent-chain + world-xfm logger for the
  candidate-list (art group members, header/career/goals siblings, etc).
- `RB3_C2B_ASM_HIDE=<name>` — force-hide one named `RndDrawable` for A/B
  confirmation.
- `RB3_C2B_ART_NUDGE=<art_x>,<art_z>,<frame_x>,<frame_y>,<frame_z>` —
  independent per-node additive calibration probe (superset of the old
  single-Z-only C2b4 probe, extended this session to control both nodes on
  independent axes once the coordinate-frame mismatch was discovered).
- Pre-existing `RB3_C2B_XFM_DBG` (C2b4-authored, untouched).

All are `getenv()`-gated and unreachable with the env vars unset — verified
unreachable via the drawlog-792 flag-off gate above.

## 7. classification.json

Updated `RB3_SS_ART_YFIX`'s `faithfulStatus` in place (same flag, not a new
one — task said "replacing or extending `RB3_SS_ART_YFIX`"; chose extend
since the flag name/gate/call-site are unchanged, only the fix body and
description). Applied as a single surgical string replacement under
`/tmp/milo-engine-classjson.lock` so the diff is exactly one JSON value,
not a whole-file reformat (a first attempt using `json.dump` reformatted
~30 unrelated entries by re-escaping non-ASCII characters differently than
the file's existing mixed style — reverted and redone as a targeted
string substitution instead).

## Build dir

`native/build-agent-W4.3-C2b-ASM` — Clang + `CMAKE_BUILD_TYPE=Debug` (Release
strips the out-of-line `TimeConversion.cpp` inline symbols other TUs need,
causing link failures — same gotcha the C2b4 lane's build dir avoided).
