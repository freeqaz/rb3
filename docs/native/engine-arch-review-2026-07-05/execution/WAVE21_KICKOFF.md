# Wave 21 — Kickoff (HANDS FLAGSHIP FIX: the two-part load-path change)

**Author:** coordinator. **Status:** DRAFT — for Fable pre-dispatch review.
Parent: `W20-SYNTHESIS/SYNTHESIS.md` (+ its E1–E12 errata) + `WAVE20_CLOSEOUT_REVIEW.md`.
Engine pin `6e6387c`. TWELVE defaults ON. **FIX WAVE** — real flag-gated implementation,
default-OFF; coordinator-only default decisions at close-out (a hands default-ON flip would
need extraordinary matched-frame evidence given this bug's history).

## COORDINATOR ACCEPTANCE (<pending review>)

_To be filled from `WAVE21_REVIEW.md`._

- **Hazard note:** engine tree carries uncommitted `M FxSendNative.cpp`; rb3 tree carries
  `native/src/rb3_session_trace.cpp` — never stage either.

## The state entering this wave (from Wave 20, errata-corrected)

- **Layer 1 (PROVEN):** native binds hand meshes to the shared male-bind magnet because our
  white-texture `FilterSubdir` shim (`BandCharacter.cpp:4433`, kMerge→kReplace on every
  on-disk resource subdir incl. skeleton-bearing `char_shared.milo`) suppresses the retail
  `sBoneMergeDir` bone remap (counted 0 vs 31,488/member shim-off). Wii binds per-member.
  Decomp is faithful (Lane D). The `RB3_LOADBIND_NOSHIM` flag already flips the shim.
- **Layer 2 (necessity PROVEN, mechanism OPEN):** per-member binding alone still flings.
  WHICH dead composition is undetermined; L2-a (gender rest-pose) vs L2-b (animated
  inter-bone blend tear on thin geo) unseparated; female ~29° gender-gap committed, **male
  `own`≈B at rest (arm S 3.1°) is counter-evidence to a pure rest-basis story for males.**

**Two code facts that sharpen the whole wave (verified this session):**
1. `RebindOutfitBonesToOwnSkeleton` (the torso rebind that SHIPS and works,
   `BandCharacter.cpp:1211`) IS `SetBone(b, own, /*calcOffset*/false)` — the *same
   operation* as the refuted 8th cell — and it is deliberately scoped **torso-only**
   (`sTorsoOnly`, :1239) precisely because "the high-bone head/hands/face … long-thin
   geometry shards under the rotation-basis mismatch" (:1236). `RB3_SKEL_REBIND_FULL=1`
   already rebinds hands and already shards them ("for study only", :1237). So the
   torso-vs-hands discriminator is *half-answered in the tree*: torso tolerates the
   per-member basis mismatch; thin multi-bone hand geo tears under it.
2. That FULL-rebind / 8th-cell / shim-off path re-points via `Find()` at **Poll** time. The
   retail `sBoneMergeDir` remap re-points during **merge (load)** — a *different time, to a
   differently-resolved instance*. **The faithful Layer-1 fix is NOT equivalent to
   RB3_SKEL_REBIND_FULL** and may land on a correctly-posed instance where FULL-rebind does
   not. This is the central untested hope of the wave — so the FIX lane must implement the
   ACTUAL retail remap restoration, not reuse the FULL-rebind approximation.

## The decisive question this wave answers

**Under a FAITHFUL Layer-1 fix (retail per-member binding restored via the merge remap), are
MALE hands coherent or torn — matched-frame, gender-split?**
- **Coherent** (arm-S `own`≈B predicts this) ⇒ Layer 1 largely fixes males; only females
  need a Layer-2 gender-pose (tractable, torso-pattern). The flagship is completable NOW.
- **Torn** (Lane-N shim-off suggests this) ⇒ the residual is the L2-b animated inter-bone
  blend tear — the same *animated* question R5 walled (articulated Wii GT unobtainable,
  CharClipDrivers=0). Honest outcome: Layer 1 is a real separable regression-fix that
  **cannot ship alone** (flings), and the remaining half is blocked on the R5 wall or the
  banned reskin. A sharp characterization, not a failure to paper over.

Either way Wave 21 converts the flagship from "closed GT-D, patched" to a **named,
gender-split, mechanism-decided** state — and possibly ships the fix.

## Lanes

**Lane FIX — the faithful two-part fix, flag-first (Opus; SOLE rb3 BandCharacter writer):**
Implement, default-OFF, HX_NATIVE, `src/system/bandobj/BandCharacter.cpp`:
- **Part 1 (Layer 1):** a NEW flag (e.g. `RB3_HANDS_BINDFIX`) that SCOPES the FilterSubdir
  shim so `char_shared.milo` (skeleton-bearing) takes retail `kMerge` while
  texture-palette-only subdirs keep `kReplace` — restoring the `sBoneMergeDir` remap for the
  bone dir WITHOUT reintroducing white textures. (If clean subdir-scoping proves impossible,
  the fallback is a targeted post-merge re-point replicating the remap's effect — but try the
  faithful merge path first.) MUST NOT reuse `RB3_SKEL_REBIND_FULL` (different time/target).
- **Part 2 (Layer 2), CONDITIONAL:** only if Part 1's matched-frame male result still shards
  OR the female still flings, add the gender-pose arm — pose each per-member hand skeleton
  to its outfit bind so `L_own(rest)==B` (study the shipping `RebindOutfitBonesToOwnSkeleton`
  torso path as the precedent; this is NOT a reskin and NOT any of the 8 dead offset-bake
  cells — it changes the SKELETON's rest, not the mesh offsets/verts). Separate flag.
- **Gates (ALL, gender-split, matched fixed-clock burst_08/burst_12):** G-FIX-E1 (ceiling-hand
  AND spike-fan GONE, both genders, gloves+fingernails non-regressing) — numeric gates alone
  are INSUFFICIENT (Wave-16 lesson); **char-texture-integrity gate** (E11: no white-texture
  regression under the scoped shim — capture the band lineup, verify skin/cloth textures
  resolve); `batch_objdiff` == baseline on touched src/system units (G3 Wii-match); flag-OFF
  drawlog-792 byte-identical; guard-DROP census 0; crowd oracle untouched. Report PASS/FAIL
  per gate with committed matched-frame evidence. Do NOT flip any default (coordinator E1).

**Lane DISCRIM — the male-coherence / L2-a-vs-L2-b decider (Opus; measurement, native-side):**
The rigorous version of the decisive question, reconciling arm-S (`own`≈B males) vs Lane-N
(shim-off flings):
- Under per-member binding (drive it via Lane FIX's Part-1 flag once available, else
  `RB3_LOADBIND_NOSHIM` as the interim substrate), measure `own`'s basis vs the authored bind
  B **at the DRAW frame** (not just rebind-entry — the arm-S/Lane-N timing gap lives here),
  gender-split, at matched burst frames. Distinguish REST-basis divergence (L2-a) from
  ANIMATED inter-bone divergence (L2-b, the two-adjacent-finger-bone RELATIVE-pose delta the
  Wave-16 HANDS-FIX §"Dolphin fallback" named as the correct instrument).
- **Verdict (mechanical):** males coherent-basis at draw ⇒ Layer 1 fixes males, residual is
  female L2-a (tractable); males divergent inter-bone at draw ⇒ L2-b, the R5-walled animated
  question. Must EXPLAIN the arm-S male 3.1° null (rest≈B) against whatever the draw-frame
  shows. Fail-red: the metric must read ≈0 on a known-good body mesh (torso) and separate on
  the torn hand mesh (the R2 good-body vs bad-torn fixture precedent).
- Charter honesty: if the verdict is L2-b/walled, say so plainly — do NOT manufacture an
  L2-a story to keep the fix alive (the exact E5/E6 overclaim the close-out caught).

_Engine writers: NONE this wave (both lanes rb3-side). Lane FIX owns BandCharacter.cpp; Lane
DISCRIM is measurement + scripts, reusing Lane FIX's flag as substrate — bless a landing
order (FIX Part-1 flag lands first; DISCRIM consumes it) or let DISCRIM bootstrap on
`RB3_LOADBIND_NOSHIM` until FIX's flag exists._

## Process rules (carried) — VERBATIM per KICKOFF_TEMPLATE.md

Locks: rb3 `/tmp/rb3-git.lock`; classjson `/tmp/milo-engine-classjson.lock`. Checkpoints
`/tmp/wave21-checkpoints/<lane>.json` — check-first, write-before-return, update every
milestone. PLAN/STATUS under `execution/<KEY>/`. Evidence committed or it doesn't exist. New
flags default-OFF; NO default flips, NO pin bumps by lanes (coordinator, ONCE, close-out).
The 8 dead offset-bake cells + reskin BANNED (VERDICT §5). Refuted flags UNSET. TWELVE
defaults stay ON. Headless `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free ports, frame-settling,
pgid-only cleanup. Build under `/tmp/rb3-native-build.lock` or own worktree. Stage only your
own files by path; NEVER `rb3_session_trace.cpp` / engine `FxSendNative.cpp`.

## Pre-dispatch checklist — the ten §4 lints

- [x] **1. Matrix-relative + pointer-verified** — DISCRIM compares matrices (L2-a/L2-b), not
  scalars (the saga's cardinal error); binding claims are pointer/dir-identity.
- [x] **2. Split by population** — everything gender-split (nb=38 male / 40 female); per-mesh.
- [x] **3. No unvalidated oracles** — G-FIX-E1 visual gate is decisive (numeric insufficient,
  Wave-16); DISCRIM's metric has the torso-good/hand-torn fail-red.
- [x] **4. Shipped-flag contradiction grep** — new flags default-OFF; no lane touches the 12.
- [x] **5. Grants** — FIX writes BandCharacter.cpp + evidence; DISCRIM scripts + evidence.
- [x] **6. Option table before 2nd fix attempt** — Part 2 is CONDITIONAL on Part 1's result;
  the 8 dead cells + reskin are restated banned in both briefs.
- [x] **7. Evidence committed** — matched-frame PNGs + gate logs or it didn't happen.
- [x] **8. Flag hit-counts on negatives** — FIX reports remap-fire counts under the fix;
  DISCRIM reports per-slot sample counts.
- [x] **9. Flavor-membership grep** — step 0: verify the edited TU compiles into rb3-native;
  confirm the scoped-shim actually flips char_shared.milo to kMerge (LOADBIND_SUBDIR log).
- [x] **10. Instruments before fixes** — DISCRIM's decider runs alongside; a failed G-FIX-E1
  is reported honestly (the R5-wall outcome is acceptable).

## Risks / open questions for the reviewer

- **R-A (Lane FIX):** is clean subdir-scoping of the shim achievable — can `char_shared.milo`
  be distinguished from texture-palette subdirs at the FilterSubdir site (name? stored-file
  path? subdir contents?), or is the post-merge re-point fallback forced? Does restoring
  kMerge on char_shared.milo actually reintroduce white textures (the E11 risk)?
- **R-B (Lane FIX):** is the faithful merge-remap path truly distinct from RB3_SKEL_REBIND_FULL
  in the instance it binds, or do they converge (making the "central hope" moot)?
- **R-C (Lane DISCRIM):** is the draw-frame own-vs-B basis measurable natively with existing
  probes (HANDS_ATTACH / the Wave-20 LOADBIND slot dump), or does it need a new draw-time hook?
- **R-D:** is the male-coherent-vs-torn split really the load-bearing decider, or could a
  female-only Layer-2 fix ship even if males need L2-b (i.e., is the shipped male hand already
  acceptable and only the female flings)? Bless the framing.
- **R-E:** anything in the GT-D closure / VERDICT §8 banned list this fix charter risks
  re-citing or re-attempting (esp. the reskin, and Part-2's "pose to B" not sliding into an
  offset-bake cell)?
