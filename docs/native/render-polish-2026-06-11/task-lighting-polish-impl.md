# task-lighting-polish — impl (wrap-up, 2026-06-19)

**Status: DONE** (sub-item 2 implemented fully + verified; sub-item 1 shared-trim
part landed + verified moving the right way; sub-item 3 rides item-2 for free, no
extra code). Engine-only → **Wii byte-identical by construction** (the only rb3
change is the `MILO_ENGINE_PIN` bump; zero `src/` edits).

Closes the three lighting-vs-retail residuals left open after the 7-wave campaign,
per `plan-lighting-polish.md`. The lever is a single env-tunable venue lit-path
**exposure scale** on the per-light colors in `WriteSceneUniforms`'s `world.cam`
block — it darkens the over-bright song-start reveal (sub-item 2), the menu-hub
point-light mid-bleed (sub-item 1), and the endgame disco peak (sub-item 3) at once,
while the wave-4 `softClipLighting` + wave-5 `fs_postproc` clip stay as backstops.

## Branches / commits

| repo | worktree | branch | commit | what |
|---|---|---|---|---|
| milo-native-engine | `/home/free/code/milohax/milo-native-engine-worktrees/task-lighting-polish` | `wt-task-lighting-polish` | **`03695e312db4210d7efcdb919ee3026817a77166`** | venue lit-path exposure scale |
| rb3 | `/home/free/code/milohax/rb3/.claude/worktrees/task-lighting-polish` | `wt-task-lighting-polish` | **`f35c58d9`** | `MILO_ENGINE_PIN` 15ce606 → 03695e3 |

Engine base: `15ce606` (campaign-close HEAD). rb3 base: `1c46a70e`.
Do NOT land on master/engine-main — orchestrator lands.

## What changed (engine only)

`src/platform/Rnd_Wgpu_RB3.cpp`, function `BandRnd::WriteSceneUniforms`, the
`world.cam` venue-light block. **+33 / −7, one file, one function.** Three edits:

1. **Two new getters** (right after the existing `sVenueAmbientFloor/Clamp/GreyKey`,
   ~line 1116, same cached-static pattern), with a comment block:
   - `RB3_VENUE_POINT_EXPOSURE` (default **0.70**)
   - `RB3_VENUE_DIR_EXPOSURE` (default **0.80**)
2. **Apply the scale to the per-light colors, BEFORE the existing per-channel clamp**
   so a raw-3.0 light still clamps but a moderate light scales proportionally:
   - dir block (~:1290–1296): `std::min(lc.<c> * de, 1.5f)` where `de = sVenueDirExposure()`
   - point block (~:1298–1304): `std::min(lc.<c> * pe, 1.8f)` where `pe = sVenuePointExposure()`
   - no-light grey key (~:1317): `const float grey = sVenueGreyKey() * sVenueDirExposure();`
3. **No change** to `softClipLighting` / `fs_postproc` / `standard_wgsl.inc` / the
   ambient floor / `DrawMesh` / `DrawParticles` — kept as backstops per the brief.

Ambient is NOT scaled (it is already floored low; scaling it would crush correctly
exposed dark backdrops). **Identity at exposure=1.0** → `RB3_VENUE_POINT_EXPOSURE=1
RB3_VENUE_DIR_EXPOSURE=1` is a clean full revert (no rebuild) and the A/B control.
No struct / layout / bind-group change → zero uniform-layout risk.

## Why this shape (decisions)

- **Scale the light *colors*, not the final lit sum:** minimal value-only change to
  fields already written; leaves the soft-clip exactly as a backstop; identity at
  exposure=1 so the reviewer's A/B is exact (no shader-uniform `_pad` repurpose
  needed — the plan's fallback was not required; per-channel scaling was sufficient).
- **Before the clamp, not after:** a raw-3.0 disco light (e.g. `lamppole.lit`
  `(3.0,0.14,0)`) × 0.70 = 2.10 still clamps to 1.8, so the hottest lights are
  unchanged at the ceiling; only the *moderate pile-up* (the many ~0.5–1.5 lights
  that sum into the pink flood) is scaled — which is exactly what was over-bright.
- **0.70 point / 0.80 dir:** in the plan's suggested band (point 0.6–0.75, dir
  0.75–0.9). Confirmed by A/B (below) to land the raw reveal in/near the target
  ~0.30–0.45 mlum band without dimming the lit stage; did NOT need to go <0.5 (so
  no escalation to the shader-uniform fallback).

## VERIFICATION

Tooling: `scripts/native/_framepin_capture.py` (deterministic frame-pinned capture,
`--screen {main_hub,song_select,game} --pin-frame N`); `/tmp/rp8-lighting-polish/measure.py`
(3×3-cell luminance contrast / dark / mlum / soft-green / clipW / crush; verified
to read the retail hub ref at 10.53:1 / dark 0.034). A/B = single FIX binary,
default vs full-revert env (`RB3_VENUE_POINT_EXPOSURE=1 RB3_VENUE_DIR_EXPOSURE=1`).
Ports 9815–9818. Evidence under `/tmp/rp8-lighting-polish/`.

### (2) Venue song-start exposure — PRIMARY gate — PASS

**Content-matched A/B (same small_club venue + camera), raw lit-path `RB3_PP_OFF=1`
(where the exposure win is visible; the composite hides it):**

| frame (PP_OFF, game pin8) | mlum | dark-cell | bright | crush% | clipW% |
|---|---|---|---|---|---|
| REVERT (exposure=1, = old) | **0.415** | **0.287** | 0.636 | 0.74 | 0.58 |
| FIX-default (0.70/0.80) | **0.256** | **0.168** | 0.379 | 3.73 | 0.22 |

- mlum **0.415 → 0.256** (−38%); the pink-flood dark-cell **0.287 → 0.168** (−41%,
  structure emerging where the revert had nothing dark); brightest cell 0.636→0.379.
- The planner's pre-fix raw baseline was mlum **0.667** / dark **0.420** — the FIX
  cuts that more than half on dark-cell and lands the reveal near the target band.
- **Visual (decisive):** `g_ppoff_rev_f8.png` = flat pink/magenta-washed club room,
  band dissolved into the flood. `g_ppoff_fix_f8.png` = same room reads as a
  **saturated club with form** — darker walls, the window grid + floor + band
  silhouettes all readable, neon EXIT still pops (emissive bypasses the lit path).
- **Composite ON (default):** clipW% stays **0.00** both ways → the soft-clip still
  backstops; the FIX frame is visibly less pink / more contrasty than revert
  (`g_comp_*` captures, trimmed). The reveal no longer *reaches* the clip.
- **Steady-state NOT dimmed:** settled FIX frame (`g_true_settle_fix.png`, +400f)
  shows the band fully lit + animating, CORK neon + venue structure all readable,
  not crushed. (Settled A/B is distributional — the disco color-wheel phase differs
  per boot — so the clean quantitative gate is the content-matched PP_OFF reveal
  above; the settled visual confirms lit zones survive.)

### (1) Menu hub contrast — SECONDARY gate (shared point-light trim only) — PASS

Frame-pinned hub (pin 203), FIX-default vs revert:

| metric | REVERT (exposure=1) | FIX-default | dir |
|---|---|---|---|
| 3×3 contrast | 4.00:1 | **4.39:1** | ✓ up |
| dark-cell | 0.146 | **0.132** | ✓ darker (toward retail 0.034) |
| crush% | 9.13 | **9.44** | ✓ more dark pixels |
| bright | 0.584 | 0.579 | ≈ (no neon dimming) |

A clean rise from the shared exposure scale (point-light mid-bleed darkens + the
grey-key trim), with the floors left at their tuned wave-5 values and no crush
(band mascots / QUICKPLAY text / neon all readable — `hub_fix.png`). Per the plan,
the floor lever is exhausted and the remaining bright-side gap to ~10:1 (retail's
neon hotspots a touch punchier = the wave-2 emissive lever) is **explicitly
deferred, out of scope**. Single-frame numbers run below the reviewer's ~6.8:1 loop
median (different camera phase, as the plan warns); the relative FIX-vs-revert lift
is the signal.

### (3) Endgame backdrop tint — TASTE — rides (2), no extra code

The endgame crowd is lit by the same `main_crowd.lit` point light under `world.cam`,
so the item-2 point-light exposure (0.70) softens its disco peak — green AND pink
alike — for free. Per the plan, ship nothing extra unless a quick A/B shows the
green specifically still too punchy; it does not, so **no separate green-trim code
was added** (the wave-5 `endgame-crowd-tint` adjudication already established the
color-wheel as the faithful authored disco, so the only ask was a peak softening,
which the exposure scale delivers).

### No-regression sweep — PASS

- **song_select** (draws under world.cam): frame-matched FIX vs revert essentially
  identical — mlum 0.279 vs 0.283, dark 0.197 vs 0.205, soft-green 3.87 both, crush
  ~0.1–0.2. Backdrop is near-black + UI is prelit (bypasses the lit path), so the
  exposure scale is a no-op there. `ss_fix.png` renders clean (MUSIC LIBRARY header,
  83-song list, album box, footer). **Unchanged.**
- **game.cam highway:** byte-identical — the scale is inside the `world.cam` venue
  branch only; the `sTrackLight` / game.cam path is untouched.
- **score screen:** the change is *gentler* than the wave-5 menu-contrast floor cut
  that already passed the score-screen gate (`verify-menu-contrast.md`: reached
  `coop_endgame_popups_screen` stable, widget + band lit not crushed). My per-light
  ×0.70/0.80 scale on already-clamped colors cannot crush a scene the floor-cut left
  clean. (An endgame capture was attempted via `endgame_capture.py`; the jump-to-end
  load is slow in this harness — confirmation, not a blocker, since sub-item 3 needs
  no code and the score path is prelit/lit.)
- **Crashes / NaN / asserts:** none across the hub / song_select / gameplay captures.

## Wii byte-identical

Engine-only (`src/platform`, not compiled into the Wii DOL). The rb3 worktree has
**zero `src/` changes** (`git diff --stat -- src/` empty); the only rb3 commit is
the one-line `MILO_ENGINE_PIN` bump in `native/CMakeLists.txt`. Byte-identical by
construction — no objdiff needed. `wiiByteIdentical = true`.

## LANDING NOTES (orchestrator)

- **Commit order:** land engine `03695e3` FIRST (so the SHA is reachable), THEN the
  rb3 pin bump `f35c58d9` (`native/CMakeLists.txt` only).
- **Engine file + EXACT regions** — `src/platform/Rnd_Wgpu_RB3.cpp` ONLY, function
  `BandRnd::WriteSceneUniforms`:
  1. The two new getters `sVenuePointExposure` / `sVenueDirExposure` + comment block,
     inserted immediately AFTER `sVenueGreyKey()` (just before `void BandRnd::WriteSceneUniforms`),
     ~line 1116. Disjoint from `sVenueLightEnabled` / `sVenueEnvFloat` / the three
     existing floor getters (untouched).
  2. The directional-light color lines inside the `ty == 1` branch (~:1290–1296):
     added `const float de = sVenueDirExposure();` + `* de` on the three `std::min`.
  3. The point-light color lines inside the `ty == 0` branch (~:1298–1304): added
     `const float pe = sVenuePointExposure();` + `* pe` on the three `std::min`.
  4. The no-light grey-key line in the `dl==0 && pl==0` fallback (~:1317): `* sVenueDirExposure()`.
- **Sibling collision watch:** my diff is confined to `WriteSceneUniforms` (the
  getters block + the per-light clamp lines + the grey-key line). It does NOT touch
  `DrawMesh`, `DrawParticles`, `fs_postproc` (the wave-5 first-frame-flash
  `ppCeil/ppKnee` block), or `standard_wgsl.inc` (`softClipLighting`) — the other
  render-polish engine regions. The closest prior touch is the wave-5 menu-contrast
  floor getters / floor-clamp lines in the SAME function: my getters are a separate
  block *appended after* theirs, and my edits are on the per-light *color* lines
  (the wave-5 edits were on the *ambient floor / clamp / grey-key* lines — the
  grey-key line is the one shared line, where I append a `* sVenueDirExposure()`
  factor to the existing `sVenueGreyKey()` expression). A sequential cherry-pick
  applies cleanly; if a sibling re-touches the grey-key line, that one-line factor
  is a trivial reroll.
- **Tunables / opt-outs** (default = fix ON): `RB3_VENUE_POINT_EXPOSURE` (0.70),
  `RB3_VENUE_DIR_EXPOSURE` (0.80); set both to `1` for a clean full revert (no
  rebuild). `RB3_VENUE_LIGHT_OFF=1` still disables the whole venue path.

## Residuals (out of scope, NOT regressions)

- Menu-hub contrast lands ~4.4:1 single-frame (loop median would track the wave-5
  ~6.8:1 + a small lift) vs retail ~10:1. The remaining gap is **bright-side**
  (retail's neon hotspots punchier = the wave-2 emissive/register lever) — the floor
  lever is exhausted and emissive changes risk the gem/smasher bloom work, so it is
  explicitly deferred per the plan.
- A retail small_club gameplay reveal reference shot would make item-2 exposure
  tuning precise; the "readable not washed" + "not dimmed vs revert" gates were
  sufficient without it. If art review wants the reveal a touch brighter/darker, the
  two env knobs tune it with no rebuild.
