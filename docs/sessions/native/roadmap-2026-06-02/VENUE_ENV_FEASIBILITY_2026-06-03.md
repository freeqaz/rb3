# Venue-Environ Bring-Up — Fresh Feasibility Re-Assessment (2026-06-03)

**Verdict: GO — CHEAP.** The blocker is a **genuine one-line decomp bug** in
`WorldInstance::SyncDir` (transposed `ObjPair` constructor arguments). Fixing it
lets the native cosmetic-venue instancing loop run to completion with **no
assert, no crash**, and the venue backdrop geometry renders. The fix is
**asm-match-neutral** (96.63135% before and after — byte-identical diff).

The elaborate prior conclusion ("deep multi-session-hard, needs many-to-one
parent-chain or per-proxy shadow dirs; obstructions (a)/(b)/(c)") was chasing a
`PostLoad`-based workaround for a symptom (`p->from->Dir()` null) that only
existed *because of the transposed args*. With the args corrected, the symptom
never arises and (a)/(b)/(c) are moot.

---

## 1. TOP LEAD — RESOLVED: genuine RB3 decomp bug (args transposed)

### The divergence
| | RB3 (`src/system/world/Instance.cpp:434`) | DC3 (`Instance.cpp:340`) | Target binary (m2c) |
|---|---|---|---|
| seed pair | `ObjPair(mDir, this)` | `ObjPair(mDir, this)` | `from=mDir, to=this` |
| **loop pair** | `ObjPair(foundObj, it)` ❌ | `ObjPair(it, foundObj)` ✅ | **`from=it, to=foundObj`** ✅ |

`ObjPair(o1, o2)` → `{from = o1, to = o2}` in **both** repos (RB3
`src/system/obj/Object.h:468-473`; DC3 `src/system/world/Instance.h:11-15`).
Identical semantics. So the first ctor arg is always `from`.

- `it` = the iterator's current object, living in the **shared** `mDir`
  → **HAS a Dir** (`it->Dir() == mDir`).
- `foundObj` = a freshly `Hmx::Object::NewObject(...)`'d copy → **null Dir**
  (`Hmx::Object::Copy` does not copy `mDir`).

RB3's seed uses `from = mDir` (the *source*), but its loop uses `from = foundObj`
(the *destination copy*) — **inconsistent with its own seed**, and backwards.

### Ground-truth confirmation against the target binary (the decisive evidence)
`SyncDir__13WorldInstanceFv` is compiled and diffed against the Bank-8 target at
**96.63%** (`report.json`), i.e. asm-verified. m2c of the target
(`bin/analyze-function -u system/world/Instance SyncDir__13WorldInstanceFv`)
shows the loop's `objPairs.push_back` node store as:

```
temp_r3_9->unk8 = sp7C;        // node +0x8 (= from) = sp7C  == 'it'
temp_r4_3->unk4 = var_r17_2;   // node +0xC (= to)   = var_r17_2 == 'foundObj'
```

(`sp7C` is the `ObjDirItr` current object; `var_r17_2` is the
`FindObject`/`NewObject`+`CopyObject` result.) The seed store earlier is
`->unk8 = var_r16 (mDir)`, `->unk4 = var_r14_2 (this)`. So the **target binary
itself does `from=it, to=foundObj`** — DC3's orientation, the **opposite** of
RB3's current source.

The three downstream uses in the target m2c all confirm `from=source`,
`to=copy`:
1. `loop_119` assert: `if ((s32) var_r22->unk8->unk10 == 0) Fail(... 0x2CA, "p->from->Dir()")`
   → asserts `from->Dir()`. With `from=it` (has Dir) → passes.
2. ref-replace: `(*ref)->Replace(var_r22->unk8 /*from=it*/, var_r22->unkC /*to=foundObj*/)`
   → re-points refs **from the shared source to the fresh copy** (per `ObjRef::Replace`
   docstring: "removes properties from the first object, moves to the second").
3. `loop_130` SetName: `to->SetName(from->Name(), this)` → registers the **copy**
   into the proxy `this` under the source's name.

RB3's downstream source (lines 439-460) already reads `p->from` for
assert/refs/Replace-source and `p->from->Name()` for SetName — i.e. it is
**target-faithful**. Only the push_back arg order is wrong. This is a clean,
isolated, single-point logic bug.

### Why matching never caught it
The swap is **asm-invisible**: both ctor orderings just store two pointer
registers into the node's two payload slots (+0x8, +0xC), and MWCC assigns the
same registers either way. Rebuilding the `.o` with the corrected order yields
**byte-identical** objdiff output (96.63135% → 96.63135%, verified in the probe
worktree). The remaining 3.4% mismatch is the unrelated Sphere-FPR cascade
(`mDir->mSphere` x/y/z/radius kept in callee-saved f27-f30 by the target vs
spilled to stack 0x60-0x6c by our `Sphere sphere = mDir->mSphere;`) — a separate,
permuter-class issue, not touched by this fix.

### Runtime confirmation (prototype, native, end-to-end)
Built `rb3-native` in an isolated worktree with the swap + a runtime
`RB3_NO_VENUE_DEFER=1` toggle that bypasses the cosmetic-venue deferral so the
fixed loop actually runs. Booted headless to `game_screen` (song playing):

- **13** cosmetic venue proxies ran the fixed instancing loop instead of being
  deferred (`classic_blacktriple` amps, `amp_fnr_bassman`, `mic_straight_*`,
  `smoke_machine` ×4, `crowd_shadow.inst`, `tape_x` props).
- **58** `ObjPair` entries reached the `p->from->Dir()` assert; **0** had a null
  Dir (every `from` = `0x55dcf9a319e0...`, a valid dir). The assert that forced
  the deferral now passes universally.
- **No** `MILO_FAIL`, **no** `0x2CA`, **no** "No entry for ...", **no** "Could
  not find ...mesh" (the downstream DeleteTransientObjects crash the prior
  session feared does **not** occur).
- Screenshot at songMs~4000 shows venue backdrop geometry rendering (dartboard
  prop, wall posters, stage scenery around the highway) where the deferred build
  drew nothing.

(Probe artifacts, not committed: rb3 worktree
`.claude/worktrees/venue-env-probe`, engine worktree
`/home/free/tmp/milo-engine-venueenv`, build `/home/free/tmp/build-venueenv`,
log `/tmp/rb3-gp-*.log`.)

---

## 2. Proposed approach (the actual fix)

**Primary fix (the whole thing):** in `src/system/world/Instance.cpp`, change
the loop's

```cpp
objPairs.push_back(ObjPair(foundObj, it));   // WRONG (current)
```
to
```cpp
objPairs.push_back(ObjPair(it, foundObj));   // correct — matches target & DC3
```

Then **remove the `HX_NATIVE` deferral** (`IsDeferredVenueProxy` + the early
`return` in `SyncDir`, lines ~304-374, and the long audit comment). With the
swap in place the loop no longer asserts, so the deferral is no longer needed.

That is the entire fix. Obstructions (a)/(b)/(c) from the prior audit do not
apply: they were obstructions to a `PostLoad`-based parent-chain workaround for
the null-`from->Dir()` symptom. The symptom is caused solely by `from=foundObj`;
with `from=it` there is no null Dir, no need to wire `sharedDir->HxSetDir(this)`,
no many-to-one parent-chain problem, no per-proxy shadow dirs.

**Note on the swap being match-neutral:** because the fix is byte-identical in
asm, it can land in the shared `src/` tree without any decomp-regression risk and
without needing an `HX_NATIVE` guard — the corrected order is what the target
binary actually does. (Recommend keeping it unguarded; it improves source
correctness for the Wii target too, even though the differ can't see it.)

**Sequencing:** land the one-line swap first (match-neutral, safe), rebuild the
Wii `.o` to confirm 96.63% holds, then in a follow-up remove the native deferral
and re-test boot-to-gameplay on native + web.

---

## 3. Effort & risk

| | Estimate |
|---|---|
| **Effort** | **~0.5 person-day.** Swap = 1 line. Deferral removal = delete ~70 lines + comment. Re-test boot-to-gameplay native + once on web. |
| **Confidence** | **High.** Bug confirmed three ways: (1) target-binary m2c shows the opposite orientation; (2) DC3 (works on native) uses the opposite orientation; (3) prototype boots to gameplay with 0/58 null Dirs and renders the venue. |
| **Match-regression risk** | **None.** 96.63135% → 96.63135%, byte-identical. Only callsite of `ObjPair(from,to)` semantics in the repo (`grep` confirms). Seed pair already correct, untouched. |
| **Runtime blast radius** | Affects every proxy that hits the instancing loop — menu/song-select/gameplay venue backdrops AND non-venue `WorldInstance` proxies. **This is a net positive**: today the deferral only skips the `world/vignette/`+`world/shared/` cosmetic subset; *non-venue* proxies already ran this (buggy) loop. The bug was latent for them because their `from=foundObj` copies happened not to be re-referenced in a way that tripped the `RefOwner()->Dir()==null` branch, OR they were always resolved by `FindObject` (so `foundObj` was an existing dir-owned object, not a fresh NewObject). Correcting the order makes the ref-rewiring point the right direction for **all** proxies — strictly more correct. Verified no crash through full boot-to-gameplay. |
| **Residual** | The venue renders but is lit by the **degenerate gameplay RndEnviron** (grey/desaturated) — that is the *separate* lighting issue, now decoupled from this task (see reframing). The geometry/instancing is correct. |

---

## 4. GO / NO-GO (with reframing)

**GO — and do it now.** Even under the reframing (gameplay lighting no longer
depends on the venue; venue-env is "only" the cosmetic backdrop), the cost has
collapsed from "deep multi-session world-subsystem work" to **a one-line
match-neutral bug fix plus deleting a workaround**. At ~0.5 day with high
confidence and zero match-regression risk, the cosmetic backdrop (amps, mics,
crowd, stage props, vignette shells) is clearly worth landing. It also removes a
standing piece of `HX_NATIVE` divergence from the shared `SyncDir`, which is
good hygiene for the port.

Recommended follow-ups after landing:
1. Land the `ObjPair(it, foundObj)` swap to the shared tree (match-neutral).
2. Remove the `IsDeferredVenueProxy` deferral + audit comment.
3. Re-test boot-to-gameplay + main-hub on native; confirm once on web.
4. Track the grey/desaturated venue lighting under the (separate) venue-env
   lighting item, not here.

### Correction to prior docs
`DIVERGENCE_AUDIT.md` / the `Instance.cpp` audit comment (lines ~326-350) and the
A4 memory note ("WorldInstance::SyncDir defers world/vignette/ + world/shared/
proxies — a known-HARD V2 inlined-cached-shared proxy instancing gap") should be
amended: the root cause was **not** an inlined-cached-shared instancing gap or a
many-to-one parent-chain problem. It was a **transposed `ObjPair` ctor** in the
RB3 decomp. The (a)/(b)/(c) obstructions were artifacts of debugging a `PostLoad`
workaround layered on top of the transposed args.
