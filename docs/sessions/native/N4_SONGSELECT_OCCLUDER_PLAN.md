# N4 — Song-select grey occluder + overlapping SAVE panel — implementation plan

**Authored:** 2026-05-29 (read-only planning subagent, Opus). No source edited,
no build, no commit. Only this doc was written.

**Defect (item N4, `STATUS_AND_NEXT_GOALS.md` §2):** on the Music Library /
song-select screen, a large **opaque grey box** covers the right half of the
song list and a half-drawn **"SAVE / Are y[ou]…"** panel intrudes on the right
edge. Evidence frame: `screenshots/v34-status-review/06_f0280.png`.

**Headline result of this investigation:** the two visible defects have **two
different root causes**, and **one of the two is already fixed in-tree but
LANDED AFTER the evidence frame was captured** — so the screenshot is stale for
that half. Details below; read §6 (Honest assessment) before dispatching.

---

## 0. Timeline / staleness note (load-bearing — read first)

- Evidence frame `06_f0280.png` mtime: **2026-05-28 20:27**, build at engine pin
  `f8a3a379` (2026-05-28 18:59).
- The glue commit `9a8a5479` *"native: N4 song-select fixes — hide stray details
  pane + blank-album placeholder"* landed **2026-05-29 02:55** — i.e. **AFTER**
  the screenshot. It already targets the **SAVE-panel** half of N4
  (`rb3_game_input.cpp:705-731`, gated `RB3_NO_DETAILS_FIX`).
- The glue commit `3600647a` *"N3 fix menu/hub black void"* (2026-05-29 03:16)
  added a matched-fork `RndDrawable::Draw`/`DrawBudget` hook
  (`src/system/rndobj/Draw.cpp`, gated `RB3_MENU_VOID_FIX_OFF`) that skips the
  opaque-black `worldcenter` backdrop box, **plus a reusable `MENU_VOID_SKIP=`
  diagnostic lever** — both directly relevant to the grey-box half of N4.

So this plan splits cleanly: **(A) the grey occluder is NOT yet fixed** and is
the real remaining N4 work; **(B) the SAVE panel is plausibly already fixed** —
the first action is to *re-capture frame 280 at HEAD with the current fixes on*
to confirm which defects survive before editing anything.

---

## 1. Root-cause analysis (two defects, separately)

### Defect (1) — the opaque GREY occluder box (top-right over the list)

**What it is:** the **`sv4` shell-vignette backdrop panel** (`sv4_panel`), a
`BackdropPanel` 3-D backdrop world, partially-instanced natively and rendering
as a single large opaque grey quad over the upper-right of the screen.

Evidence chain:
- `song_select_screen` (`ui/song_select/song_select.dta:1931-1938`) is a
  `BandScreen` with `(panels meta postsong_sfx_panel sv4_panel song_select_panel
  song_select_shortcut_panel song_select_filter_panel)` and its `enter` handler
  is literally `{sv4_panel set_showing TRUE}` (`song_select.dta:1936-1937`). So
  on this screen `sv4_panel` is **deliberately shown** (contrast `main_hub.dta:752`
  which does `{sv4_panel set_showing FALSE}`).
- `sv4_panel` is created by `{new_shell_vignette "sv4"}` (`ui/vignettes.dta:188`).
  `new_shell_vignette` (`vignettes.dta:58-128`) makes a **`BackdropPanel`** whose
  `dyn_file` resolves to `"../world/vignette/shell/%s.milo"` where `%s` is a
  random pick from `config/vignettes.dta` `(backdrops … (sv4 …))`
  (`config/vignettes.dta:29-39` → `sv4_a`/`sv4_b`/`sv4_c`/`sv4_d`/`sv4_e`).
  Those milos exist in the extract:
  `orig-assets/extracted/world/vignette/shell/gen/sv4_{a..e}.milo_xbox`.
- `world/vignette/shell/**` is exactly the dir family the
  `IsDeferredVenueProxy` gate in `WorldInstance::SyncDir`
  (`src/system/world/Instance.cpp:351-358`, DIVERGENCE hack #2) **SKIPS** — so
  these shell backdrops instance only partially natively.
- This is the **same defect class as N3** (the menu/hub black void), which was
  root-caused (`3600647a`) to a single degenerate opaque backdrop box
  (`worldcenter.mesh`, color `(0,0,0,1)`, no diffuse, depth-writing) in the
  `sv8_a` shell vignette occluding the textured sky layers behind it. The
  song-select grey box is the **`sv4` sibling** of that same backdrop-box
  problem: an opaque (grey, not black) backdrop mesh in an `sv4_*` shell milo
  that draws as a flat plate over the list because the textured layers that
  retail composites in front of it (RTT cloud/sky targets, or the rest of the
  partially-instanced world) are absent natively.

**Why grey not black, and why only top-right:** unlike N3's pure-black
`worldcenter` box, the on-screen plate here is mid-grey and confined to the
upper-right quadrant — consistent with either (i) a different shell-vignette
backdrop mesh with a grey/un-prelit material, or (ii) a mesh whose missing
diffuse render-target samples as a flat grey (default/NULL-texture grey) rather
than black. The N3 fix's skip predicate (`MenuVoidIsWorldcenterOccluder`,
`Draw.cpp:40-62`) is **name-gated to `"worldcenter"` AND requires opaque-black**,
so it does **not** catch this grey `sv4` mesh today. **The exact mesh+material is
not yet identified** — it must be confirmed at runtime via the existing
`MENU_VOID_SKIP=`/`MENU_VOID_DBG2=2` levers (see §6) before the fix predicate is
written. This is the honest gap in this plan.

### Defect (2) — the overlapping "SAVE / Are y[ou]…" panel (right edge)

**What it is:** the **`song_select_details` sub-pane** of `song_select_panel`,
left drawing when it should be hidden. It is NOT a literal save dialog — the
"SAVE"/"Are y…" text are the details pane's save/share widgets (its handlers in
`song_select.dta` call `{saveload_mgr autosave}` on share/unshare —
`song_select.dta:346,360` — and it hosts `view_gamer_card.ihp`,
`song_select.dta:1891/1900/1918`). That visual + the `saveload_mgr` coupling is
why the status doc tagged it "`NativeSaveLoadStub`-adjacent / save-prompt"; the
real object is the details pane, which is save/gamercard-adjacent but distinct
from the `SaveLoadStatusPanel` (the Wii memcard write-icon, which is correctly
no-op'd offline — `SaveLoadStatusPanel.cpp:16-22` `#ifndef HX_NATIVE` AddSink
guard).

**Why it leaks natively:** `song_select_panel`'s `enter` handler runs
`{if_else {&& [details_mode] {input_mgr has_user}} {show_details} {hide_details}}`
(`song_select.dta:62-69`). With `(details_mode FALSE)` default (`:19`),
`hide_details` runs, which fires `{details_hide.trg trigger}`
(`song_select.dta:1313-1316`). That PropAnim's terminal `showing=FALSE` keyframe
on `song_select_details` **does not apply natively** — the same PropAnim /
transform-keyframe class of gap documented across the port (cf. V25
`draw_order.grp`, V31 empty save-arrays). So `song_select_details` is left
`Showing()==true` and draws over the right of the list.

**Current state:** the committed glue fix (`rb3_game_input.cpp:705-731`,
`9a8a5479`) enforces the retail invariant directly each frame: on
`song_select_screen`, if `song_select_panel` is `kUp` and its `details_mode`
property is false, force `song_select_details` `SetShowing(false)`. This is the
correct shape (glue, permuter-safe, only force-HIDES when details_mode off so
opening details later is unaffected). **It must be re-verified at HEAD** (the
evidence frame predates it) and checked for the screen-name issue in §6.

---

## 2. Files to edit (tagged; feeds the worktree dep-graph)

The grey-box fix and the SAVE-panel fix are **disjoint files** → can be two
independent worktrees if desired. Prefer doing the verification pass (§5/§6)
first; it may collapse the SAVE-panel half to zero edits.

### Defect (1) — grey occluder

| Layer | File | Function/block | Edit |
|---|---|---|---|
| **(a) matched-fork** | `src/system/rndobj/Draw.cpp` | `MenuVoidIsWorldcenterOccluder` (`:40-62`) + `MenuVoidDrawHook` (`:63-`) inside the existing `#ifdef HX_NATIVE` block | **Generalize** the skip predicate to also catch the identified `sv4` shell-vignette grey backdrop mesh (by name+material signature, once identified at runtime). Additive only; do not touch the `#else`. |

No new file needed — the N3 HX_NATIVE hook + the `MENU_VOID_SKIP`/`MENU_VOID_DBG2`
levers already exist in this exact TU. The fix is an *additive extension of the
existing predicate*, which keeps the matched-fork blast radius to one already-
modified function.

(Alternative, lower-preference: a **(c) glue** approach — force
`{sv4_panel set_showing FALSE}` on `song_select_screen` from `RB3GameInputPoll`,
mirroring the §1(2) details-pane hide. This is permuter-safe and avoids the
matched-fork wipe surface, BUT it would suppress the *entire* sv4 backdrop world
(retail shows a real animated backdrop there), so it trades one wrong look
(grey plate) for another (no backdrop) — acceptable only as a stopgap. The
Draw.cpp mesh-level skip is the better fix because it removes only the
degenerate occluder plate and lets any other instanced backdrop geometry show,
exactly as N3 did for the hub.)

### Defect (2) — SAVE panel (details pane)

| Layer | File | Function/block | Edit |
|---|---|---|---|
| **(c) glue** | `rb3/native/src/rb3_game_input.cpp` | `RB3GameInputPoll`, the `RB3_NO_DETAILS_FIX` block (`:705-731`) | **Likely zero new edit** — verify it fires (screen-name + `LoadedDir()` resolution). If it does not fire because of a screen-name mismatch, fix the `curName == Symbol("song_select_screen")` guard (see §6). |

---

## 3. Fix approach

### Defect (1) — grey occluder (the real remaining work)

1. **Identify the mesh (runtime, mandatory first step — see §6).** Run the
   reproducer with `MENU_VOID_DBG2=2` to dump every mesh reaching
   `Draw`/`DrawBudget` on `song_select_screen` (name / material color / blend /
   zmode / diffuse-tex / world position / vert-extent), and bisect with
   `MENU_VOID_SKIP=<substr>` to find which mesh, when skipped, removes the grey
   plate. Expect an `sv4_*` shell-vignette backdrop mesh (skybox-sized box at
   origin, opaque material, NULL/missing diffuse).
2. **Generalize the predicate.** In `Draw.cpp`, rename/extend
   `MenuVoidIsWorldcenterOccluder` to recognize *both* the N3 `worldcenter`
   black box *and* the identified `sv4` grey backdrop, by an explicit
   name-substring set + material signature (opaque alpha, untextured **or**
   NULL-render-target diffuse, depth-writing). Keep it tightly scoped by
   name+material so it cannot touch gameplay venue / highway / HUD meshes.
   Additive `#ifdef HX_NATIVE` (the block already exists).
3. **Gate:** reuse the existing N3 gate **`RB3_MENU_VOID_FIX_OFF=1`** (this is
   the same defect family — a single "skip degenerate opaque backdrop occluders"
   lever is cleaner than a second env var). **Default: ON** (fix active).
   *If the reviewer prefers a defect-specific lever,* add
   **`RB3_NO_SONGSELECT_OCCLUDER_FIX=1`** (default ON) checked alongside — but
   sharing `RB3_MENU_VOID_FIX_OFF` is recommended for coherence.

### Defect (2) — SAVE panel

- If §5/§6 verification shows the committed `RB3_NO_DETAILS_FIX` block already
  clears it at HEAD: **no code change**; just document N4-defect-2 as resolved by
  `9a8a5479`.
- If it does not fire: correct the screen-name guard
  (`rb3_game_input.cpp:718`). Gate stays the existing **`RB3_NO_DETAILS_FIX`**
  (default ON / fix active).

---

## 4. Regression risks

- **Grey-box skip must not hide a needed mesh.** The N3 predicate is name+material
  scoped; the generalization must stay equally tight (explicit `sv4`/`worldcenter`
  name set + opaque-untextured-depth-write signature). Risk: a too-broad material
  signature could skip a legit opaque backdrop on another screen. Mitigation:
  name-gate to the shell-vignette backdrop names only; A/B every menu screen
  (hub, main menu, seldiff, tour — all of which also mount `sv4_panel`/`sv3_panel`)
  with the fix on vs `RB3_MENU_VOID_FIX_OFF=1` to confirm no other screen loses
  geometry.
- **Do not regress N3.** Sharing the predicate means a bad edit could break the
  hub-void fix; keep the `worldcenter` branch byte-equivalent and only *add* the
  `sv4` branch.
- **Details-pane hide is screen-scoped** (`song_select_screen` only) and only
  force-HIDES when `details_mode` is false, so pressing Options to open details
  (sets `details_mode=1`, runs `details_show.trg`) is unaffected — confirmed by
  reading the guard. Risk that the same `song_select_details` object is needed on
  another screen: it lives inside `song_select_panel`'s milo dir and the guard is
  gated on `curName == song_select_screen`, so it is scoped. Low risk.
- **Permuter:** `Draw.cpp` is matched-fork (permuter-owned). The N3 block is
  intact at HEAD but is a wipe risk; the generalized predicate inherits that risk
  and should be added to the HX_NATIVE audit-block list.

## 5. Verification recipe

Reproducer (unchanged from the task brief), capturing frame 280:

```
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
  RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  MILO_MAX_FRAMES=24000 MILO_SCREENSHOT_DIR=/abs/dir MILO_SCREENSHOT_FRAMES=280 \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
  /home/free/code/milohax/rb3/native/build-native/rb3-native
```

1. **Baseline-at-HEAD capture (do this BEFORE any edit):** run as above and
   inspect frame 280. Determine which of the two defects still appear at HEAD
   (the SAVE panel may already be gone via `9a8a5479`).
2. **Grey-box identification:** add `MENU_VOID_DBG2=2` and grep the stderr dump
   for meshes drawn on `song_select_screen` with opaque material + grey/NULL
   diffuse; then `MENU_VOID_SKIP=<candidate-substr>` and re-capture 280 to
   confirm the plate disappears.
3. **Post-fix capture:** with the generalized predicate, frame 280 must show a
   **clean full-width song list, no grey box, no SAVE pane.** A/B with
   `RB3_MENU_VOID_FIX_OFF=1` (grey box returns) and `RB3_NO_DETAILS_FIX=1` (SAVE
   pane returns) to prove each lever owns its defect.
4. **No-regression sweep:** confirm hub (`01_f0007`-class), main menu, seldiff,
   gameplay highway frames are unchanged with the fix on (the predicate must not
   drop any non-occluder mesh). Full 24000-frame run must still exit 0.

## 6. Honest assessment

- **Defect (2) — SAVE panel:** **probably already fixed** by `9a8a5479`
  (`RB3_NO_DETAILS_FIX`); the evidence frame predates that commit. **One thing to
  verify:** the glue guard keys on `curName == Symbol("song_select_screen")`
  (`rb3_game_input.cpp:718`), and the screen IS named `song_select_screen`
  (`song_select.dta:1933`) — but the *reproducer script* targets
  `msg:music_library:...`, and the `music_library` object is the Synchronizable
  (`MusicLibrary.cpp:182` `SetName("music_library", …)`), **not** the screen. So
  the screen name is right and the guard should fire; still, confirm at runtime
  (step 5.1) before declaring it done. Confidence the details-pane hide is the
  correct mechanism: **high**. Confidence it currently fires at HEAD: **medium**
  until re-captured.
- **Defect (1) — grey occluder:** root-cause *class* is identified with **high
  confidence** (the `sv4` shell-vignette `BackdropPanel`, shown by
  `song_select.dta:1937`, same deferred-`world/vignette/shell` backdrop-box
  family as the N3 `worldcenter` box). The **exact mesh + material signature is
  NOT yet identified** — this is read-only analysis and the grey plate's precise
  source must be pinned with the existing `MENU_VOID_DBG2=2` / `MENU_VOID_SKIP=`
  runtime levers **before** the Draw.cpp predicate is written. **Do not write the
  skip predicate blind.** Step 5.2 is a mandatory pre-edit runtime step.
  Confidence in the fix *location* (`Draw.cpp` N3 hook) and *approach*
  (extend the predicate, share `RB3_MENU_VOID_FIX_OFF`): **high**. Confidence in
  the exact predicate without the runtime dump: **low** — hence the dump-first
  requirement.

**Overall:** N4 is **smaller than the status doc implies** — likely one real
edit (generalize the N3 `Draw.cpp` occluder predicate to the `sv4` grey box) plus
a verification that the already-committed details-pane hide clears the SAVE
panel. File scope for the dep-graph: **`src/system/rndobj/Draw.cpp` (matched-fork,
shared with N3 — serialize against N3/any Draw.cpp owner)** and
**`rb3/native/src/rb3_game_input.cpp` (glue, shared with N6/N7 input-verb owners
— serialize input edits)**.
