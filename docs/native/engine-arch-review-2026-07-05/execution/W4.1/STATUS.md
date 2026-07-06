# W4.1 — STATUS

## C.S1 — planner (Opus) — done — 2026-07-06

Re-derived all three subitems against ground truth (`images/retail-screenshots/`,
`/tmp/visdiff-20260702/`, `/tmp/wave6-current-state/`) plus fresh native captures. PLAN.md written
and committed. Diagnostic harnesses preserved under `W4.1/harness/`.

**Verdicts:**
- **(a) main_hub grey quad — REAL, actionable.** Live-toggle bisection (`/api/dta/eval`, harnesses
  `hub-quad-*.py`, shots `/tmp/wave6-hub-probe{1..6}/`) localized it to a **backing/divider mesh
  child of `menu_buttons.grp`** in the 360-ARK `ui/main/main_hub.milo`, drawn opaque grey by native,
  hidden/translucent on Wii (SYS-5 family). Not a stale text slot; not any named button/subgroup/
  find-reachable mesh (all persistently-hide-tested negative). `showing` is re-driven each Poll.
  Fix = HX_NATIVE persistent `SetShowing(false)` on the leaf from `MainHubPanel`, default-OFF
  `RB3_HUB_MENU_QUAD_HIDE`. Owning files: `src/band3/meta_band/MainHubPanel.cpp/.h`.
- **(b) song_select overlap — LARGELY ALREADY FIXED.** Fresh captures (`/tmp/wave6-ss-recap/`) show
  text-overlap resolved by the existing `MusicLibrary::Text()` HX_NATIVE override
  (`MusicLibrary.cpp:1094-1160`) and the visdiff grey-quad-below-album-art gone. Residual = a minor
  red vertical sliver (identify-before-fix; may be a legit scrollbar). Impl = verify+document +
  optional sliver probe (default-OFF `RB3_SONGSEL_SLIVER_HIDE`).
- **(c) part_difficulty — NOT A BUG.** Recaptured with settle (`/tmp/wave6-partdiff-recap/`, 7-frame
  sequence frames 366→734): widgets render correctly and stably; the current-state frame-390 capture
  was mid the `part:guitar` camera zoom into the venue poster wall. Black poster quads = venue
  backdrop (venue-side, out of fence) → **documented backlog handoff to Lane D venue family.** No
  in-lane fix.

**Next (impl, C.S2 Sonnet):** implement (a) leaf-hide (pin the exact mesh first via group child
enumeration — the six named guesses already failed), verify (b) + optional sliver, file (c) backlog.

**Fence adherence:** no engine files, no `rb3_render_hook.cpp` touched (planning only).

## C.S2-a — impl (Sonnet) — done — 2026-07-06

Implemented subitem (a) per PLAN.md exactly (found the code + flag already
present in the working tree from an earlier interrupted run of this stage;
no checkpoint existed, so re-verified from scratch rather than trusting it
blind).

**Fix:** `MainHubPanel::Poll()` (rb3 `src/band3/meta_band/MainHubPanel.cpp`)
persistently hides `playnow.lsw` (a LabelShrinkWrapper child of
`mb_playnow.grp`/`menu_buttons.grp` in `ui/main/main_hub.milo`) under
`#ifdef HX_NATIVE`, gated by opt-in env flag `RB3_HUB_MENU_QUAD_HIDE`
(default-OFF). Re-asserted every Poll since the object's own `showing` is
re-driven by its owning group each frame (one-shot Enter()-only hides do not
stick, per the plan's finding from the six failed named-mesh probes).

**Verify (fresh build + before/after gate, this stage):**
- Built via `flock /tmp/rb3-native-build.lock cmake --build native/build-native
  --target rb3-native` (clean build against current engine + rb3 tree).
- Before/after capture (`RB3_FIXED_CLOCK=1`, nav `@10:start,@30:confirm`,
  ~2s settle at main_hub_screen): ROI x[380,895] y[365,410] mean luma
  **143.1 -> 44.1** (backdrop level) with the flag on. Screenshots
  `/tmp/wave6-w41a-gate/{before_off,after_on}.png` visually confirm the grey
  capsule quad is fully gone flag-ON; menu text (PLAY NOW / QUICKPLAY / START
  A ROAD CHALLENGE), MOTD ticker ("NEXT MESSAGE" text), and the bottom
  overshell bar are pixel-identical otherwise (A/A).
- Cross-checked against `images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`:
  retail has no such quad below the menu list — matches the flag-ON render.
- **A/A on the other screen:** song_select (`ss_off.png`/`ss_on.png`,
  nav `@10:start,@30:confirm,@140:select:pn_quickplay.btn,
  @220:select:qp_quickplay.btn`) is unaffected by the flag (expected — the
  fix is scoped to `MainHubPanel` only).

**Flag registration:** `RB3_HUB_MENU_QUAD_HIDE` was already present in the
engine's `NativeCompatFlags.classification.json` working tree (uncommitted,
from the same earlier interrupted run) alongside three unrelated in-flight
rows from the W3.1b lane (`RB3_ENV_PROJLIGHT`, `RB3_ENV_PROJLIGHT_FORCE`,
`RB3_ENV_FOG_FORCE`). To commit only my own row without sweeping in the
other lanes uncommitted work (hard rule: stage only your own files), I
staged an isolated git blob (HEAD content + my one row only, via
`git hash-object` + `git update-index --cacheinfo`) under
`flock /tmp/milo-engine-classjson.lock` rather than `git add`-ing the
whole file. Verified post-commit: the other three rows remain as an
uncommitted working-tree diff against the new HEAD, untouched, for their
owner to commit separately. class=`workaround`, default=`off`.

**Commits:**
- rb3 `b537d275` — `W4.1: hide spurious main_hub grey menu quad
  (RB3_HUB_MENU_QUAD_HIDE)`
- engine `8e38e74` — `W4.1: register RB3_HUB_MENU_QUAD_HIDE (main_hub
  grey menu-quad hide, default-OFF)`

**Fence adherence:** touched only `src/band3/meta_band/MainHubPanel.cpp`
(rb3) and the classification.json row (engine, append-only). No engine
render/gfx files, no `rb3_render_hook.cpp`.

**Not done in this stage (belongs to other C.S2 subitems / other agents):**
subitem (b) song_select sliver investigation, subitem (c) part_difficulty
backlog handoff documentation — these are separate checkpoint IDs per the
plan's C.S2 breakdown.

## C.S2-b — impl (Sonnet) — done (no-fix / verify+document) — 2026-07-06

Implemented subitem (b) per PLAN.md exactly: verified the two already-fixed
song_select issues are still resolved on current HEAD, then identified the
remaining "red vertical sliver" residual. **Verdict: legitimate scrollbar
thumb, matches retail pixel-for-pixel behavior — no bug, no fix, no flag.**

**Text overlap — reconfirmed fixed.** Re-read
`src/band3/meta_band/MusicLibrary.cpp:1094-1160` on current HEAD: the
`#ifdef HX_NATIVE` override in `MusicLibrary::Text()` is present and
unchanged (clears `label->SetTextToken(gNullStr)` before type-specific
writes, plus the `AppLabel`-cast-null `UILabel` fallback). Fresh captures
(`/tmp/wave6-ss-sliver-check/`, `/tmp/wave6-ss-header-check/`,
`RB3_FIXED_CLOCK=1`, native rb3-native, depths 0/6/10/13/20/30/50/70) at
both song rows and header rows show clean non-overlapping text throughout —
no stale group-letter / song-count residue.

**Grey placeholder quad below album art — reconfirmed fixed.** Explicitly
tested a *header row* highlighted (not just song rows, since the plan flagged
this as the trigger condition): `native_depth_06.png` (`B` header, 6 SONGS
0/30, highlighted) shows the venue backdrop below the album-art panel, not a
grey backing quad. Matches the visdiff-documented fix.

**Red sliver — root-caused, NOT a bug.** Investigated with a new harness
(`W4.1/harness/songsel-sliver-probe.py`) built on
`scripts/native/song-select-capture.py`:
- Captured 6 scroll depths (0/10/20/30/50/70) and scanned screen column
  x=893 for the dark-red pixels (`~(123,26,2)`) vs. the yellow highlight-bar
  column (x=50).
- **The sliver moves ~2.0-2.1px per scroll step** (21px/10 steps, 42px/20,
  84px/40) — a small, linear, *whole-list-proportional* rate, categorically
  different from the ~22-26px-per-row list scroll and independent of the
  highlighted row's on-screen position (which jumps once then holds fixed at
  y=400-442 across depths 10-70 while the sliver keeps sliding). This is
  scrollbar-thumb kinematics, not a mesh/label artifact tied to one slot.
- **Cross-checked against retail** (`images/retail-screenshots/`):
  `yt_qRagnZCIMzk_song_select_album_art.png` (list near the top, letters
  A/B) shows the identical dark-red mark near the *top* of the same track
  position; `yt_qRagnZCIMzk_song_select_diff_ratings.png` (list scrolled to
  the *end*, letters V-Y) shows the same mark near the *bottom* of the
  track. Retail has the exact same element, moving the same way. It is an
  authored scrollbar-thumb decoration for the song list, correctly rendered
  by the native port — the "overlap family" visdiff note flagged an
  intentional design element as a possible bug, which this investigation
  refutes.
- No `RB3_SONGSEL_SLIVER_HIDE` flag registered (per the plan: "if it is a
  legitimate scrollbar, document as no-fix and close" — nothing to hide).

**Gate:** no code changed in this stage (`src/band3/`, `src/system/ui/`,
`src/system/bandobj/` all untouched — `git status` clean for those paths at
both start and end of this stage), so the plan's "confirm the OTHER two
screens unchanged" A/A requirement is trivially satisfied — main_hub and
part_difficulty could not have regressed since nothing built differently.
No rebuild was performed; captures reused the existing `rb3-native` binary
built by C.S2-a.

**Fence adherence:** touched only documentation/harness files under
`docs/native/engine-arch-review-2026-07-05/execution/W4.1/` (this STATUS.md
+ new `harness/songsel-sliver-probe.py`). No `src/band3/`, `src/system/`,
`rb3_render_hook.cpp`, or engine files. No classification.json changes (no
new flag needed).

**Commit:** rb3 (docs-only, this stage's own files) — see commit list below.

## C.S2-c — impl (Sonnet) — done (no-fix / backlog handoff) — 2026-07-06

Implemented subitem (c) per PLAN.md exactly: the plan's verdict is **NOT A BUG** (mid-transition
frame), so per this stage's own instructions the action is documentation, not a code change.

**Re-verified the C.S1 diagnosis against current HEAD** (post subitem-(a) commit `b537d275`,
freshly built `native/build-native/rb3-native`) by re-running the unchanged
`W4.1/harness/partdiff-settle-recap.py` harness (`RB3_FIXED_CLOCK=1`, nav to
`part_difficulty_screen` with **no** `part:`/`diff:` press, +0/+30/.../+360-frame settle
sequence, `/tmp/wave6-partdiff-recap/`):

- Widgets (song header, album art, "RIGHTY MODE / SONG DIFFICULTY" panel, "CHOOSE INSTRUMENT /
  GUITAR / BASS" card) render correctly and stably across the **entire** 360-frame window. **No
  black poster quads in any frame.**
- Confirms `/tmp/wave6-current-state/partdiff_default.png` (frame 390) is a **mid-transition
  frame** — its nav pressed `@380:part:guitar` only 10 frames earlier, landing inside the
  part-confirm camera **zoom into the venue poster wall** ("Restaurant Bar & Grille" menu-board
  backdrop). The black patches are that backdrop's own poster/menu-board meshes seen in close-up
  during the zoom, not missing/broken UI widgets. Reproducible, not a one-off capture artifact.

**Verdict confirmed: NOT a UI/widget bug.** No code changed this stage (`src/band3/`,
`src/system/ui/`, `src/system/bandobj/` all untouched — the finding's root cause is venue backdrop
rendering, which is out of the band3/UI fence). No flag registered (nothing to gate).

**Documentation action (this stage's deliverable):** filed
`W4.1/BACKLOG-partdiff-venue-poster.md` — the backlog handoff for the black poster/menu-board
quads, cross-referencing the two Lane D venue-family characterizations landed this same wave
(`W2.7` black head, `W3.3` grayscale venue) as plausible-but-unproven shared-mechanism candidates,
with a concrete Wave-7 repro + flag-isolation recipe and an explicit file-fence note (fix territory
is engine render files / `rb3_render_hook.cpp`, out of Lane C's scope).

**A/A on the other two screens:** trivially satisfied — no source changed in this stage, so
main_hub (subitem a) and song_select (subitem b) cannot have regressed. `git status` clean for all
in-fence source paths at both start and end of this stage.

**Fence adherence:** touched only `docs/native/engine-arch-review-2026-07-05/execution/W4.1/`
(this STATUS.md + new `BACKLOG-partdiff-venue-poster.md`). No `src/band3/`, `src/system/`,
`rb3_render_hook.cpp`, or engine files. No classification.json changes.

**Commit:** rb3 (docs-only, this stage's own files) — see commit list below.

## C.S5 — lane verify (Opus) — done — 2026-07-06

Independent whole-lane verification. Fresh rb3-native build under
`flock /tmp/rb3-native-build.lock cmake --build native/build-native --target rb3-native`
(green, links current engine HEAD). New harness
`W4.1/harness/lane-verify-capture.py` boots to each of the three lane screens
(`RB3_FIXED_CLOCK=1`, 3s settle) and captures an A/A OFF vs A/B ON pair keyed on
`RB3_HUB_MENU_QUAD_HIDE`. Captures: `/tmp/wave6-cs5-verify/{screen}_{off,on}.png`.

**Subitem (a) main_hub grey quad — FIX CONFIRMED PRESENT.**
- flag-OFF (`main_hub_off.png`): the opaque light-grey rounded-capsule quad is
  present mid-screen below "START A ROAD CHALLENGE" (matches the pre-wave
  `/tmp/wave6-current-state/mainhub_default.png`).
- flag-ON (`main_hub_on.png`): the quad is **gone**; PLAY NOW / QUICKPLAY /
  START A ROAD CHALLENGE menu text, the "NEXT MESSAGE (1/1)" MOTD ticker, and the
  bottom overshell bar are all intact.
- ROI x[380,895] y[365,410] mean luma **144.7 → 73.5** (quad removed; ON value
  above the earlier-reported 44.1 only because this boot's venue-backdrop
  parallax phase places brighter venue geometry — the "PALA" sign / doorway — in
  the ROI rather than dark sky; the capsule itself is verified absent by eye).
- Cross-checked vs `images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`:
  retail has no such quad. Matches flag-ON.

**Subitem (b) song_select — VERIFIED CLEAN, no regression.**
`song_select_off.png`: MUSIC LIBRARY list renders with clean non-overlapping
header/song/count text (SETLISTS / PARTY SHUFFLE / RANDOM SONG highlighted /
"123  2 SONGS  0/10" / A / B headers), the random-song "?" art panel (no grey
placeholder quad below album art), and the thin dark-red scrollbar thumb at the
right edge — consistent with C.S2-b's "legit scrollbar, not a bug" verdict. ROI
luma OFF==ON (delta 0.0): the hub flag does not touch this screen.

**Subitem (c) part_difficulty — VERIFIED CLEAN, no regression.**
`part_difficulty_off.png`: song header ("2. 25 OR 6 TO 4  CHICAGO"),
"SETLIST (2 SONGS)", Chicago album art, "RIGHTY MODE / SONG DIFFICULTY" panel,
and "CHOOSE INSTRUMENT / GUITAR / BASS" card all render correctly and stably.
**No black poster quads** — confirms C.S2-c's "mid-zoom venue backdrop, not a
widget bug" verdict (this capture has no `part:` press, so no camera zoom into
the poster wall). (The grey-skinned band member on the right is the separate
char-skinning lane, not a W4.1 artifact.)

**Cross-screen regression check.** The flag is scoped to `MainHubPanel::Poll`
(`src/band3/meta_band/MainHubPanel.cpp`) and cannot alter song_select or
part_difficulty rendering. Whole-frame OFF-vs-ON diffs (song_select ~8%,
part_difficulty ~12%, main_hub ~73%) are **boot-to-boot animation variance**
(venue-backdrop parallax + character idle at different frame-arrival phases
across two separate boots) — NOT the flag; the identical song_select ROI luma
confirms this. No regression.

**File fence — RESPECTED.** `git show --stat` on all five lane commits:
- `b537d275` (the fix): only `src/band3/meta_band/MainHubPanel.cpp` (+20 lines,
  `#ifdef HX_NATIVE`).
- `69fffdac`/`1665e8e5`/`d04b0076`/`3c145299`: docs + harness under
  `W4.1/` only.
- Nothing touches `native/src/rb3_render_hook.cpp` or any engine
  (`../milo-native-engine/**`) render/gfx file. Engine change is the single
  append-only classification.json row (engine `8e38e74`).

**Flag registration — CONFIRMED.** `RB3_HUB_MENU_QUAD_HIDE` present in the
engine `NativeCompatFlags.classification.json` (class `workaround`, default
`off`, owner `ui/hub`).

**Verdict: whole lane PASSES.** Each claimed fix is visibly present, no
cross-screen regression, fence respected, flag registered. No fix-forward /
revert needed.

Captures: `/tmp/wave6-cs5-verify/`. Harness:
`W4.1/harness/lane-verify-capture.py`.

## C.S3 — hub-quad flip package (Sonnet) — done — 2026-07-06 (Wave 7)

**Deliverable:** `W4.1/hub-quad-flip/` (README + REVIEWER_CHECKLIST + `capture.py` +
`captures/`) — the coordinator flip-decision package for `RB3_HUB_MENU_QUAD_HIDE`, per the Wave-7
kickoff's Lane C S3 brief. Packaging only, no flip performed.

**Build:** fresh `native/build-agent-C-S3-hubquad` (Clang/Debug — RelWithDebInfo+GNU first
attempt failed link with unrelated undefined refs `MsToTick`/`TickToMs`/`SystemConfig`/
`SongDB::IsInCoda`, resolved by matching the campaign's standard Debug config used by every other
`build-agent-*` dir). Engine HEAD at capture time: `7943bfa` (one commit past the pinned
`1b045d9`, includes W4.2-fix `031461a` + W3.3-fix `7943bfa`, both landed default-OFF this wave).
rb3 source untouched by this stage.

**Method:** `capture.py` boots `rb3-native` headless to `main_hub_screen` **four times** — OFF,
OFF, ON, ON (`RB3_HUB_MENU_QUAD_HIDE` unset/unset/`=1`/`=1`) — a genuine two-boot A/A pair per
state, not one shot per side. W4.2's `RB3_UI_TEXT_FLOOR_RELAXED` and W3.3-fix's postproc ceiling
flag were both left at their current default (OFF/unset) in every capture, per the kickoff
amendment A7 (pin the sibling Lane C flag OFF during this screen's captures so the two decisions
don't confound). ROI crops: quad ROI `x[380,900] y[365,410]`, PLAY-NOW label ROI
`x[140,700] y[195,330]`.

**Findings (see README for full detail):**
1. Quad renders per flag state, A/A-stable across 2/2 + 2/2 boots (`montage_2x2_off_vs_on.png`,
   `quad_roi_off_vs_on.png`).
2. **PLAY NOW / QUICKPLAY / START A ROAD CHALLENGE label is unaffected by the hide** — the
   blocker check named in the stage brief. `label_roi_all_4.png` shows the label ROI from all
   four boots stacked: text present, legible, pixel-similar regardless of flag state.
   `playnow.lsw` is a sibling to the label object, not its parent/container, so this is expected
   and confirmed, not a surprise. **No blocker found** — package proceeds as a normal flip
   recommendation.
3. Retail ground truth (`images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`) has no analog of
   the capsule anywhere on `main_hub` — matches flag-ON, contradicts flag-OFF
   (`retail_vs_flagon.png` / `retail_vs_flagoff.png`).

**Recommendation:** clean flip candidate — flag-OFF is native-only render debris with no retail
counterpart, flag-ON matches retail, and the one named risk (label survival) is proven clean.
`REVIEWER_CHECKLIST.md` gives the coordinator a 7-point walk (mirrors the Wave-5 W2.1-flip
checklist format) before executing; also notes the actual flip mechanism is inverting
`MainHubPanel.cpp:130`'s `getenv("RB3_HUB_MENU_QUAD_HIDE")` opt-in read to an opt-out (matching
the `RB3_PLACEMENT_CONTRACT_OFF` / `RB3_BLACK_HEAD_FIX_OFF` pattern already used twice this
campaign), not a bare classification.json default edit.

**Fence — RESPECTED.** Touched only `W4.1/hub-quad-flip/` (this package). No `src/`, no engine
files, no classification.json edits, no default flipped. Never flip (per brief) — honored.

Captures: `docs/native/engine-arch-review-2026-07-05/execution/W4.1/hub-quad-flip/captures/`
(committed). Raw working dir (uncommitted, regenerable): `/tmp/w41-hubquad-flip-cs3/`.
