# W25-CROWD — STATUS

## Headline

**Trigger-gap branch: NEITHER "no default clip" flow gap NOR whole-vignette hand-off.
It is a native async load-merge defect.** The sv3_a hub crowd walk clips ARE triggered
and DO play + skin correctly for ~1.2s, then a load-merge at ~beat 2.4 **destroys the
playing clip AND swaps the driver's clip bank**, and nothing re-establishes the loop.

**Fixed or handed-off: PARTIAL (narrowed-but-unfixed).** A flag-gated, `clipType=='crowd'`-
scoped, crash-safe re-arm landed (default-OFF), but it does NOT recover the drivers whose
bank was swapped away — census still `animating=0`. The complete fix is engine-side (out
of this lane's A7/A5 scope) and is handed off with a precise charter below.

**Material (A6): moot/deferred** — never reached `animating>0`, so the near-black-material
discriminator (max-pixel vs 17/255) doesn't apply yet. Isolate capture flag-ON still
reads max luma 5/255 (undriven scramble, unchanged from recon).

## Discriminator verdict (checkpointed before fix)

STEP-0 probes (extended `CHARDRV_PROBE` + Enter/Play/Starve/Replace/Die/Clear/Life, all
`#ifdef HX_NATIVE` + env-gated, byte-inert by default) established the exact mechanism:

1. Load-time `CharDriver::Enter` on 8 crowd drivers: **`defClip=(nil)`** → no auto-play.
   (`mClips.Load`/`mDefaultClip.Load` did NOT silently null a bank — the bank resolves
   fine, `nclips=8/11`. The A4 "prime suspect" is exonerated.)
2. The vignette `vignette_start.trig` sends `play_clip` ONCE at beat 0 → 7/8 drivers
   `Play crowdN.clp` with flags `0x222` = `kPlayNoBlend|kPlayLoop(0x20)|kPlayRealTime(0x200)`
   (a self-looping real-time walk). `crowd_female04` never receives it (starts `mFirst=nil`).
   The clip plays and reaches `ScaleAdd(*mBones)` skinning (`CHARDRV_APPLY weight=1.000`).
3. At **beat 2.433 (pollFrame 72)** an async load-merge **destroys** the playing
   `crowdN.clp` object → `Hmx::Object::~Object` → `Replace(this, NULL)` → `CharDriver::Replace`
   → `mFirst = mFirst->DeleteClip(from)` → `Exit(false)` → **`mFirst` NULL**.
   Proof: `[CHARDRV_REPLACE] from='crowdN.clp' to='?'(null) beat=2.433` on all 7, plus
   `[CHARDRV_DIE] pollFrame=72 beat=2.433` on all 7. (NOT PreEvaluate-pop: `[CHARDRV_POP]`
   never fires. NOT the 2nd Enter's Clear: `mFirstAtEntry=(nil)` — already gone.)
4. The SAME merge **swaps this driver's `mClips` ObjPtr** to a wrong player-only sub-bank
   with **zero `crowd*` clips**. The never-triggered `crowd_female04` keeps its full bank
   (`nCrowd=5, nExtra=6`). Post-merge, the destroyed clip name is unreachable from the
   driver's `mClips`, its `Dir()` tree, OR `sMainDir` (`[CHARDRV_KEEPDIAG]` all-nil).
5. No re-trigger exists: `defClip=(nil)`, `mStarvedHandler` Null, and `kPlayLoop(0x20)` is
   NOT one of the starved-replay branches (only `0x30`/`0x40`). `Starved()` returns true
   forever, `mFirst` NULL forever → census `animating=0` → undriven skin scrambles.

NOT the A5 hand-off condition ("whole vignette Enter/flow machinery absent"): `Enter`
fires (twice), `play_clip` fires, the clip plays + skins. The machinery is PRESENT — the
merge corrupts it. This is a load-lifecycle defect, not a missing subsystem.

## The landed change (flag RB3_CROWD_CLIP_KEEP, default-OFF)

`src/system/char/CharDriver.cpp`, all `#ifdef HX_NATIVE`:
- At first `Play` of a `crowd*`-named clip on a driver: snapshot the clip NAME (name-only —
  survives the clip's later destruction).
- In `Poll`, scoped to `mClipType == Symbol("crowd")`: when the driver is `Starved()` with
  `mFirst==NULL`, re-resolve the snapshotted name against the driver's OWN current (live
  ObjPtr) `mClips` and re-`Play` it `kPlayLoop|kPlayRealTime`. Re-fires only while `mFirst`
  is NULL (self-limits to one re-arm per starvation gap).

This recovers any crowd driver whose bank SURVIVED the merge intact. It does NOT recover
the 7 drivers whose `mClips` was swapped to a crowd-less bank (defect (b)).

### Why not fully fixed (honest)

The only way to reach a surviving crowd bank for the swapped drivers is another bank
(e.g. `crowd_female04`'s). Every cross-driver / cached-raw-pointer path to that bank
**use-after-frees against the active merge frame** — repeatedly reproduced as a `proc
SIGSEGV (exit 139)` at beat 2.4 while iterating/deref'ing a bank mid-destruction. Only the
own-`mClips` re-arm is crash-safe, and that bank no longer has the clips. Preventing the
clip destruction (skip `DeleteClip`) is also unsafe — the clip object is genuinely freed,
so `mFirst` would dangle (UAF next Poll). The clips are transient and no resident copy
survives reachably.

## Hand-off charter (engine-side; coordinator to route to a W26 engine lane)

Fix the sv3_a `streetslomo` load-merge so that, for the crowd proxy drivers:
- the playing `crowdN.clp` is NOT destroyed out from under the driver (or is re-established
  post-merge), AND
- the driver's `mClips` ObjPtr is NOT swapped to the player-only sub-bank (keep its own
  `crowd*` bank).
Likely site: the FileMerger / DirLoader merge path that reloads `streetslomo`/the crowd
anim set (`char/crowd/anim/gen/*_base.milo`), and the `Replace`/ObjPtr rewiring in
`src/system/obj/Dir.cpp` / `Object.cpp:131`. This is shared-engine surgery — explicitly
out of the W25-CROWD lane's A7 (no un-scoped shared change) / A5 (no broad bring-up) scope.

## Gates

| Gate | Result |
|---|---|
| flag-OFF `drawlog-golden --fixed-clock --canonical-order` | **PASS 792** byte-identical (307 known-residual within bound) |
| `batch_objdiff` `char/CharDriver` == baseline | **PASS** — `Poll` 93.54% == report.json baseline 93.54499%; Enter/Clear/Replace 100%; Play 100% fuzzy. All changes `#ifdef HX_NATIVE` → Wii object byte-identical |
| `rb3-tests` | **PASS 116 / 0 fail** (7 skipped) |
| flag-ON crash-free | **PASS** (no SIGSEGV; cross-bank attempts SIGSEGV'd → reverted to own-bank re-arm) |
| WorldCrowd A/B (flag-ON vs baseline) | **PASS (safe)** — gameplay run produced ZERO `clipType=='crowd'` CharDriver events → fix provably DORMANT in gameplay; WorldCrowd renders via `RndMultiMesh` with no CharDriver (protected oracle unreachable) |
| recon acceptance: `animating>0` + 8 lit isolate figures | **NOT MET** (partial fix; see "why not fully fixed") |

## Files

- `src/system/char/CharDriver.cpp` — STEP-0 probes (HX_NATIVE, env-gated, inert) + the
  flag-gated partial re-arm (default-OFF, `clipType=='crowd'`-scoped, byte-identical `#else`).
- `scripts/native/_w25_crowd_trace.py` — STEP-0 trace harness.
- `docs/.../W25-CROWD/{PLAN.md, STATUS.md, evidence/*}`.

---
## ERRATA (Wave-25 close-out review `0809e6ef`, E-C1..E-C3)
- E-C2 (SHARPEN): "recovers any crowd driver whose bank survived the merge intact" — in the
  observed sv3_a repro that set is {crowd_female04}, which NEVER received `play_clip` and so has
  NO snapshot → the re-arm provably recovers ZERO of the 8 drivers AS-OBSERVED. RB3_CROWD_CLIP_KEEP's
  current value is PROPHYLACTIC SCAFFOLDING for the W26 engine fix (kept default-OFF through W26
  with a removal criterion: delete if W26 lands the engine load-merge fix and the flag adds nothing).
- E-C1: comments corrected in-code (snapshots the clip NAME only, not the bank).
- E-C3: `gCrowdKeep()` is never pruned on driver destruction (stale-key alias risk flag-ON) —
  noted in-code, opportunistic W26 cleanup (harmless while default-OFF).
