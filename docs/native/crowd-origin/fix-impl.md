# FIX IMPL — band lead-guitar *_strings skin explosion (SHARD_GUARD instrument drop)

Date: 2026-06-20. Agent: IMPLEMENT (Opus). Worktree
`/home/free/code/milohax/rb3/.claude/worktrees/inststrings` (branch
`wt-inststrings`, from master `5ebcf887`). Engine NOT modified (no pin bump).

Implements `deform-investigation.md` STEP 3 (REAL-EXPLOSION fork). The band lead
guitar's `*_strings.mesh` (chainsaw / guitar_brain family) skinned to a ~136u world
AABB (ratio ~5.0) and the engine V24 `[SHARD_GUARD]` correctly DROPPED it → the
visible left-edge smeared mass. Fixed match-neutral (Wii byte-identical) by
rebinding those strings meshes so the world skin re-composes back to ~bind (ratio
~1.0), so the guard stops firing on its own.

## Files changed (this worktree, this fix only)

- `src/system/bandobj/BandCharacter.cpp` — new HX_NATIVE method
  `RebindInstStringsToRestBasis()`, called from `Poll()` AFTER `mInstDir->Poll()`;
  constructor init + SyncObjects/StartLoad latch re-arm (all inside `#ifdef HX_NATIVE`).
- `src/system/bandobj/BandCharacter.h` — method decl + 2 native members
  (`mNativeInstReboundOnce`/`mNativeInstReboundQuiet`), all inside `#ifdef HX_NATIVE`,
  appended after the matched layout.
- `docs/native/crowd-origin/fix-impl.md` (this) + `shots/*.png`.

The Wii `#else`/strip path is byte-identical (see "Wii byte-identity" below).

## bindingFinding — the skeleton_unshared binding is BY DESIGN (basis divergence)

Settled by an INST_STRINGS_PROBE (temporary, reverted) dumping every
`chainsaw_strings.mesh` bone's resolved Dir/mStoredFile, world pos, per-Poll motion,
and whether `mInstDir->Find(boneName)` resolves a distinct instrument-resource bone:

- **All 10 chainsaw_strings bones resolve to `char/char/main/skeleton_unshared.milo`**
  (the CHARACTER skeleton). Bone names: `bone_nut`, `bone_bridge`,
  `bone_bend_string01..06`, `bone_vibrate_hi`, `bone_vibrate_low` — the guitar's
  string-bend rig.
- **`mInstDir->Find` returns NIL for every one** (`distinct=0 instFile='(none)'`): the
  instrument's own `chainsaw_resource.milo` has NO neck/string bones. There is NOTHING
  to repoint them to — so mechanism (1) "repoint to the instrument resource" is OUT.
- **The bones ANIMATE during play**: across ~30-Poll windows `bone_nut` swings
  3.1→5.1→5.4u while `bone_bridge` stays 1.2→0.7→0.6u — the guitarist flexes the neck
  (the bend feature). Each bone is individually sane (~near player0's staged guitar).

Conclusion: this is a **merge-time-by-design** binding (the string-bend rig genuinely
lives on the character skeleton), and the explosion is the **char-skinning-deform
basis divergence** family on `mInstDir` — the native per-member skeleton's basis/
spacing for these neck bones diverges from the authored inverse-bind. NOT a name
mis-resolution.

This contrasts with the FINE instruments (standard guitars/basses: dinky01, js,
magnum1, precision01, telebass, c20bass): their `*_strings` bones bind to their OWN
rigid `<inst>_resource.milo` neck and are classified `other` (NOT skeleton_unshared),
ratio 1.0. The "brain"-class special guitars (axe/batwing/brain/chainsaw/... — the
ones with ONLY a `_resource.milo` and no swap variants) are the exploding set.

## mechanism — rigid-anchor (rebake-at-rest variant kept for A/B)

Because the neck bones animate (so a one-time static rebake risks re-growing under
the bend) AND the FINE instruments already render rigid strings (ratio 1.0), the
default is the plan's **rigid-anchor** (mechanism 2):

> Repoint EVERY strings bone to ONE rigid anchor — `bone_bridge` (the body-end bone,
> least-moving, rides the instrument body rigidly) — and rebake each bone's offset =
> `meshWorld * inv(anchorWorld)` at the current pose. The whole mesh then rides that
> single rigid bone: world AABB == bind AABB through the entire bend (ratio ~1.0),
> matching the FINE instruments. This drops the in-mesh string-bend wobble, which is
> the correct visual target state (the FINE strings are rigid too).

`RB3_INST_STRINGS_MODE=rebake` instead rest-rebakes each bone IN PLACE (preserves the
bend). MEASURED both: rebake held ratio 1.06-1.08 (0 drops), rigid held 1.00-1.03 (0
drops). Both fix the explosion; rigid is the default (bulletproof single-bone ride +
matches the FINE rigid look + the plan's "if bones animate → rigid-anchor" guidance).

## Gating (mandatory)

- Entirely inside `#ifdef HX_NATIVE`; Wii path byte-identical.
- Scope: `mInstDir`'s `*_strings.mesh` ONLY (own draw-tree walk; never this/mOutfitDir
  — the rest of the instrument, `_resource`/`_teeth`, is ratio 1.0 and untouched), AND
  gated on at least one bone resolving to `skeleton_unshared.milo`. FINE own-resource
  instruments never match → never touched.
- Sets `mesh->mNativeBonesRebound = true` so the engine rebake/clamp skip the mesh.
- Idempotent latch (`mNativeInstReboundOnce`), re-armed by SyncObjects + StartLoad.
- DEFAULT-ON, opt-out `RB3_NO_INST_REBIND=1` (mirrors `RB3_NO_SKEL_REBIND`).
- Engine NOT modified (the guard stays as the backstop; it simply stops firing).

## Repro (the blowup is character/guitar-dependent, NOT song-dependent)

In THIS asset set the default headless prefab equips `jp80` (guitar) + a FINE bass,
so the DEFAULT nav does NOT load an exploding instrument. The explosion is tied to
the EQUIPPED guitar MODEL: the "brain"-class specials (`chainsaw`, `brain`, etc.,
which have only `char/main/guitar/gen/<name>_resource.milo` and no swap variant) bind
their strings to the character skeleton. To force the repro deterministically a
TEMPORARY native test hook was added to `BandWardrobe::LoadMainCharacters` (read the
guitarist's `piece->mName` and override to `getenv("RB3_FORCE_GUITAR")`), **reverted
before commit** (BandWardrobe.cpp diff is empty). To re-verify, re-apply that hook
and boot with `RB3_FORCE_GUITAR=chainsaw_resource`:

```
piece := bchar->mInstruments.GetPiece(inst);   // BandWardrobe.cpp ~ line 659
#ifdef HX_NATIVE
  const char *fg = getenv("RB3_FORCE_GUITAR");
  if (fg && fg[0] && BandCharDesc::GetInstrumentFromSym(inst)==BandCharDesc::kGuitar)
    piece->mName = Symbol(fg);
#endif
```

Harness: `/tmp/inststrings_probe.py LABEL chainsaw_resource [ENV=VAL ...]` (boots to
gameplay with `SHARD_RATIO_DBG=1 SHARD_DBG=1 RB3_FORCE_GUITAR=chainsaw_resource`,
parses strings ratios + instrument shard drops). Screenshots:
`/tmp/inststrings_shots.py`.

## Before / after numbers (chainsaw_strings, forced lead guitar)

| metric | BEFORE (fix off / opt-out) | AFTER (fix on, rigid default) |
|---|---|---|
| `chainsaw_strings` bind / world | 27.63 / ~136 (125-148) | 27.63 / ~28 (27.6-28.4) |
| `chainsaw_strings` world/bind RATIO | **~4.9-5.0** | **~1.00-1.03** |
| `[SHARD_GUARD]` dir=instrument drops/run | **~1984** (opt-out re-run: 1926) | **0** |
| `chainsaw_strings` GUARD drops | 1984 | **0** |

Non-regression — the already-FINE instruments STAY ratio ~1.0 (the gate skips them):
- `dinky01_strings` (forced FINE guitar): bind 33.87 / world 33.87, ratio **1.00**,
  classified `other`, instrument_drops=0 — never matched the skeleton_unshared gate.
- `magnum1_strings` (bass on the chainsaw run): ratio **1.00**, untouched.
- `chainsaw_resource.mesh` / `chainsaw_teeth.mesh` (same instrument, other sub-meshes):
  ratio ~1.0 before AND after — untouched.

Opt-out proof: `RB3_NO_INST_REBIND=1` reverts to the status quo (1926 instrument
drops, chainsaw_strings re-explodes) — the gate works.

## Screenshots (docs/native/crowd-origin/shots/)

All captured with the forced chainsaw lead guitar at a matched song time (~songMs
6.4-6.6s):

- `fixoff_guardoff.png` — fix OFF + guard OFF: the BUG — a dark smeared mass intrudes
  from the LEFT across the highway, covering the guitarist.
- `fixed_guard_off.png` — fix ON + guard OFF: smear GONE; guitarist + intact strings
  render cleanly at the staged spot. **The decisive A/B.**
- `fixed_on.png` — fix ON + guard ON (the shipping default): clean highway, guitarist
  visible, no drop-gap, no smear.
- `fixoff_guardon.png` — fix OFF + guard ON (opt-out): guard drops the exploded
  strings (the status-quo masking — guitarist visible, strings missing).
- `before.png` — explosion baseline (guard-on, exploding lead loaded).
- `after_rigid.png` — gameplay capture during the rigid-mode (default) measure.
- `fine_dinky.png` — non-regression visual: a FINE own-resource guitar (dinky01)
  renders unchanged (its strings never matched the gate).

## Wii byte-identity

Rebuilt the Wii `BandCharacter.o` with `tools/ninja-locked` for BOTH the pristine
master source (base `5ebcf887`) and my changed source, then byte-compared:

- The two `.o` files are the SAME SIZE (249584 B) and differ in **exactly 6 bytes**,
  all in `.strtab` (verified via section-offset mapping). They are ASCII digits in the
  `FORCEACTIVEBandCharacter<__LINE__>` stub symbol NAMES (e.g. `...218`→`...220`),
  shifted because my native code (inside `#ifdef HX_NATIVE`) moved the physical source
  lines of three `DECOMP_FORCEACTIVE` macros. **ZERO bytes differ in any code/text
  section.**
- `FORCEACTIVE*` symbols are decomp scaffolding stubs **NOT present in the target
  binary** (`grep FORCEACTIVEBandCharacter orig/.../band_r_wii.map` = 0), so renaming
  them cannot affect matching.
- objdiff match% is **UNCHANGED**: unit `main/system/bandobj/BandCharacter` =
  **77.31311%, 275/290 functions** both before and after; `BandCharacter::Poll()`
  matches 98.56771% (matched-function code byte-identical).

So the only artifact of the line shift is 6 strtab bytes in non-matched stub names;
the matched-function bytes and the match% are unchanged. (A pre-existing structural
limitation: inserting a method mid-file unavoidably shifts later `__LINE__` stub
names; it is provably match-neutral here.)

## Build

- Native: `cmake --build native/build-native --target rb3-native -j16` — GREEN.
- Wii: `tools/ninja-locked build/SZBE69_B8/src/system/bandobj/BandCharacter.o` — GREEN.
