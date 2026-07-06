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
