# Wave 20 — SYNTHESIS: the hands root cause, found

**Coordinator synthesis of Lanes W / N / D. Discharges A11.** AUDIT-ONLY wave — this
document charters no code; it names the root cause and hands Wave 21 a fix charter.
Anchors: Lane W `ee967298`, Lane N `d92cf1a3`, Lane D (evidence commit in W20-DECOMPAUDIT).

## The owner's question, answered

> *"is it a bug in decomp code? where is it? we're patching around but there are answers."*

**It is NOT a bug in decomp code.** Lane D audited every function that decides skeleton
binding — `BandCharacter::Filter` (95.6%), `ObjectDir::PreLoad` (100%, owns the
kInlineCached read), `LoadSubDir`, `PostLoadInlined`, `ReplaceRefs`, `OnInstallFilter`,
the whole FileMerger chain — and found **zero semantic divergences**: every sub-100%
diff is register-allocation / scheduling / SDA-relocation noise. The `sBoneMergeDir`
remap branch the record calls "never-firing" is *structurally present and correct in the
compiled target*. The bug is not in the code being wrong; it is in **what data reaches
that correct code at load, and one native-only shim that stops the correct code from
running.**

## The root cause is a two-layer load-path divergence

### Layer 1 — native binds the WRONG skeleton, and our own white-texture shim causes it (PROVEN)

The three lanes join cleanly:

| | Wii (Lane W, ground truth) | Native shipped (Lane N) |
|---|---|---|
| hand-mesh bone binding | **OWN_MEMBER** — per-member `skeleton_unshared.milo` (60/60 rows, SHARED_ROOT=0, 3 distinct per-member instances) | **SHARED_ROOT** — the one shared `skeleton.milo` magnet (205/205 slots, distinct=1, all 4 members on one instance) |
| `sBoneMergeDir` remap (`Filter:4202`) | fires (retail kMerge iterates merged objects → ReplaceRefs re-points bones) | **0 fires** — counted, all members |

**Why native's remap is dead — the mechanism, counted:** the HX_NATIVE `FilterSubdir`
white-texture shim (`BandCharacter.cpp:4433`, the 2026-06-06 fix for white char
textures) converts **every on-disk resource subdir** from `kMerge` to `kReplace` —
including `char/main/shared/char_shared.milo`, the subdir that carries the skeleton
merge (not just the texture palette it was written for). A `kReplace`d subdir's objects
are **never iterated through `Filter`**, so `o1->Dir() == sBoneMergeDir` can never match
and the retail bone-remap cannot fire. Turn the shim off and the same remap fires
**31,488×/member** and native's binding flips to per-member (distinct=0, bound == the
member's own instance) — **exactly matching Wii's topology**. This is a native-introduced
regression, not a decomp defect: our texture fix silently disabled the retail skeleton
re-point.

This **supersedes the 2026-06-06 "shim-off didn't change binding" record for hand
meshes** (that arm measured torso `upArmPtr` at *draw* time; this measures hand meshes at
*rebind-entry* — different population, now instrumented with branch hit-counts the old
arm lacked).

Layer 1 is **separable in principle**: the shim only needs `kReplace` for the
texture-palette subdirs; scoping it to leave `char_shared.milo` at retail `kMerge` would
restore the bone remap *and* keep the white-texture fix. But — see Layer 2 — Layer 1
alone is not landable.

### Layer 2 — per-member binding is necessary but NOT sufficient (PROVEN insufficient; mechanism inferred)

The decisive result, and the **A11 discharge**: the shim-off arm delivers *exactly* what
the "make the loader produce the Wii object" hypothesis wanted — per-member hand binding,
one instance per member — **and the hands still visually fling** (worse than the shipped
clamped state; `W20-NATIVETRACE/evidence/noshim_shimOFF_gameplay.png`).

This is the **same composition** as the VISUAL-refuted 8th dead cell
(`RB3_HANDS_AUTHORED_REPOINT`): authored offsets `inv(B)` applied on the per-member
animated bone `own` = `inv(B)·L_own(t)`. Two independent arms — the Wave-16 post-load
repoint AND the Wave-20 load-time shim-off — reach that composition and **both fling**.
So per-member binding topology alone re-enters the dead class.

**What the Wii object has that both arms lacked (A11):** the per-member
`skeleton_unshared.milo` instance must be **posed to the outfit's bind at rest** so that
`L_own(t)` sits at the authored bind basis B (`inv(B)·L_own ≈ I` at rest, articulating
correctly). Native's fresh `kInlineCached` per-member skeleton is the raw **male-template
bind** — 87° off B at the distal finger bones (VERDICT §2's seed-R), and ~29° off for the
female. On Wii the outfit/clip poses each per-member skeleton to its gender; native never
applies that pose. **Gender/bind-posed per-member rest basis is the missing sufficient
ingredient** — corroborated three ways: the 8th-cell numeric+visual refutation, the
Wave-20 shim-off visual, and VERDICT §2's committed 87°/29° basis measurement.

**Honest limit (walled, not concluded):** the Dolphin substrate could not fully close
Layer 2's *precise* mechanism. Lane W's Wii capture bound only 6 hand bones (the
headless gameplay LOADING wall — the same subsystem gate behind GT-D) and reached **no
female member** (Guest lineup is all male-count). So the Wii finger-level and female
bind-pose basis is not in hand. Two candidate Layer-2 mechanisms remain unseparated by
committed data:
- **(L2-a) rest-basis pose**: per-member skeleton not gender-posed (the account above).
- **(L2-b) inter-bone blend tear**: even correctly posed, the finger mesh weights encode
  the *shared-bind inter-bone geometry*; per-member `own`'s animated inter-bone poses
  differ, tearing multi-bone finger blends (the SKEL seam-B / HANDS-FIX root-cause).
  Tension worth stating: the **torso** rebind to own-skeleton
  (`RebindOutfitBonesToOwnSkeleton`) *ships and works* — so per-member binding is not
  fatal for rigid-ish meshes; hands may differ only by (L2-b)'s multi-bone blends, or
  only because the torso path happens to pose correctly where the hand path does not.

### Layer D — decomp is faithful (PROVEN)

Rules out the kickoff's suspicion (c) entirely. `ObjectDir::PreLoad` at 100% means the
per-member fresh-skeleton `kInlineCached` load is decomp-clean (matches the doc's "loader
is correct"). The two behavioral HX_NATIVE blocks (ReplaceRefs LP64 reimpl; the
FilterSubdir shim) are semantics-preserving / the very shim Layer 1 indicts — not decomp
bugs.

## GT-D closure is UNTOUCHED

Every finding here is **static / load-time / rest-pose** — binding topology and bind-basis,
fixed before any clip drives. The GT-D closure concerns the *articulated* question and its
reopen condition is an *articulated* Wii capture; nothing in Wave 20 makes an animation
claim (Lane W held A11 strictly). This synthesis lives in the VERDICT §1 "(b)-at-load" fix
class that the adjudication explicitly placed **outside** the 8-dead-cell invariant.

## Wave 21 fix charter (hand-off)

The fix is a **two-part load-path change, landable only together** (Layer 1 alone
regresses — shim-off flings):

1. **Restore per-member binding** — scope the `FilterSubdir` shim so `char_shared.milo`
   (skeleton-bearing) takes retail `kMerge` while texture-palette subdirs keep `kReplace`
   (white-tex fix preserved), OR add a targeted post-merge re-point equivalent to the
   retail `sBoneMergeDir` remap. Flag-first, default-OFF, HX_NATIVE.
2. **Pose the per-member skeleton to the outfit's gender bind** so `L_own` at rest == B —
   the sufficient ingredient A11 names. This is where the torso path
   (`RebindOutfitBonesToOwnSkeleton`, shipping) is the proven precedent to study first:
   *why does torso-to-own work while hand-to-own (8th cell / shim-off) tears?* Answering
   that separates L2-a from L2-b and picks the fix shape.

**Pre-registered gate (inherited):** G-FIX-E1 — matched fixed-clock burst_08/burst_12,
ceiling-hand AND spike-fan GONE, both genders, gloves+fingernails non-regressing; numeric
gates alone are insufficient (the Wave-16 lesson). Plus drawlog-792 flag-OFF byte-identical,
`batch_objdiff` == baseline on touched units, guard-DROP census 0.

**Do NOT** re-attempt any of the 8 dead offset-bake cells or the reskin (VERDICT §5). A
Layer-1-only landing that reproduces `inv(B)·L_own` without the Layer-2 pose is the dead
class by the back door (A11).

**Substrate note for Wave 21's Layer-2 discriminator:** the Wii finger/female bind-pose
basis needed to fully separate L2-a/L2-b is walled on the current headless Dolphin
substrate (gameplay LOADING wall + no female in Guest lineup). Closing it needs either a
saved band with a female member / a boot mode that assembles one, or a native-side
discriminator (compare native per-member `own` rest basis vs authored B, and torso-vs-hand
rebind behavior) that does not depend on Wii articulation.

## Evidence index
- Lane W: `W20-WIITRUTH/` (STATUS, evidence/wii_binding_*.json + basis matrices, tool
  `milo-trace tools/wii_mesh_binding.py`).
- Lane N: `W20-NATIVETRACE/` (STATUS, branch_hitcount_table, a10 tables both arms,
  control vs shim-off gameplay PNGs, probes in `BandCharacter.cpp`/`Mesh.cpp`).
- Lane D: `W20-DECOMPAUDIT/` (STATUS verdict table, per_function_diagnose,
  hx_native_ifdef_enumeration, Filter_full_listing).
