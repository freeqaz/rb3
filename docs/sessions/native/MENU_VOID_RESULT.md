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
