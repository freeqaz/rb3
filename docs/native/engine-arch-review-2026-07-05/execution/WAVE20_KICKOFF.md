# Wave 20 — Kickoff (HANDS ROOT-CAUSE: the load path, not another bake)

**Author:** coordinator. **Status:** DRAFT — for Fable pre-dispatch review.
Parent: `execution/README.md` (Wave 19 results) + owner redirect 2026-07-08: *"zoom out on
the hands to figure out the actual root cause — is it a bug in decomp code? where is it?
we're patching around but there are answers if we are smart."*
Engine pin `6e6387c`. TWELVE defaults ON. **AUDIT WAVE — zero fixes, zero flips, zero pin
bumps; probes flag-gated default-OFF only.**

## COORDINATOR ACCEPTANCE (<pending review>)

_To be filled from `WAVE20_REVIEW.md`._

- **Hazard note:** engine tree carries uncommitted `M FxSendNative.cpp`; rb3 tree carries
  `native/src/rb3_session_trace.cpp` — never stage either.

## Why this wave exists (the zoom-out)

The hands family closed GT-D because ARTICULATED Wii ground truth was unobtainable
(CharClipDrivers=0 on the patched-disc substrate). But the record's own root-cause
statement was never attacked at its source. `HANDS-ADJUDICATION/VERDICT.md` §1:

> the defect is **(b)-at-load — the loader/merge never produces the Wii object** (one
> per-member instance that both carries the authored bind basis B and receives
> animation; the documented `kInlineCached`-under-preloaded-share divergence + the
> never-firing `sBoneMergeDir` ReplaceRefs remap, `BandCharacter.cpp:4159-4181`) — and
> every downstream bake was an attempt to synthesize that object out of the two halves
> native does have.

All 8 dead cells + the refuted reskin are POST-LOAD constructions; the verdict's §3
invariant proves that class dead *as a class*. The one fix class never attempted is the
load-time one (`FilterSubdir` comment: "the faithful fix must un-share
`char/main/skeleton.milo` for the band at the name-resolution/share layer" — labeled
broad/high-risk in 2026-06-06 and shelved for the clamp). Before any fix can be
chartered we need three facts nobody has established:

1. **What the Wii loader actually produces** (per-member skeleton instances? shared?
   where do hand-mesh bone refs point after a real Wii load?) — a STATIC question,
   answerable on the frozen Dolphin substrate that GT-D only disqualified for
   *animation*. Static reads are D2/D4-validated on that rig.
2. **Where native diverges from that** — candidate mechanisms, in suspicion order:
   (a) the HX_NATIVE `FilterSubdir` kMerge→kReplace shim (`BandCharacter.cpp:4236-4286`)
   changes subdir/object identity, plausibly disabling the `sBoneMergeDir` ReplaceRefs
   remap at `:4202` and the `sCharSharedDir`/`sInstrumentDir` remaps above it;
   (b) poll-interleaved native loader vs Wii atomic loads changing merge/name-resolution
   order (`ObjectDir::LoadSubDir` share=true first-loader-wins);
   (c) a plain decomp bug in the merge/share chain.
3. **Whether the decomp of that chain is even faithful** — match% + asm audit of the
   specific functions whose semantics decide binding.

If (1) shows Wii hand meshes bound to per-member animated instances and (2) names the
native step that loses that, the fix stops being "broad, high-risk, un-share everything"
and becomes a targeted load-path correction — the first fix candidate in this family
that is NOT in the dead class.

## Lanes (all Opus, all AUDIT-ONLY)

**Lane W — Wii load-truth, static (Dolphin patched-disc; milo-trace tooling):**
Boot the D2 patched disc (`/home/free/tmp/wave17-d2/`, `tools/wii_bone_dirboot.py` /
`wii_visgame_capture.py` in `../milo-trace`), reach main_hub band lineup (VISCAP nav is
proven). With STATIC memory reads only (no animation required), answer per band member:
(a) how many distinct skeleton ObjectDir instances exist (map symbols + heap walk —
count `skeleton.milo` / `skeleton_unshared.milo` dir instances); (b) for each member's
hand meshes (`hands_naked.mesh`, gloves), which skeleton instance do the mesh's bone
trans refs resolve to — the member's OWN dir or one shared root; (c) same for one torso
mesh as a control (native torso rebind ships, so Wii-vs-native torso binding is the
calibration row). Deliverable: the **Wii binding table** — mesh × member → owning-dir
identity + bone-count + gender label. Fail-red: the harness must LOUDLY distinguish
"couldn't resolve mesh bone refs" from "resolved to shared". Evidence:
`execution/W20-WIITRUTH/evidence/`.

**Lane N — native load-path divergence trace (rb3-side; probe flags default-OFF):**
Instrument the native load of the same lineup: (a) log `sBoneMergeDir` /
`sCharSharedDir` / `sInstrumentDir` values at `OnInstallFilter` and whether the
`:4202` ReplaceRefs branch EVER fires for `bone_*` transformables (hit counts per
merge); (b) log each hand-mesh `BoneTransAt` dir identity at end-of-load (before any
rebind/rebake runs — gate probe earlier than `RebindHeadHandsAtRest`); (c) A/B the
`FilterSubdir` HX_NATIVE shim: run one boot with the shim bypassed for the
skeleton-relevant subdirs ONLY as a PROBE arm (textures may go white — irrelevant,
we're reading binding, not pixels) and record whether binding topology changes;
(d) map the mechanism: which load event establishes the shared-root binding natively
(name-resolution share vs merge), with the CHAR_SKINNING_DEFORM_INVESTIGATION.md claims
re-verified not assumed. Deliverable: causal chain from load sequence → shared-static
hand binding, with the native deviation NAMED (shim / interleaving / decomp bug), plus
the same binding table as Lane W for native so the two join row-for-row. Evidence:
`execution/W20-NATIVETRACE/evidence/`.

**Lane D — decomp-fidelity audit of the merge/share chain (read-only + objdiff):**
For the functions whose semantics decide binding — `BandCharacter::OnInstallFilter`,
the merge filter (`:4180-4232` region incl. the `sBoneMergeDir` branch),
`FilterSubdir`/`DefaultSubdirAction`, `FileMerger::MergeAction`+poll path,
`ObjectDir::LoadSubDir`/`LoadInlinedFile`/inline-dir-type handling (`Dir.cpp:321`
region), `ReplaceRefs` — establish: current objdiff match% (batch_objdiff), and for
every sub-100% function whether the mismatch is cosmetic or SEMANTIC (wrong condition,
wrong dir compare, missing case) using `bin/analyze-function` (Bank8 default) +
`scripts/analysis/bank_divergence.py` before trusting any Bank5 body + DC3 reference
(`/home/free/code/milohax/dc3-decomp/`). Also audit kInlineCached vs kInlineCachedShared
handling vs Bank8. Deliverable: per-function verdict table (FAITHFUL / COSMETIC-DIFF /
SEMANTIC-SUSPECT with the exact suspect line) — this is the direct answer to "is it a
bug in decomp code?". Evidence: `execution/W20-DECOMPAUDIT/evidence/`.

**Coordinator synthesis (post-lane, Fable):** join W×N tables → is the Wii object real
and what loses it natively; fold in D's verdicts → root-cause statement + Wave-21 fix
charter (or an honest "load path faithful, divergence is X" if the hypothesis dies).
The 8 dead cells + reskin stay banned regardless of outcome (VERDICT §5 option table).

## Process rules (carried) — VERBATIM per KICKOFF_TEMPLATE.md

Locks: rb3 `/tmp/rb3-git.lock`; engine `/tmp/milo-engine-git.lock`; classjson
`/tmp/milo-engine-classjson.lock`. Checkpoints `/tmp/wave20-checkpoints/<lane>.json` —
check-first, write-before-return, update every milestone. PLAN/STATUS under
`execution/<KEY>/`. Evidence committed or it doesn't exist. New probe flags default-OFF;
NO default flips, NO pin bumps by lanes. Refuted flags UNSET. TWELVE defaults stay ON.
Headless `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free ports, pgid-only cleanup. milo-trace repo
has its own norms (commit there directly). Builds under `/tmp/rb3-native-build.lock` or
own worktree (`tools/setup-worktree.sh`).

## Pre-dispatch checklist — the ten §4 lints

- [x] **1. Matrix-relative + pointer-verified bone claims** — W/N report POINTER/dir
  identity (the lesson's home turf); no scalar angle claims chartered.
- [x] **2. Split by population** — all tables per-member × per-mesh × gender.
- [x] **3. No unvalidated oracles** — W's fail-red (unresolved ≠ shared); N's A/B arm
  is a probe with its own control boot; D anchored on bank_divergence before Bank5 use.
- [x] **4. Shipped-flag contradiction grep** — audit-only; no lane touches the twelve.
- [x] **5. Grants** — W writes milo-trace + evidence only; N writes rb3 probe code
  (default-OFF) + evidence; D read-only + evidence.
- [x] **6. Option table before 2nd fix attempt** — NO fix attempts (audit wave); the
  banned-cell table is restated in every lane brief.
- [x] **7. Evidence committed** — carried.
- [x] **8. Flag hit-counts on negatives** — N logs per-branch hit counts (a
  never-firing ReplaceRefs must show count=0, not absence of a log line).
- [x] **9. Flavor-membership grep** — N step 0: verify probe TUs compile into
  rb3-native; D step 0: verify each audited symbol's unit is in the build.
- [x] **10. Instruments before fixes** — the wave IS the instrument.

## Risks / open questions for the reviewer

- **R-A (Lane W):** is main_hub the right Wii state, or does the closet/lineup state
  bind outfits differently than gameplay? Should W capture at BOTH main_hub and live
  gameplay (VISCAP reached both)?
- **R-B (Lane N):** is the shim-bypass probe arm safe to interpret (white-texture
  cascade may abort load earlier)? Should it be scoped to skeleton dirs only, and what
  is the control?
- **R-C (Lane D):** the merge-filter functions may be band3-side with no DC3 analog —
  what's the reference when DC3 lacks the function?
- **R-D (join):** W and N tables must join row-for-row — bless one shared schema
  (member, mesh, boneName, owningDirName, owningDirInstanceId, gender) now.
- **R-E:** anything in the GT-D closure or VERDICT §8 banned-citation list that this
  charter accidentally violates?
