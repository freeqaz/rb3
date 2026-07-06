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
