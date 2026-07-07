# W4.3-C2b4 STATUS — C2b (song-select album art) + C4 (hub ticker)

VERDICT: **BOTH FIXED.** Two independent flag-gated, default-OFF game-side
fixes land this wave:
- `RB3_HUB_TICKER_YFIX` (C4) — `src/band3/meta_band/MainHubPanel.cpp`
- `RB3_SS_ART_YFIX` (C2b) — `src/band3/meta_band/SongSelectPanel.cpp`

Engine working tree 44716f4, untouched (no engine render TUs edited — A9
respected, Lane G's composite region left alone). Build dir:
`native/build-agent-W4.3-C2b4`. Drawlog-792 gate PASS with both new flags
unset (flag-OFF byte-identical by construction — the fix code is unreachable
with `getenv()` returning null).

## C4 — hub ticker "NEXT MESSAGE (n/n)" / message body collision

### The defect
On `main_hub_screen`, `expand_message_area.ihp`'s header
("NEXT MESSAGE (1/1)") and `message.lbl`'s body ("Connect to Xbox LIVE to get
more songs...") draw on the same visual line, colliding. Retail
(`images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`) stacks them: header
line, then body line below.

### Root-cause history (this lane inherited and refuted two prior theories)
1. An earlier pass of this lane claimed nudging `message.lbl` down z-fights
   with `BandCharacter` heads and makes the text invisible. **Re-measured this
   wave at +8/+14/+20/+30u — text stayed fully visible/legible at every
   offset, no occlusion or z-fight at any magnitude.** That finding does not
   reproduce.
2. The Wave-12 C34 side-agent (`execution/W4.3-C34/STATUS.md`,
   `COORDINATOR_NOTE.md`) proposed a text SCALE/WRAP-WIDTH application gap:
   retail renders the message body at a visibly smaller wrapped font; native
   renders it label-sized, unwrapped. **Probe evidence refutes this**: adding
   wrap_width/size logging to the `RB3_HOLDLABEL_DBG` probes in
   `UILabel::DrawShowing()` and `InlineHelp::DrawShowing()` (the latter is the
   actual draw path for `mTextLabels[i]`, bypassing `UILabel::DrawShowing()`
   entirely — this is why a UILabel-only probe missed the header label) shows
   `message.lbl` already renders with a real smaller runtime size (16.224 vs
   header's 18.0) and a real nonzero wrap_width (750.0 vs header's 0.0, since
   the short fixed header text needs no wrap). Scale/wrap is correctly
   applied; it is not the gap.

### Actual root cause
Y-anchor/panel-origin spacing gap. Authored world Z:
- `message.lbl` = -134.15
- `expand_message_area.ihp` = -128.12
- gap = ~6.0u

Compare to a confirmed-correctly-stacked pair on the same screen:
- `fan_total.lbl` / `level.lbl` = 190.51 / 169.71, gap = ~20.8u (3-4x more
  clearance)

### Fix
`MainHubPanel::UpdateHeader()` (line ~301-321): under
`getenv("RB3_HUB_TICKER_YFIX")`, `label->DirtyLocalXfm().v.z -= 15.0f;` —
restores a same-order-of-magnitude stacking gap (~21u total). Verified via
before/after screenshot (`c4_flagoff_crop.png` shows the collision;
`c4_flagon_crop.png` shows clean stacked separation, re-verified fresh this
session with frame-count-settled captures, not stale from an earlier attempt)
and a nudge sweep (8/14/20/30u all clean, no regression).

## C2b — song-select album-art quad overlaps header row

### The defect
Quick-view `song_select_screen`: `album_art.grp`'s picture (`album_art.pic`)
+ its own bezel (`album_frame01.mesh`, a sibling child of the same group —
census `W4.3-C2a/census.txt:364-375`) sits positioned too far up-left in the
native render. Its top-left corner rides over the header row's `stats.grp`
content (`gamertag.lbl`, `careerscore.scr`, `careerstars.sd`,
`profile_picture.mesh` — census `census.txt:278-282`), hiding the score/star
counter. Retail (`yt_qRagnZCIMzk_song_select_album_art.png`,
`yt_qSRJ8HHPXzM_song_select_wii.png`) shows the art frame sitting cleanly
below/right of the header content, no overlap.

### Diagnosis
- No C++/DTA code positions `album_art.pic`/`album_art.grp` anywhere in
  `SongSelectPanel.cpp` or `TexLoadPanel.cpp` — confirmed via source search and
  via `orig-assets/extracted/ui/song_select/song_select.dta:1430-1545` (only
  `set tex_file`/`set mesh` swaps, no positional DTA commands). The screen
  position is purely the authored xbox-360-ARK milo asset xfm.
- Unlike C4's clean single-axis label pair, raw `WorldXfm()` comparison
  between `album_art.grp` (292.57, -0.00, 206.33) and `header.grp`
  (6.64, 0.00, 7.40) does NOT cleanly indicate the fix — both X and Z differ
  by 100s of units, inconsistent with a simple small vertical-gap read. This
  motivated a pivot to the same empirical nudge-and-capture method used
  successfully for C4.
- **Methodology finding**: an initial 3-way nudge test (off/+50/-50 on
  `album_art.grp`'s local Z) using a fixed 2.0s wall-clock sleep after
  reaching `song_select_screen` produced a **non-monotonic, contradictory**
  trend — a follow-up finer sweep (-20/-30/-40/-60/-80, still wall-clock)
  showed overlap getting *worse* as the nudge magnitude increased, opposite of
  the single -50 sample. Root cause: the album-art panel has an
  entrance-slide-in animation, and small process-launch timing jitter across
  separate headless runs means a fixed wall-clock sleep duration corresponds
  to a *different number of simulated frames* run-to-run, so the capture
  catches the animation at different (and inconsistent) settle points despite
  `RB3_FIXED_CLOCK=1`. **Fix**: poll `/api/health`'s frame counter and wait for
  a fixed number of simulation frames (+200) past `song_select_screen` entry,
  instead of sleeping wall-clock time. Re-running the full sweep
  (off/-20/-40/-60/-80/-100/-120/-150/+50) with frame-count settling gave a
  clean, monotonic trend: more-negative Z moves the whole group (picture +
  its own bezel, as one rigid unit — both are children of `album_art.grp`, so
  the "static-looking gray decorative panel" behind it that does NOT move is
  a *separate*, unrelated always-present background decoration, not the
  album's own frame) down-and-right, away from the header icon; positive Z
  makes the overlap worse.
- Calibrated fix magnitude: **-120.0f** on `album_art.grp`'s local Z clears the
  header icon/score with a clean gap (confirmed no overlap at -120 or -150;
  -120 chosen as a less aggressive value that already fully clears).

### Fix
`SongSelectPanel::FinishLoad()` (line ~104-125 after edit): under
`getenv("RB3_SS_ART_YFIX")`,
`mDir->Find<RndTransformable>("album_art.grp", false)->DirtyLocalXfm().v.z -= 120.0f;`.
Verified via before/after screenshot: `c2b_flagoff_crop.png` (guitar icon +
score partially hidden behind the art) vs `c2b_flagon_crop.png` (icon fully
visible, clean gap, art sits inside its own decorative bezel/backdrop with no
overlap).

Diagnostic-only probes left in `SongSelectPanel.cpp` (harmless, unreachable
with flags unset): `RB3_C2B_XFM_DBG` (world/local xfm logger for
`album_art.grp`/`album_art.pic`/`header.grp`/`header_song_bg.grp`/`all.grp`,
both at `FinishLoad()` and every `Poll()` — logged values were identical at
both call sites, confirming this is a static authored-position bug, not a
mid-animation timing issue) and `RB3_C2B_ART_NUDGE=<float>` (further ad-hoc
calibration on top of the -120 default, for any follow-up tuning).

## Shared-family verdict (task #80)

**PARTIAL / not literally the same bug, but the same bug CLASS.** Both C2b and
C4 are Y-anchor/panel-origin-family symptoms: an authored spacing/position gap
between sibling UI elements that the native renderer does not correct,
requiring a small empirically-calibrated local-Z nudge to restore the intended
on-screen separation. But the concrete mechanism differs:
- C4 is a clean **single-axis, label-to-label** spacing deficit (~6u vs an
  ~20.8u reference pair, both siblings share the same simple projection
  behavior — Z directly maps to on-screen vertical position).
- C2b is a **whole-group, multi-axis (diagonal)** offset — the raw world-xfm
  gap between the two groups is large and uninterpretable on its own, and a
  single Z nudge moves the group diagonally on screen (both apparent X and Y
  shift together), most likely because `album_art.grp` sits off the camera's
  optical axis so Z (depth) changes project into apparent X+Y screen motion
  under this panel's perspective camera, unlike C4's simpler local coordinate
  frame.

Given the differing mechanism, each ships as its own independent flag/fix
(`RB3_HUB_TICKER_YFIX`, `RB3_SS_ART_YFIX`) rather than one shared code path.
The COORDINATOR_NOTE's caution that "same family as C2b" is likely FALSE for
C4's *original* (font-scale/wrap) hypothesis is upheld — but at the level of
"both are authored Y-anchor spacing gaps needing empirical calibration," the
family resemblance holds loosely enough to be worth naming for future lanes
that hit similar overlap symptoms elsewhere in the UI.

## Gates
- Before/after captures vs retail for E1: done for both (screenshots in this
  directory: `c4_flagoff_crop.png`/`c4_flagon_crop.png`,
  `c2b_flagoff_crop.png`/`c2b_flagon_crop.png`).
- Drawlog 792 flag-OFF unchanged:
  `python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order
  --bin native/build-agent-W4.3-C2b4/rb3-native` → **PASS** (792 draws, 237
  known-residual divergences within bound) with both new flags unset.
- Settle-frames: frame-count-based settling required for C2b (see methodology
  finding above); C4 re-verified with the same frame-count-settled harness
  this session (not the possibly-stale wall-clock captures from an earlier
  pass of this lane) and still confirmed clean.

## Files touched (rb3; all `#ifdef HX_NATIVE` + `getenv`-gated, default-OFF,
Wii decomp byte-identical, no engine render TUs)
- `src/band3/meta_band/MainHubPanel.cpp` — C4 fix (`UpdateHeader()`) +
  diagnosis comment block + `RB3_HUB_TICKER_DBG` probe (`Poll()`).
- `src/band3/meta_band/SongSelectPanel.cpp` — C2b fix (`FinishLoad()`) +
  `RB3_C2B_XFM_DBG`/`RB3_C2B_ART_NUDGE` diagnostic probes (`FinishLoad()` +
  `Poll()`).
- `src/system/bandobj/InlineHelp.cpp` — extended `RB3_HOLDLABEL_DBG` probe
  (wrap_width/size) in `DrawShowing()`.
- `src/system/ui/UILabel.cpp` — extended `RB3_HOLDLABEL_DBG` probe
  (wrap_width/size) in `DrawShowing()`.
- `src/system/rndobj/Text.h` — added `float WrapWidth() const` getter (no
  prior public getter existed; needed by the above probes).
- `docs/native/engine-arch-review-2026-07-05/execution/W4.3-C2b4/{PLAN,STATUS}.md`
  + evidence screenshots.

## Flags appended (engine `src/platform/NativeCompatFlags.classification.json`,
append-only, under flock)
- `RB3_HUB_TICKER_YFIX` — class `workaround`, default off.
- `RB3_SS_ART_YFIX` — class `workaround`, default off.
- (Diagnostic-only probes `RB3_HUB_TICKER_DBG`, `RB3_C2B_XFM_DBG`,
  `RB3_C2B_ART_NUDGE` NOT appended, following the W4.3-C2a precedent of not
  cataloguing scratch/calibration-only probes in that lane's own three
  diagnostics.)
