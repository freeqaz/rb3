# MENU / HUB "BLACK VOID" — Step 2 implementation result (v41)

**Authored:** 2026-05-29 (Opus implementation subagent, worktree `nwt-menu`).
**Plan:** `MENU_VOID_PLAN.md` Step 2 (smallest-correct, ~M). Step 3 NOT attempted.
**Verdict (TL;DR): HOLD — ship the change (it is safe + correctly gated + does
NOT regress anything, exit 0), but it does NOT fix the user-visible hub void
because the plan's stated root cause does not apply to the actual rooftop-hub
scene. See §4. The change is a correct, no-regression prerequisite, not the
fix.**

---

## 1. What changed (file:line)

Only ONE file changed (the `Object.cpp` `Copy` edit was tried, broke an
unrelated UI copy path, and was REVERTED — see §3):

- `src/system/world/Instance.cpp`
  - `IsDeferredVenueProxy()` (~`:351-371`): narrowed the deferral to
    `world/shared/` ONLY (dropped the `world/vignette/` clause) when the fix is
    ON. `RB3_MENU_VOID_FIX_OFF=1` restores the original both-clause baseline.
  - `WorldInstance::SyncDir()` instancing loop (~`:440-465`): after the
    `NewObject + CopyObject` fresh-copy branch, carry the copy's `mDir` from the
    SOURCE object via the existing `HxSetDir` seam
    (`if (!foundObj->Dir() && it->Dir()) foundObj->HxSetDir(it->Dir());`) so the
    `:0x2CA` assert (`MILO_ASSERT(p->from->Dir(), …)`) passes. This is scoped to
    the WorldInstance instancing loop ONLY (the global `Hmx::Object::Copy`
    approach was rejected — §3). The copy is a throwaway ref-replace stand-in
    that is never entered in any dir hash, so teardown (`DeleteObjects` iterates
    only hash-registered members) never `RemoveFromDir`s it.
  - `MENU_VOID_DBG=1` instrumentation (render-inert): logs proxy name / file /
    deferred-flag / source obj-count per `SyncDir`, and any null-`Dir()` object
    reaching the `:0x2CA` assert.

All edits are additive `#ifdef HX_NATIVE` and gated by `RB3_MENU_VOID_FIX_OFF`.

## 2. Env gate

- `RB3_MENU_VOID_FIX_OFF=1` → original both-clause deferral (vignette + shared
  both bail to empty proxy) AND the instancing loop's `HxSetDir` never matters
  because the loop is not reached for vignette roots. = baseline.
- unset → narrowed deferral (`world/shared/` only) + `mDir`-carry on copies.
- `MENU_VOID_DBG=1` → diagnostic logging (independent of the fix gate).

## 3. Why the `Hmx::Object::Copy` mDir-carry was REVERTED (key finding)

The plan's preferred "clean version" — carry `mDir` inside `Hmx::Object::Copy`
mirroring the copy-ctor (`operator=`, `Object.cpp:103`) — was implemented first
(`if (o->Dir() && !Dir()) HxSetDir(o->Dir());`, gated). It **crashed the boot**
(exit 134, SIGABRT) with:

```
No entry for Object (ui/resource/list/list_overshell_menu.milo)
  in overshell_users (ui/resource/list/list_overshell_menu.milo)
```

`Copy()`/`CopyObject()` is on EVERY clone path (the plan's HIGH-blast-radius
warning, §4 of the plan, confirmed empirically). Carrying `mDir` universally
gave an **unrelated UI list-panel copy** a non-null `Dir()` with no hash entry →
`~Object` → `RemoveFromDir` MILO_FAIL → `free(): invalid pointer`. This is
exactly the teardown-symmetry trap the plan flagged as the most likely break.

Fix: move the `mDir`-carry to the `Instance.cpp` instancing-loop CALL SITE only,
so the blast radius is the WorldInstance proxy path alone. With that scoping the
full pipeline exits 0 (§5).

## 4. A/B visual result — the change does NOT fix the hub void (honest)

Screens in `screenshots/v41-menu-void/` (`*_on_*` = fix ON, `*_off_*` = OFF).

- **f0007 (rooftop city hub):** fix ON and OFF are VISUALLY IDENTICAL — same
  lower-band rooftop + top/right black void as the baseline
  (`v34-status-review/01_f0007.png`). NO improvement.
- **f0120 / f0200 (BABOON NEST menu):** the backdrop already renders well in
  BOTH ON and OFF (and in the baseline) — neon signs, baboon, "BABOON NEST"
  text, the PLAY NOW/QUICKPLAY menu. This menu backdrop was never the void.

**Root-cause finding (contradicts plan §1.2 for this scene):** with
`MENU_VOID_DBG=1`, across the entire boot→hub→menu run the ONLY `WorldInstance`
proxy that ever reaches `SyncDir` is `world/shared/amps/classic_blacktriple`
(an amp prop, correctly kept deferred). **NO `world/vignette/` proxy ever hits
`SyncDir`** — `grep "deferring.*vignette"` is empty in both ON and OFF logs. The
vignette milos (`sv3/sv4/sv8` cityscape/streetslomo) DO load (their `.clp`
PostLoad NOTIFY lines fire) and the rooftop scene DOES render its lower band —
but it is NOT gated by `IsDeferredVenueProxy` at all. So:

- Narrowing the deferral is a **no-op for the visible hub void** — there is no
  deferred vignette proxy to un-defer on this path.
- The `mDir`-carry never fires for the hub (the instancing loop's null-`Dir()`
  branch logged ZERO hits via `MENU_VOID_DBG`).
- **The rooftop-hub void is a DIFFERENT rendering problem** (the sv8 cityscape
  loads + partially renders through a non-proxy path; the top/right black is
  likely a camera-frustum / partial-geometry / clip issue), OUTSIDE Step 2's
  scope and not the proxy-instancing deferral the plan targeted.

## 5. Full regression — CLEAN (hard gate passed)

Gameplay reproducer (`MILO_MAX_FRAMES=24000`, `track:guitar`, `nofail`):

- **EXIT 0**, clean shutdown ("APP EXITED, EXIT CODE 0"), no asserts / no
  `No entry` / no SIGABRT / SIGSEGV.
- Song-load reaches `Game::mLoadState = kReady` (no song-load regression).
- Gameplay venue + highway + HUD intact: ~253 meshes / 128787 tris/frame;
  f1100 shows the guitar highway with colored gems streaming to the smasher +
  HUD (`screenshots/v41-menu-void/01_f1100_on_gameplay_f1100.png`).
- During gameplay the deferred proxies are exclusively `world/shared/` props
  (amps/mics/decals) — the in-song venue (V19) path is untouched; no
  `world/vignette/` proxy is instanced there either.
- Boot path (240-frame) also exits 0 with no fails.

## 6. Readiness verdict

- **Safety: SHIP-SAFE.** Additive, gated, scoped to the WorldInstance instancing
  loop; full pipeline exits 0; no venue/song-load/gameplay regression; A/B both
  ways green.
- **Efficacy for N3: DOES NOT fix the user-visible hub void.** The change is a
  correct, no-regression prerequisite (the assert-root-cause `mDir`-carry is now
  in place for any future vignette-proxy path), but the captured void is not
  produced by the vignette-proxy deferral on the real boot path.
- **Recommendation: HOLD on claiming N3 fixed.** Merge is optional (it is safe
  and de-risks the instancing loop), but it should NOT be presented as the
  menu-void fix. The actual hub void needs a fresh root-cause: the sv8 vignette
  renders partially through a NON-proxy path, so the next probe should trace how
  the rooftop cityscape is attached/drawn (camera frustum / world chunk / clip),
  NOT the `SyncDir` deferral. The plan's §1.2 hypothesis is not borne out for the
  observed frame.

---

## Re-investigation (v43, 2026-05-29) — ROOT-CAUSED + FIXED

**Verdict: FIXED.** The hub void is a single opaque-black backdrop mesh
(`worldcenter.mesh`) depth-occluding the real sky. Skipping it (gated,
default ON) fills the upper region with the cloudy-night sky + RB3/BAND3 logo.
A/B verified, full 24000-frame gameplay regression exits 0 with no
venue/highway/HUD/song-load regression. Subjective visual sign-off is Opus-gated
per `visual-reviews-opus-only`.

### Root cause (file:line evidence)

The captured main-hub frame uses the **rooftop-city shell vignette**
`world/vignette/shell/gen/sv8_a.milo_xbox` (the `{random_elem}` pick at
`ui/vignettes.dta:89`; `worldcenter` exists in NO other milo — verified by a
full `orig-assets/extracted` scan). That milo contains `worldcenter.mesh`:

- material `worldcenter.mat`, **color=(0,0,0,1)**, **NO diffuse texture**,
  blend=`kBlendSrc`, **zmode=`kZModeNormal` (writes depth)**, 24 compressed
  verts = a skybox-sized box at the world origin (pos=(0,0,0)).

It draws opaque-black and **depth-occludes the sky backdrop layers behind it**:
`difference_clouds.mesh` (real 512² tex, zmode=kZModeTransparent), `skynight.mesh`
(sky_gradient tex), the BAND3/RB3 `logo`, and the `sky_dome*` cloud domes. Retail
layers the painted cloud render-target (`clouds_rnd.tex`, type `kRenderedNoZ`
= `IsRenderTarget`) over the scene — but on native that RT is **never painted**
(`RndTex::MakeDrawTarget`/`FinishDrawTarget` are no-op stubs in the engine
`Rnd_Wgpu_RB3.cpp`), so its bitmap is **pixels=NULL, 0×0** (confirmed by
instrumentation) → `useTexture=0` in `BandRnd::DrawMesh`
(`Rnd_Wgpu_RB3.cpp:1516-1523`) → `sky_dome` contributes nothing. With the
sky-dome layer gone, the opaque-black `worldcenter` box is all that fills the
upper ~45% of the frame = the void.

### How it was proven (rejecting the prior hypotheses cleanly)

1. `RB3_RENDER_DBG` enumeration: the hub world **does** issue draw calls for
   `sky_dome`, `skynight`, `moon`, `difference_clouds`, cityscape, crowd, etc.
   — so "proxy never instanced / never drawn" is wrong (prior result already
   ruled out the proxy-deferral path; this confirms the geometry is present).
2. Frustum culling is already fully disabled on native
   (`rndobj/Draw.cpp:17-21,36-46`), so culling is NOT the cause.
3. **The clear-color was a trap.** `MILO_CLEAR_COLOR` is read only in the generic
   `Rnd_Wgpu.cpp:266`, NOT in `BandRnd` (`Rnd_Wgpu_RB3.cpp`), and `App.cpp:173`
   (HX_NATIVE) hard-set `gBandRnd` clear to black. Adding a real
   `RB3_CLEAR_COLOR` lever (App.cpp, authoritative) and clearing to **green**
   showed: the lower-right region went green (genuinely empty/clear) while the
   **upper region stayed pure black under a green clear ⇒ opaque geometry**.
4. A per-drawable `MENU_VOID_SKIP=<substr>` A/B (matched-fork `Draw.cpp` hook):
   skipping just `worldcenter.mesh` turned the upper black to the green clear
   (and, under the normal black clear, revealed the cloud sky + RB3 logo). No
   other mesh (sky_dome / skyscrapers / bgbuildings / difference_clouds) produced
   that effect. Single occluder isolated.

### The fix (gated, default ON; `RB3_MENU_VOID_FIX_OFF=1` = baseline)

`src/system/rndobj/Draw.cpp` (matched-fork, additive `#ifdef HX_NATIVE`): a
per-drawable native hook in `RndDrawable::Draw()` + `RndDrawable::DrawBudget()`
that skips the `worldcenter` backdrop **only** when its material signature is
opaque-black + untextured + depth-writing (`MenuVoidIsWorldcenterOccluder`).
Tightly scoped — it removes exactly **2 draws / 24 tris** at the hub frame
(826→824 meshes), nothing else. The same TU also carries the (render-inert)
`MENU_VOID_DBG2` material dump and `MENU_VOID_SKIP` A/B levers used above.

Supporting glue/matched-fork (default black = baseline, diagnostic only):
- `src/App.cpp` — authoritative `RB3_CLEAR_COLOR=r,g,b` clear-color lever (the
  experiment that distinguished opaque-black geometry from empty clear).
- `native/src/main_native.cpp` — same env lever at the earlier `gBandRnd`
  clear-color set (superseded by App.cpp; harmless, defaults black).

NO shared-engine edit was required or made.

### A/B + regression (all green)

- Hub repro (`@10:start,@30:confirm`, 400 frames),
  `screenshots/v43-menu-void2/{on,off}/`:
  - **f0007 ON**: cloudy-night sky + RB3/BAND3 neon logo fill the upper region.
    **OFF**: the original upper black void. Decisive improvement.
  - **f0120/f0200 (BABOON NEST menu)**: identical ON vs OFF (that menu uses a
    different vignette w/o `worldcenter`) — fix correctly scoped.
- Full gameplay reproducer (`track:guitar`, `nofail`, `MILO_MAX_FRAMES=24000`),
  `screenshots/v43-menu-void2/gameplay/`: **EXIT 0**, clean shutdown, no
  asserts / `No entry` / SIGABRT / SIGSEGV. f1100 + f3000 show the guitar
  highway + colored gems + venue + HUD intact (no venue/highway/song-load
  regression). Mesh tally identical to baseline except the 2 worldcenter draws.

### Residual (not in scope, lower-impact)

The `clouds_rnd.tex` **render-to-texture is still unpainted** on native
(`MakeDrawTarget`/`FinishDrawTarget` are engine no-op stubs), so the primary
*animated* cloud-dome layer is absent; the sky shown is the static
`difference_clouds` + `skynight` gradient + logo layers. Implementing RT support
is an engine change (flagged, NOT done) and would further raise fidelity, but the
user-visible void is resolved by un-occluding the textured layers.
