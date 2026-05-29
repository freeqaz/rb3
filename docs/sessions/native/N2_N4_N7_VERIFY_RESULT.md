# N2 / N4 / N7 — verification batch result (2026-05-29)

**Context:** dispatched 3 read-only Opus planners for the next visible-impact batch
(N2 void camera cuts, N4 song-select occluder, N7 perceptible load screen) per
`STATUS_AND_NEXT_GOALS.md`. The planners discovered the status doc (authored
2026-05-28, "V34") is **stale** — two of the three were already fixed. Coordinator
then verified ground-truth at HEAD (`1df5db08`) with fresh captures + the Opus
visual gate. Outcome: **N2 done, N4 done, N7 honest-negative.** No new fix landed;
the value was confirming the real state and root-causing N7.

Evidence sweep: `screenshots/n2n4n7-verify/` (HEAD, frames 7/280/360/500/700/1100/3400),
`screenshots/n7-window/` + `screenshots/n7-tv3b/` (load-window fine captures).

---

## N2 — void / wall camera cuts during a song — ALREADY FIXED ✅

- Fixed by commit **`83780743`** ("native: N2 void/wall camera-cut fallback"),
  documented as **V36** in `VENUE_RENDER.md`. HX_NATIVE block in
  `BandDirector::DrawShowing` (`src/system/bandobj/BandDirector.cpp:~335-421`,
  env opt-out **`RB3_CAM_FALLBACK_OFF`** at `:363`). Block intact at HEAD
  (`git diff 83780743 HEAD -- BandDirector.cpp` empty).
- **Verified visually at HEAD:** `n2n4n7-verify/05_f0700` shows a full framed
  club interior (crowd + band + stage + EXIT sign) — this exact frame was a *pure
  void cut* ("two slivers of stage-wall against black") in the V34 status doc.
  `07_f3400` (deep song) shows clean gem highway + venue behind it — was
  "near-total black" in V34. The void-cut tell is gone.
- Residual (out of scope, pre-existing): rare crowd-shot character slivers (N5),
  and the earliest pre-gameplay intro frames can still frame wall (no last-good
  cam to hold yet) — cinematography re-authoring, separate item.

## N4 — song-select grey occluder + SAVE panel — ALREADY FIXED ✅

- Both halves resolved by commit **`9a8a5479`** ("native: N4 song-select fixes —
  hide stray details pane + blank-album placeholder", env opt-out
  **`RB3_NO_DETAILS_FIX`**, glue `rb3_game_input.cpp:~705-731`). The V34 evidence
  frame (`v34-status-review/06_f0280`, mtime 2026-05-28 20:27) **predates** this
  commit (2026-05-29 02:55) — the screenshot was stale.
- **Verified visually at HEAD:** `n2n4n7-verify/02_f0280` — the "SAVE / Are you…"
  overlapping panel is **gone**, and the former opaque grey occluder box is now
  the blank-album **"?" placeholder** thumbnail. The MENU_VOID_DBG2 mesh dump
  found no screen-filling grey `RndMesh` on song-select, consistent with the
  occluder having been the details/album panel (now handled), not a 3-D backdrop.
- Remaining (DATA, not code): the "?" is the missing-album-art placeholder. Real
  thumbnails need `ui/image/` added to the asset-extraction step
  (`orig-assets/extracted/` is local/gitignored). Tracked separately.

## N7 — perceptible load screen — HONEST NEGATIVE (deferred) ⊘

**The load is genuinely instant AND the transition renders black — there is no
animatable loading content to extend.**

- **Screen flow (trace):** `part_difficulty_screen` (f352) →
  **`tv3_a`/`tv3_b`/`tv3_c`_screen`** (f453, "in transition") → `game_screen`
  (f456). There is **no `preloading_screen`** in the quickplay flow; the cinematic
  "load screen" *is* the `tv3_*` transition vignette, committed only ~3 frames.
  (The vignette key is a random pick from `("tv3_a" "tv3_b" "tv3_c")` for
  `small_club` per `config/vignettes.dta:117-120` — runs vary which one shows.)
- **The transition renders as a fade-to-black on native.** Full natural window
  captured (`n7-tv3b/`): f453 still shows the seldiff, **f454 and f455 are pure
  black**, f456 is game_screen. The `tv3_*` vignette worlds contribute **no
  visible content** — the same vignette-world deferred-instancing gap that N3
  (hub backdrop) and N4 chip at, or simply an authored fade-through-black.
- **Therefore a frame-hold is the wrong fix.** Holding the screen for N frames
  (whether glue re-pin or matched-fork `IsLoaded` gate) would display **N frames
  of pure black** — strictly worse than the current quick flash. Confirmed:
  Approach A (glue `GotoScreen` re-pin) was implemented + tested with
  `RB3_LOAD_HOLD_FRAMES=120` and did nothing (it guarded on `preloading_screen`,
  which never appears; reverted). The N7 plan's frame-hold mechanism is sound for
  *animating* a vignette — but the premise (that there IS renderable vignette
  content) is false on native today.
- **What N7 is actually blocked on:** making the `tv3_*` transition vignette
  worlds instance + render their content on native — i.e. the broader
  vignette-world rendering problem (shared root with N3's menu backdrops). Until
  that lands there is nothing to make perceptible. Alternatively, a *custom*
  native animated loading display could be authored, but that is new UI scope, not
  the N7 "make the existing load screen perceptible" ask.
- **Decision:** defer N7. Re-attempt only after vignette-world rendering is
  solved (or as a deliberate custom-loading-UI feature). No code committed.

---

## Net

The batch's headline visible items (N2, N4) were **already complete** at HEAD; the
fresh capture + Opus visual gate confirm both look right. N7 is a documented
honest-negative blocked on vignette rendering. `STATUS_AND_NEXT_GOALS.md` updated
to mark N2/N4 resolved and N7 deferred-with-root-cause. The remaining genuinely-open
items are the deeper ones: vignette-world rendering (unblocks N3-residual/N4-data/N7),
N5 crowd slivers (deep CharServo), N8 hit/flame FX, N9 teardown SIGSEGV, N10 audio
host output, N11 empty save-arrays.
