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
