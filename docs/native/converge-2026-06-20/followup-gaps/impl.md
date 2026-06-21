# Converge follow-up gaps — IMPLEMENTATION (GAP B(a) crowd-dim + GAP A1 watermark-dim)

**Impl agent (Opus), 2026-06-21.** Both fixes landed in the paired engine worktree
as two separate commits. All edits are in
`../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` — the **RB3-only** GPU backend
TU. **NOT pushed; master pin NOT bumped** (the coordinator consolidates).

| | |
|---|---|
| engine worktree | `/home/free/code/milohax/milo-native-engine-worktrees/converge-render2` (branch `wt-converge-render2`, from `a360e3c`) |
| GAP B(a) crowd-dim commit | **`ada6e56`** |
| GAP A1 watermark-dim commit | **`b8f3cfa`** |
| rb3 worktree | `/home/free/code/milohax/rb3/.claude/worktrees/converge-render2` |
| build | `cmake --build native/build-native --target rb3-native` (gpu backend flavor: `rb3`) |

---

## DC3-safety proof (both fixes)

`MILO_ENGINE_GPU_BACKEND` selects the backend TU in engine `CMakeLists.txt`:
`dc3` → `list(APPEND … Rnd_Wgpu.cpp)` (line 364); `rb3` → `list(APPEND …
Rnd_Wgpu_RB3.cpp)` (line 366). They are **mutually exclusive** — DC3 never compiles
`Rnd_Wgpu_RB3.cpp`. Both fixes live entirely in that file → **DC3 byte-identical by
construction**, zero ifdef/flag needed. No `src/system`/shader change was made, so
nothing shared with DC3 is touched. **A2 was NOT needed** (see GAP A below), so the
shared `standard_wgsl.inc` is untouched. Wii-neutral (HX_NATIVE render path only).

---

## GAP B(a) — big_club white crowd → dim (commit `ada6e56`)

### The discriminator was WRONG in the plan — measured root cause is different

The lighting/synthesis docs assumed the visible white was the **skinned crowd
CHARACTERS** (`female_crowd_body*`, `male_extras_skin*`, …) and that a
`mu.color *= K` on those would darken them. **It does not.** I instrumented it
exhaustively:

- The skinned crowd bodies DO go through my classifier (`isCrowdOrExtras=1,
  bandMember=0`) and DO get `mu.color` multiplied. But forcing them `mu.color=0`
  (and even forcing them **magenta + prelit + unlit**) left the visible white crowd
  **fully white and unchanged** (`CROWD_FORCE_MAGENTA` test). Forcing **every NAMED
  world.cam mesh** magenta turned the band + venue magenta but the white crowd stayed
  white (`DIM_EVERYTHING` w/ text-excluded). Forcing **every** world.cam mesh
  (incl. text-tagged) magenta finally removed the white → the white is a
  text-heuristic-tagged family.
- `EMPTYNAME_PROBE` pinned it: the dominant white is **~9000 four-vertex quads per
  frame** drawn under `world.cam` with an **EMPTY mesh name** (so `isTextMeshHeur`
  mis-tags them as text and every prior fix skipped them), an **empty-named shared
  material**, `color=(1,1,1)`, a near-white baked impostor diffuse, `blend=1`,
  non-skinned. These are the **2D bowl-IMPOSTOR crowd billboards**. They are the ONLY
  empty-name `world.cam` draws that are NOT a Pentatonic/UI font quad (real text is
  `[ui.cam]`/`overshell.cam`, `blend=3`, named font material).

### The fix

In the `if (mat)` material block (`Rnd_Wgpu_RB3.cpp` ~5513, where `mat/mu/owner/
mesh/meshName/matName/skinned/isTextMeshHeur/isLikelyUiText` are all in scope), dim
the BASE color of two crowd families (white% is exposure-invariant, so the base is
the only lever):

1. **impostor billboards** — exact discriminator:
   `world.cam && !skinned && isTextMeshHeur && matName[0]=='\0'`.
2. **skinned crowd/extras characters** — `char/crowd/*`|`char/extras/*` skeleton or
   `crowd`/`extra` mesh name, with the band hard-excluded by the
   `skeleton_unshared.milo` skeleton walk (mirrors SHARD_GUARD :5154-5162). Lower
   magnitude but same intent.

Default factor **0.10** (the impostor diffuse is near-white, so the multiplier must
be small; the plan's 0.30 barely moved it, 0.0 removes the crowd entirely). Opt out
`RB3_CROWD_DIM_OFF=1`; tune `RB3_CROWD_DIM`.

### A/B numbers (big_club_01, `coop_dir_crowd` pin, foreground crowd strips, 6-frame avg)

| metric | OFF (baseline) | ON (default 0.10) | gate |
|---|---|---|---|
| crowdR white% | 17.4 | **0.0** | → ~0 ✓ |
| crowdL white% | 11.3 | **0.0** | → ~0 ✓ |
| crowdR luma | 59.4 | 35.0 | into 20s-30s ✓ |
| crowdL luma | 50.5 | 36.5 | into 20s-30s ✓ |
| **band luma** | 24.6 | **24.9** | UNCHANGED ✓ |
| band white% | 1.0 | 0.5 | unchanged ✓ |

- **Band not dimmed:** band-closeup harness PASS both ON/OFF (10/10 pinned, 0 drops);
  the guitarist renders identically lit/colored in matched shots; venue posters/text
  full-bright. The `skeleton_unshared.milo` exclusion + the (text/ui)-name exclusions
  protect band + HUD text.
- **game.cam highway unaffected:** crowd-dim is `world.cam`-gated; highway metrics
  with crowd-dim ON vs OFF are within frame noise (stroke-bg 74.8 vs 75.9).
- **Other venues not over-darkened:** default-club crowd ON luma ~16 white% 0, band
  present; the change is uniform + crowd-scoped.
- **Visual:** crowd is now a dim pink/red mass (correctly tinted by the venue stage
  light) instead of stark blown-out white — retail-faithful (band is the brightest
  element). Shots: `shots/crowd/BEFORE_bigclub_crowd_white.png`,
  `shots/crowd/AFTER_bigclub_crowd_dim.png`.

---

## GAP A1 — highway watermark too bright → dim (commit `b8f3cfa`)

### The fix

In the `surface.mat` branch of the `game.cam` track-light block
(`Rnd_Wgpu_RB3.cpp` ~5629, after the existing `mu.color *= 0.12`):
`mu.emissiveMultiplier *= 0.30` (static-cached `RB3_HIGHWAY_WATERMARK_OFF` →
0.0, `RB3_HIGHWAY_WATERMARK_DIM` → tune). The shipped `×0.12` darkens only the base
and never the emissive add (`standard_wgsl.inc:868`), so the watermark
over-brightens; this dims the emissive toward retail's faint ghost. **Pattern not
removed** (retail has it).

### A/B numbers (isolated surface patch x[0.40,0.60] y[0.64,0.80] — surface+watermark
only, no gems/lanes/venue; 5-frame avg)

| config | surface luma | watermark Δluma | surface teal (g+b-2r) |
|---|---|---|---|
| watermark OFF (proof) | 73.5 | — (floor) | 59.9 |
| BASE (emisMul ×1.0) | 87.7 | **+14.2** | 60.8 |
| ON (default ×0.30) | 80.5 | **+7.0** | 58.6 |

The watermark luma contribution is **halved** by the ×0.30 dim (matching the plan's
K≈24/82≈0.30 target), and the filigree stays clearly visible.
`RB3_HIGHWAY_WATERMARK_OFF=1` removes the pattern entirely (the source proof).

### A2 (shader desaturate) was NOT applied — measure-gated out

The plan said: add the shared-shader `materialEmissiveDesat` ONLY if A1 leaves the
**watermark** teal > +25. Measured the watermark's OWN teal contribution =
`BASE teal − watermark-OFF teal = 60.8 − 59.9 = +0.9` — **negligible**. The residual
highway teal (~58-60 in the patch) is the rails/surface BASE (the cool blue rail
tint + venue reflection), which was adversarially tuned (MEMORY a234) and is OUT OF
SCOPE — NOT the watermark. So the A2 gate is not met. **Keeping A1-only keeps the
entire change inside the RB3-only TU** (no shared `standard_wgsl.inc` edit → the
cleanest DC3-safe outcome).

### Blast radius

Keyed on `game.cam && surface.mat` only → gems (`prism_mat`), now-bar
(`gem_smasher_glow.mat`), lanes (`rails.mat`), SP `peakstate`, HUD/menu, and the
whole `world.cam` venue/band/crowd are untouched (each is a separate material
branch). Shots: `shots/highway/BEFORE_watermark_bright.png`,
`shots/highway/AFTER_watermark_dim.png`, `shots/highway/AB_watermark_off_proof.png`.

---

## Verify harness

`docs/native/converge-2026-06-20/followup-gaps/_converge_verify.py` (worktree-rooted;
boots the worktree binary; `--mode crowd|highway`, `--override <venue>`, `--shot`,
`--anchor-ms`, `--nframes`; env A/B via `RB3_CROWD_DIM[_OFF]` /
`RB3_HIGHWAY_WATERMARK_*`). Crowd metric measures the foreground crowd strips +
band control; highway metric adds the isolated surface patch. Engine log is binary
(NUL) — read with `decode("utf-8","replace")`.

## Toggles (runtime A/B, no rebuild)

- `RB3_CROWD_DIM_OFF=1` / `RB3_CROWD_DIM=<f>` (default 0.10)
- `RB3_HIGHWAY_WATERMARK_OFF=1` / `RB3_HIGHWAY_WATERMARK_DIM=<f>` (default 0.30)
