# Lane D — per-function semantic diagnose (exact mismatching instructions)

Each function's mismatches captured from `run_diff_inspect mode=diagnose/mismatches`.
Enum: MergeFilter::Action { kMerge=0, kReplace=1, kKeep=2, kIgnore=3 } (src/system/obj/Utl.h:136-140).

## Filter (94.76% raw / 95.60% report) — bandobj/BandCharacter — COSMETIC-DIFF
Root cause: target build saves one extra callee-save reg (idx4 `_savegpr_24` vs base `_savegpr_23`)
→ UNIFORM register-allocation shift (r23→r24, r26→r27, r28→r29, r29→r30). diagnose:
"Register swaps: 171 instructions across 12 pairs"; "Unexplained diff_args: 0".

3 diff_op, all beq↔bne PAIRED with a same-target branch a few instrs later (= bool-mat / block reorder):
- idx 62: TGT `beq 0xcbe0` / SRC `bne 0xe5f0`. Pairs with idx 71 (both →0xcbe0/0xe5f0). Region = the
  `!o2 && ClassName()==AmbientOcclusion/CharWeightSetter` short-circuit; base has an extra `b 0xe5bc`
  (idx 42) that mirrors the block layout. Returns identical (idx58-59 li r3,0x3=kIgnore; idx75-76 li r3,0x2=kKeep).
- idx 356: TGT `bne 0xd0ac` / SRC `beq 0xeaa8`. THE sBoneMergeDir guard region. TGT materializes
  `!(o1->Dir()==sOutfitDir||sResourceDir||sToDir)` into r3 (idx354 `li r3,0x1`; idx355 `cmpwi r3,0x0`;
  idx356 `bne`) — base (idx353-355 deleted) branches directly. BOTH then load sBoneMergeDir (idx357
  `lwz r0,0x54,r31,sBoneMergeDir`), cmplw, RndTransformable dynamic_cast (idx360-367), FindObject,
  ReplaceRefs. Branch INTO the sBoneMergeDir remap is structurally preserved.
- idx 419: TGT `beq 0xd150` / SRC `bne 0xeb4c`. The `bone_`/`exo_` strnicmp return. idx420-423 the two
  return blocks emitted in SWAPPED order (TGT li r3,0x2 then li r3,0x0; SRC li r3,0x0 then li r3,0x2),
  so branch polarity flips to pick the same value. strnicmp==0 → kMerge(0x2); else kKeep. Identical.

Dir-compare chain byte-faithful (operand + branch structure):
  sCharSharedDir  idx230 `lwz r0,0x60,r31,sCharSharedDir; cmplw; bne`
  sInstrumentDir  idx285 `lwz r0,0x64,r31,sInstrumentDir`
  sInstResourceDir idx288 `lwz r0,0x68,r31,sInstResourceDir`
  sOutfit/sResource/sToDir idx344-351
  sBoneMergeDir   idx357 `lwz r0,0x54,r31,sBoneMergeDir`
(offsets differ from base by whole-struct SDA layout; SYMBOLS identical.)

## OnInstallFilter (99.05% / 99.11%) — COSMETIC-DIFF
1 diff_op idx112 beq↔bne, driven by idx110 `cntlzw r0,r3` (TGT) vs `cmpwi r3,0x0` (SRC) — the MWCC
count-leading-zeros bool-materialization idiom for a pointer null test (one of the `if(xfm)`/`if(feetObj)`/
`if(pelvis)` null checks that gate the STORES sBoneMergeDir=xfm->Dir() / sCharSharedDir=feet->Dir()).
Value stored is identical. "Unexplained diff_args: 0" (rest = stack offset shift -84 + r3↔r4 regalloc).

## ReplaceRefs (98.79%) — Wii #else path — COSMETIC-DIFF
- idx 49/52/55 diff_arg: `lwz r0,0x58/0x5c/0x6c,r29,{sOutfitDir/sResourceDir/sToDir}` vs base 0x4/0x8/0x18
  = SDA base-offset reloc noise; SAME static symbols; cmplw + beq/bne targets byte-identical.
- idx 75 delete `b 0xca74`: target re-jumps to loop-condition test after the `it=end()` reset;
  base falls through. BOTH reach the same cond block (idx76-78 subi/cmplw/bne). Redundant MWCC jump.
Match set {sOutfitDir,sResourceDir,sToDir} + `theirs!=mine` guard (idx61-62) + ref->Replace (idx63-69) faithful.

## LoadSubDir (99.38%) — COSMETIC-DIFF
- idx 49/185/301/461 diff_op: `subi r3,r3,0x2a90` (TGT) vs `addi r3,r3,TheLoadMgr` (SRC) — SAME global
  &TheLoadMgr, different SDA-relative representation. Linker/layout, not semantic.
- idx 265-267 delete `addi r3,r1,0x14; li r4,0; bl __dt__6StringFv`: target has a duplicated
  `~String` cleanup site for the `FilePath subdirpath` local; base coalesces both paths into one
  `~String` epilogue (idx263/268 both `b 0x9940`). String destroyed once per path in both. Identical.
`mSubDirs[i].LoadFile(subdirpath, share=true, ...)` (the shared-skeleton-root establishment call) is faithful.

## PostLoadInlined (99.77%) — COSMETIC-DIFF (inline-cache resolution)
10 diff_arg idx137-146: a two-halfword swap where TGT uses stack slots [r1+0xc/0xe] + member [r31+0x70/0x72]
and base has the two slots' ROLES swapped (lhz/lhz/sth/sth interleave, std::swap-style). Same two 16-bit
values exchanged. Stack-layout/scheduling swap only. No change to which inlined dirs resolve.

## __as ObjDirPtr<ObjectDir> copy-assign (95.17%) — COSMETIC-DIFF
idx36-63: TGT loads the two ObjPtr fields (0x8, 0x4 off r30) directly into r3; base loads into r0 then
adds a redundant `mr r3,r0` (idx39/60), shifting the vtable-deref by one (insert idx42/63). Classic
r0-vs-named-reg allocation + one redundant move. Same fields (0x8,0x4,0x0), same vtable slots (0x8,0x24).
Refcount (AddRef/Release) management faithful. Not a dir-identity/name-resolution function.

## LoadFile / LoadInlinedFile (99.96 / 99.97%) — COSMETIC-DIFF (cosmetic scan)
LoadFile: 2 diff_op idx40/202; LoadInlinedFile: 1 diff_op idx39 — ALL the `subi r3,r3,0x2a90` vs
`addi r3,r3,TheLoadMgr` &TheLoadMgr SDA materialization. Nothing else. Trivially faithful.
(These are the LoadFile(share=true) and kInlineCached LoadInlinedFile calls the CHAR_SKINNING doc names.)
