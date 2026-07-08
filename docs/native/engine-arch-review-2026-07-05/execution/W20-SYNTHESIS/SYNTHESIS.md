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

---

## ERRATA (Wave-20 close-out review `d1e3faf0`, E1–E12 — append-only, binding)

The body above overstates Layer 2's mechanism and compresses a few Layer-1 facts. These
corrections govern; where they conflict with the body, THEY win. Layer 1's core result
(counted 0-vs-31,488 remap suppression + OWN-vs-SHARED topology flip + decomp-faithful)
survived adversarial re-derivation unchanged.

- **E1 (§Layer 1 table, Wii column):** "hand-mesh bone binding … 60/60" is loose — only
  **6/60 committed rows are hand-mesh** (`hands_naked.mesh`, member 0 only; the other 54
  are jacket/jeans/strap outfit meshes; rows span members 0/1/2, member 3 absent). Hand-
  specific Wii evidence = those 6 bones + one non-reproducible 38-bone `drivinggloves`→OWN
  sighting. The class result (OWN, SHARED_ROOT=0) holds; the row count is outfit-wide.
- **E2:** "exactly matching Wii's topology" → matches at the A10 `owningDirClass` level on
  every *reachable* row; full 38-slot per-slot uniformity on Wii is **walled**, and slot-
  mixing is a real failure mode (native 8/46, HANDS-ADJ §1).
- **E3:** "native-introduced regression" is fair for the **load-path topology only**, NOT
  for the hand visuals — the female fling predates the shim (2026-06-05) and worsens with
  the shim off.
- **E4 (§Layer 2, the key correction):** the "shim-off = the 8th cell's `inv(B)·L_own`"
  identity is NOT established. Lane N measured binding at rebind ENTRY; the shim-off arm set
  no banned flag, so the default seed-R rebake still ran → draw-time composition is
  plausibly `inv(R)·L_own` (**dead cell #1, the default**), not the 8th cell. Draw-time
  offsets were never dumped. **What is proven: per-member topology alone fails the visual
  gate. Which dead cell it reproduces is undetermined.**
- **E5:** the 87.2° is `angle(B·inv(R))` for the SetDeformation-time **transient seed R**
  (provenance expressly open, HANDS-ADJ §2) — NOT a measured rest basis of the fresh
  per-member skeleton. And **arm S measured the male `own` ≈ B (3.1°, 1038 blocks) during
  play** — standing counter-evidence to the L2-a story for males. The only committed
  gender-basis gap is the **female ~29°** (authored female offsets vs the shared male bind).
- **E6:** "gender/bind-posed rest basis IS the missing sufficient ingredient — corroborated
  three ways" → it is the **leading CANDIDATE (L2-a)**; sufficiency is untested and arm S's
  male 3.1° contradicts it for males. The Wave-21 step-1 discriminator (torso-vs-hands,
  native-side) decides L2-a vs L2-b and must explain the male null.
- **E7:** "On Wii the outfit/clip poses each per-member skeleton to its gender" is an
  unevidenced Wii-animation assertion (substrate CharClipDrivers=0) → restate as: IF the
  Wii object reaches a gender-correct rest basis, some Wii-side mechanism poses it —
  hypothesis, not observed. (Does not trip the GT-D reopen condition.)
- **E8 (banned shorthand):** "the shipped clamped state" → the shipped default is the
  **seed-R rebake + mitten-ON**; the clamp SKIPS rebound band-hand meshes (R5 VERDICT §8.3).
  Both Wave-20 arms were mitten-ON, so the coherent-vs-flung asymmetry is a genuine
  **topology** effect, not clamp/mitten mediation.
- **E9:** the FilterSubdir shim is NOT "semantics-preserving" — it is a DELIBERATE
  semantics-changing divergence (the Layer-1 mechanism itself), intentional, not a decomp
  defect. Only the ReplaceRefs LP64 reimpl is semantics-preserving.
- **E11:** "separable in principle" is a Wave-21 HYPOTHESIS, not established — the chartered
  skeleton-scoped shim arm was never run, and `char_shared.milo` carries palette content, so
  restoring its kMerge may reintroduce white textures. Wave-21 gate list ADDS: char-texture
  integrity on the scoped-shim arm (no white-texture regression).
- **E12:** the Wave-21 charter's part 2 ("pose to B") presumes L2-a; if the step-1
  discriminator lands L2-b, part 2 is the wrong shape → re-charter. Reskin stays banned
  (R5 §8.4/§5) regardless.

**Net after errata:** the durable, proven Wave-20 result is **Layer 1** — native binds the
shared male-bind magnet because our white-texture shim suppresses the retail per-member
bone remap (counted), the decomp is faithful, and Wii binds per-member. **Layer 2** is
proven at the necessity level (per-member binding alone still fails) and OPEN at the
mechanism level (L2-a gender-pose vs L2-b blend-tear undetermined; female gender-gap
committed, male case counter-indicated). Wave 21 lands Layer 1 + Layer 2 together or not at
all, with the torso-precedent discriminator deciding Layer 2's shape.
