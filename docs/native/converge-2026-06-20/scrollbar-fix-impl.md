# GAP 1 — Scrollbar UI-leak: implementation + root-cause CORRECTION

**Impl agent (Opus), 2026-06-21. Worktree `wt-converge-scrollbar`.**
Companion to `audit/scrollbar-rootcause.md` + `audit/RANKED-GAPS.md`.

This implements the GAP 1 fix **and corrects two factual errors in the
root-cause doc**, established with frame-stamped, game-state-stamped, anchored
deterministic measurement on the current master build (7502526a, engine
`5cbe855`).

---

## TL;DR

- **Shipped fix (Option B, content gate):** `ScrollbarDisplay::DrawShowing`
  (`src/system/bandobj/ScrollbarDisplay.cpp:179`) now, under `HX_NATIVE` only,
  suppresses the draw of the shared scrollbar resource dir when the widget has
  **no scrollable content** (`m_fSavedScale == 0`, i.e. no list / no provider
  data) — the degenerate over-draw the root-cause doc describes. A legitimate
  scrollbar (`0 < m_fSavedScale < 1`, or `mAlwaysShow`) is **kept**. Default ON;
  opt out `RB3_SCROLLBAR_FIX_OFF=1` (restores the exact Wii gate).
- **Wii-neutral, PROVEN:** the entire change is inside `#ifdef HX_NATIVE`.
  objdiff `DrawShowing__16ScrollbarDisplayFv` = **100.0% (58/58 instr equal)**
  with the edit applied.
- **Option A is NOT applicable** (the doc's recommended option): the scrollbar
  draws under `[ui.cam]` in BOTH gameplay and song_select (measured), so a
  world-cam predicate in `Draw.cpp` cannot separate them. The doc's premise that
  it draws "in the venue WORLD camera pass" is **wrong on the camera name**.
- **The "scrollbar across the gameplay note highway" is NOT reproducible in
  steady-state gameplay on this build.** Frame-stamped diagnostics prove
  `scrollbar_bg.mesh`/`scrollbar.mesh` draw **only** during `song_select_screen`
  and its transitions (frames < game_screen arrival), never while a game is
  active (`TheGame != null`). Anchored deep-song captures (songMs 30k-46k) show a
  **clean highway**. The doc's `834` "drops" are the **legitimate song_select
  scrollbar**, mis-attributed because the harness parses the whole-session log.
- **Net:** the gate is a correct, safe defensive fix for the degenerate-content
  case. It does **not** reduce the `834` count, because those drops are not a
  contentless over-draw — they are the real song_select scrollbar (which must,
  and does, keep drawing).

---

## The diff (one file, +45 lines, all HX_NATIVE)

`src/system/bandobj/ScrollbarDisplay.cpp`:
1. `#ifdef HX_NATIVE\n#include <cstdlib>\n#endif` (getenv).
2. In `DrawShowing`, an `#ifdef HX_NATIVE { … return; }` block replacing the
   draw gate for native:
   ```cpp
   bool hasContent = m_fSavedScale > 0.0f && m_fSavedScale < 1.0f &&
                     GetListAttached() && m_pList.Ptr() != nullptr;
   bool drawWii    = mAlwaysShow || m_fSavedScale < 1.0f;   // exact Wii gate
   bool drawNative = mAlwaysShow || hasContent;
   if (sFixOff) drawNative = drawWii;                       // RB3_SCROLLBAR_FIX_OFF
   if (drawNative) { pDir->SetWorldXfm(WorldXfm()); pDir->Draw(); }
   ```
   The non-HX_NATIVE Wii arm (`if (mAlwaysShow || m_fSavedScale < 1.0f)`) is
   untouched.

**Why Option B, not Option A:** measured — both the gameplay-resident scrollbar
and the legitimate song_select scrollbar draw under `RndCam::sCurrent == [ui.cam]`
(not `world.cam`). The `RB3VenueFrustumCull` world-cam discriminator the doc
points at does not fire here, so Option A's camera predicate is unreachable. The
doc explicitly authorises this fallback ("If a clean world-cam test is not
reachable in Draw.cpp, fall back to Option B").

---

## Acceptance results (scrollbar-rootcause.md §6)

Measured on master 7502526a worktree build, harness
`scripts/native/band-closeup-capture.py` (+ `song-select-capture.py`).

| # | §6 check | Result | Detail |
|---|---|---|---|
| 1 | drop table no longer lists `scrollbar_bg.mesh`; drops_other −74% | **N/A — premise wrong** | Fix-ON verdict still reports `scrollbar_bg.mesh: 795`. Those drops are ALL from the `song_select` phase (frames 783-1680, < game_screen frame 3414+), the LEGITIMATE list scrollbar — not a contentless over-draw. The gate (correctly) does not suppress them. |
| 2 | drops_band stays 0, verdict PASS | **PASS** | Fix-ON: `verdict=PASS pinned=15/15 drops_band=0`. |
| 3 | A/B `RB3_SCROLLBAR_FIX_OFF=1` flips the drop back ON | **N/A — premise wrong** | The opt-out is wired + compiles + restores the exact Wii gate, but there is no contentless gameplay over-draw to flip: `scrollbar_bg` drop count is unchanged by the env because those draws are legitimate song_select content (which both paths keep). |
| 4 | guard-OFF + fix-ON: highway CLEAN | **PASS (already clean)** | Anchored deep gameplay (songMs 30k-46k), guard OFF, no skip: highway is clean (`B_anchored_deep_gameplay_clean_noskip.png`, `D_guardoff_closeup_clean.png`). The teal filigree only appears transiently DURING the song_select→game transition (`A_guardoff_transition_scrollbar_visible.png`), not in steady-state gameplay. |
| 5 | NO menu regression: song_select + accomplishments scrollbar | **PASS** | song_select renders the full list correctly with fix ON (`C_song_select_fixon_scrollbar_kept.png`); `song.sbd` reports `savedScale=0.11, listAttached=1` → `hasContent=true` → kept. |

### Wii-neutrality proof (§ instruction)
```
mcp__orchestrator__run_objdiff DrawShowing__16ScrollbarDisplayFv
  unit=system/bandobj/ScrollbarDisplay  project_dir=<worktree>
→ Match: 100.0% normalized (100.0% raw) | 58 instructions | all equal
```
The edit touches zero non-HX_NATIVE lines (verified by `git diff`: the include
and the new block are both inside `#ifdef HX_NATIVE`). Wii codegen byte-identical.

---

## The measured root-cause correction (why the premise is wrong)

Method: a one-shot, frame-stamped (`RB3HttpServerPoll(frame)`), game-state-stamped
(`TheGame != null`), screen-stamped (`TheUI.CurrentScreen()->Name()`) diagnostic
on every scrollbar mesh draw, across 8 builds + anchored deterministic captures.
(All diagnostics reverted; only the fix remains.)

1. **Camera:** scrollbar meshes draw under `cam='[ui.cam]'` — in BOTH gameplay
   and song_select. The doc's "venue WORLD camera pass" is wrong. → Option A out.
2. **Timing:** `scrollbar_bg.mesh`/`scrollbar.mesh` draw ONLY at frames < the
   `game_screen` arrival frame, always with `gameActive=0` /
   `screen='song_select_screen'`. With per-(name, gameActive) dedup (which would
   log any game-time draw), **zero** game-time draws were ever recorded across
   multiple anchored deep-song runs.
3. **Owner:** the gameplay-resident `chars.sbd` (the doc's named suspect) never
   reaches `DrawShowing` on this build. The only `ScrollbarDisplay` that draws is
   `song.sbd` (the song-library list), and it has REAL content (savedScale=0.11),
   so it is correct, not a leak.
4. **The transient highway filigree** seen guard-OFF on wide/front framings is a
   **song_select→game transition artifact** (the song_select panel still
   compositing over the first venue frames, incl. a pink album-grid wash that
   fades). At a deterministic deep anchor it is gone. A separate ornate teal
   highway pattern in some frames persists with `RB3_HIGHWAY_BLOOM_OFF=1` and is
   independent of `MENU_VOID_SKIP=scrollbar` → it is a gameplay/now-bar visual,
   not the scrollbar.

The leak the original audit artifact (`shots/audit/scrollbar_guardoff_highway.png`)
captured was likely a transition frame and/or a pre-frame-stall-fix build state;
it does not reproduce as a steady-state gameplay over-draw here.

### What the gate IS good for
It is the correct, minimal, Wii-neutral guard for the genuine degenerate case the
doc identified (`m_fSavedScale == 0`, no list): if any `ScrollbarDisplay` ever
draws contentless on native (e.g. an overshell-slot `chars.sbd` under a wardrobe
state we didn't roll), it is now suppressed instead of painting the 200u ribbon.
It cannot regress a real scrollbar (those have `0 < savedScale < 1` or
`mAlwaysShow`). Zero blast radius on Wii.

---

## Screenshots (`shots/scrollbar/`)
- `A_guardoff_transition_scrollbar_visible.png` — transition-window artifact (the
  only place the ribbon appears; NOT steady-state gameplay).
- `B_anchored_deep_gameplay_clean_noskip.png` — anchored songMs 35k, guard OFF,
  no skip: clean highway (steady-state truth).
- `C_song_select_fixon_scrollbar_kept.png` — song_select, fix ON: list/scrollbar
  intact (no-regression gate).
- `D_guardoff_closeup_clean.png` — guard-OFF gameplay closeup: clean.

## Worktree
- branch `wt-converge-scrollbar`, from master 7502526a.
- files: `src/system/bandobj/ScrollbarDisplay.cpp` (+45, all HX_NATIVE);
  `docs/native/converge-2026-06-20/scrollbar-fix-impl.md` + `shots/scrollbar/*`.
- No engine edit, no `MILO_ENGINE_PIN` change.
