# Lane D — HX_NATIVE / HX_WII ifdef enumeration on the load/merge/share/binding path

Method: enumerate every `#ifdef HX_NATIVE` block that intersects an AUDITED function
(ReplaceRefs, Filter, FilterSubdir, OnInstallFilter) or the ObjPtr/Dir load path, and
classify each as BINDING-RELEVANT or not. Line numbers @ BandCharacter.cpp (62bc897f-era tree).

KEY PRINCIPLE: objdiff compiles the **Wii-target flavor** (HX_NATIVE undefined), so every
`#ifdef HX_NATIVE` block is INVISIBLE to the match% audit above — the COSMETIC verdicts
describe the `#else`/Wii code that ships on the target. The HX_NATIVE blocks are what the
NATIVE build actually runs; they are audited here for behavioral divergence separately.

| lines | function | block content | binding-relevant? | classification |
|---|---|---|---|---|
| 4217-4267 | ReplaceRefs | `#ifdef HX_NATIVE` reallocation-safe reverse-walk rewrite (`while(true){for i=sz-1..0 ...}`); `#else` = Wii iterator `it[-1]` reverse walk | YES (on remap path) | FAITHFUL-REIMPL — semantics identical: same match set {sOutfitDir,sResourceDir,sToDir}, same `theirs!=mine` guard, replaces every qualifying ref, restart-after-replace. Documented reason: clang/LP64 iterator-invalidation UB that MWCC/PPC tolerated. NOT a decomp bug; a UB-safety port that preserves behavior. |
| 4305-4310 | Filter | `if(LoadBindProbeOn()) ++gFilterCallTotal; if(!sBoneMergeDir)++gFilterBoneMergeDirNull;` | NO (probe only) | PROBE (Wave-20 Lane N hit counter, default-OFF via LoadBindProbeOn) |
| 4312-4314 | Filter | `if(LoadBindProbeOn())++gFilterBranchHits[0];` (sCharSharedDir branch) | NO (probe) | PROBE |
| 4325-4327 | Filter | `++gFilterBranchHits[1];` (sInstrument/sInstResource branch) | NO (probe) | PROBE |
| 4338-4349 | Filter | `++gFilterBranchHits[2]` + `[LOADBIND_BR2]` fprintf (outer sBoneMergeDir :4202 entered) | NO (probe) | PROBE |
| 4354-4356 | Filter | `++gFilterBranchHits[3]` (sBoneMergeDir ReplaceRefs FIRED) | NO (probe) | PROBE |
| 4386-4459 | FilterSubdir | `#ifdef HX_NATIVE` white-texture shim: DefaultSubdirAction then override kMerge→kReplace when `o1 && !o1->mStoredFile.empty()`; RB3_LOADBIND_NOSHIM env can disable; probe logging. `#else` = plain `DefaultSubdirAction(o1, front()->mSubdirs)` | YES (documented contributor) | DELIBERATE-DIVERGENCE — the ONE behavioral load-path change. Keeps external shared-resource subdirs as REFERENCES (kReplace) not merged (kMerge). Documented (comment 4413-4457) + 2026-06-06 record: proven to NOT change skeleton binding topology (shim-off = same shared root). Wii `#else` path is 100%-matched. This is Lane N's A/B arm, NOT Lane D's bug. |
| 4534-4561 | OnInstallFilter | Probe C: dump the 5 static dirs (sBoneMergeDir/sCharSharedDir/…) + branch-hit counters per install | NO (probe) | PROBE (default-OFF) |
| 4569-4583 | OnInstallFilter | `if(LoadBindProbeOn())` reset gFilterBranchHits[]/gFilterCallTotal at install | NO (probe) | PROBE |

## obj/Dir.cpp load path
LoadSubDir / LoadFile / LoadInlinedFile / PostLoadInlined / PreLoad / FindObject / __as:
NO HX_NATIVE ifdef in the AUDITED bodies (grep-verified). DC3's LoadSubDir carries an
HX_NATIVE `SetParentDir(this)` parent-propagation block that RB3 does NOT have — RB3's
LoadSubDir is the plain shared decomp on both flavors, so its COSMETIC verdict holds for
native too. (RB3 uses MILO_WARN where DC3 uses MILO_NOTIFY — a string-macro diff, not behavioral.)

## Conclusion
Only TWO HX_NATIVE blocks on the binding path carry behavior (not probes):
1. ReplaceRefs reimpl — FAITHFUL (semantics-preserving UB fix).
2. FilterSubdir white-texture shim — DELIBERATE, already exonerated for binding by the
   2026-06-06 record; it is Lane N's reconciliation subject, out of Lane D's read-only scope.
Every other HX_NATIVE block on this path is a default-OFF Wave-20 probe (LoadBindProbeOn-gated),
byte-invisible to the Wii target and to the native default run.
