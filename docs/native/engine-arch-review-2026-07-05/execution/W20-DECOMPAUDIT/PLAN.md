# Lane D — decomp-fidelity audit PLAN (Wave 20)

## Mission
Per-function fidelity verdicts (FAITHFUL-BY-MATCH / COSMETIC-DIFF / SEMANTIC-SUSPECT)
for the load/merge/share chain that decides skeleton binding. Direct answer to the
owner's question "is it a bug in decomp code? where?". READ-ONLY on src/.

## Method (A8/A9 binding)
1. batch_objdiff gate on the sub-100 shortlist → confirm current match%.
2. For each sub-100: run_diff_inspect mode=diagnose + mode=mismatches. Cite EXACT
   mismatching instructions. Classify regalloc/scheduling/pool-base/SDA-reloc/bool-mat
   = COSMETIC; changed condition/compare/branch-target/missing-case = SEMANTIC-SUSPECT.
3. BandCharacter has NO DC3 analog → anchor on Bank-8 asm; gate with bank_divergence.py
   BEFORE trusting Bank-5. DC3 valid for obj/Dir, obj/Utl, char/FileMerger.
4. Special attention: Filter dir-compare chain + early-return structure (a wrong dir
   compare / inverted condition here silently disables the remaps).
5. Enumerate every HX_NATIVE/HX_WII ifdef on the audited load path; classify binding-relevance.
6. For any SEMANTIC-SUSPECT: state the BINDING consequence (changes which dir answers
   FindObject / whether ReplaceRefs fires / merge action). Document, do NOT fix.

## Shortlist (pre-gated from report.json by review A8)
Deep audit: Filter 95.60 (P1), __as copy-assign 95.17, ReplaceRefs 98.79,
OnInstallFilter 99.11, LoadSubDir 99.39, PostLoadInlined 99.80.
Cosmetic-scan: LoadFile 99.96, LoadInlinedFile 99.97.
FAITHFUL-BY-MATCH free (100%): FileMerger MergeAction/Filter/FilterSubdir,
MergeDirs/MergeObjectsRecurse, DefaultSubdirAction, ObjectDir PreLoad/FindObject,
DirLoader::Find, BandCharacter::FilterSubdir(Wii).

## Banned (VERDICT §5): the 8 dead offset-bake cells + reskin. Evidence only, no fixes.

## Deliverables
PLAN.md (this), STATUS.md (verdict table), evidence/ (objdiff gate, per-function
diagnose, HX_NATIVE ifdef enumeration, Filter full listing), checkpoint /tmp/wave20-checkpoints/D.json.
