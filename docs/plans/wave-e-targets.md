# Wave E — Permuter target report

**Date**: 2026-05-27  
**Origin**: `reclassify_at_limit.py --apply` reopened 1,035 functions (NULL verdict, 80–99.9% match, non-Wii-specific units). Wave E fuses four scanner passes — AST-pattern, asm-level encoding, regswap-classify, and function_health — to rank the best permuter-grinding targets.

## Scanner summary

| Scanner | Hits (total) | In-scope | Notes |
|---|---|---|---|
| `pattern_scan` (AST) | 5,025 | 4,153 | Filtered `src/sdk/`, `src/network/`, `rndwii/`, `os/`, `synthwii/`, `lib/` |
| `batch_pattern_scan` (asm encoding) | 0 | 0 | No cached diffs for fresh-reopened fns; returns 0 — **scanner error / cache miss, not a real zero** |
| `regswap_classify` | 264 fns with CS swap (68 single-pair) | 37 matched to candidates | 17% estimated fixable via decl reorder |
| `function_health` | 30 sampled | 30 | All return `at_limit` verdict; 6 have FMA fixable category |

**Note on `batch_pattern_scan` returning 0**: This scanner requires cached `diff_*.json` files in `/tmp/claude/`. The 1,035 reopened functions have no cached diffs (they were AT_LIMIT before). Run `run_objdiff` on them first to populate the cache, then re-run `batch_pattern_scan`.

## Top 50 ranked targets

Scoring formula: `headroom×1.5 + actionable_ast_patterns×2 + bool_hit + regswap_fixable×3`

Higher-value actionable patterns: `member_readback`, `return_this_op_assign`, `bitpack_or_reorder`, `makestring_wrap_literal`, `store_then_compound_add`, `positive_branch_invert`, `demorgan_guard`, `switch_case_reorder`.

| Rank | Symbol | Unit | Match% | Head | AST patterns | Asm signal | Approach |
|---|---|---|---|---|---|---|---|
| 1 | `DoFancyElbow__14BandIKEffectorFR7QuatXfmf` | bandobj/BandIKEffector | 80.03 | 19.97 | – | FMA×6 | fma_reorder |
| 2 | `HandleRGGemStart__10SongParserFiRQ210SongParser14DifficultyInfoUcUcUci` | beatmatch/SongParser | 80.03 | 19.97 | member_readback+store_then | – | AST: member_readback |
| 3 | `UpdateScrolling__10VocalTrackFf` | bandtrack/VocalTrack | 80.07 | 19.93 | makestring_wrap+store_then | – | AST: makestring_wrap_literal |
| 4 | `SimulateZeroTime__8CharHairFv` | char/CharHair | 80.12 | 19.88 | switch_case_reorder | – | AST: switch_case_reorder |
| 5 | `Intersect__FRC7SegmentRC8TrianglebRf` | math/Geo | 80.24 | 19.76 | – | FMA×9 | fma_reorder |
| 6 | `CurrentTileRect__11HiResScreenCFRCQ23Hmx4RectRQ23Hmx4RectRQ23Hmx4Rect` | rndobj/HiResScreen | 80.28 | 19.72 | – | regswap | permuter-grind |
| 7 | `NumVerts__Q214BandFaceDeform10DeltaArrayFv` | bandobj/BandFaceDeform | 80.29 | 19.71 | – | – | permuter-grind |
| 8 | `_M_insert_overflow_aux__Q211stlpmtx_std81_Vector_impl<14OldColorOption...>` | bandobj/OutfitConfig | 80.41 | 19.59 | positive_branch+demorgan | – | AST: positive_branch_invert |
| 9 | `BuildBeam__9SpotlightFRQ29Spotlight7BeamDef` | world/Spotlight | 80.58 | 19.42 | bitpack+demorgan | – | AST: bitpack_or_reorder |
| 10 | `__dt__20StoreMetadataManagerFv` | meta/StorePackedMetadata | 80.73 | 19.27 | member_readback+bitpack | – | AST: member_readback |
| 11 | `AddDevice__6UsbWiiFP9HIDDevice7UsbType` | usbwii/UsbWii | 80.88 | 19.12 | bitpack+switch_case | – | AST: bitpack_or_reorder |
| 12 | `GrowToContain__6SphereFRC7Vector3f` | math/Geo | 81.18 | 18.82 | – | – | permuter-grind |
| 13 | `MeasureLengths__10CharIKHandFv` | char/CharIKHand | 81.35 | 18.65 | demorgan_guard | – | AST: demorgan_guard |
| 14 | `TestMesh__FP16RndTransformableP16RndTransformable` | bandobj/BandHeadShaper | 81.45 | 18.55 | positive_branch_invert | – | AST: positive_branch_invert |
| 15 | `BinkFileReadHeader__FP6BINKIOiPvUi` | utl/BinkIntegration | 81.71 | 18.29 | bitpack+positive_branch | – | AST: bitpack_or_reorder |
| 16 | `Poll__13CharForeTwistFv` | char/CharForeTwist | 81.82 | 18.18 | demorgan_guard | – | AST: demorgan_guard |
| 17 | `Highlight__8CharEyesFv` | char/CharEyes | 81.97 | 18.03 | store_then+positive | FMA×2 | fma_reorder + store_then |
| 18 | `reserve__Q211stlpmtx_std107_Vector_impl<Q210SongParser13GemInProgress...>` | beatmatch/SongParser | 82.18 | 17.82 | member_readback+store_then | – | AST: member_readback |
| 19 | `Multiply__FRC7Vector3RCQ23Hmx4QuatR7Vector3` | math/Rot | 82.45 | 17.55 | – | – | permuter-grind |
| 20 | `Interp__FRCQ23Hmx5ColorRCQ23Hmx5ColorfRQ23Hmx5Color` | rndobj/MatAnim | 82.62 | 17.38 | return_this_op_assign | – | AST: return_this_op_assign |
| 21 | `UtilDrawAxes__FRC9TransformfRCQ23Hmx5Color` | rndobj/Utl | 82.66 | 17.29 | switch_case_reorder | FMA×12 | fma_reorder + switch_case |
| 22 | `MultiplyEq__FP7BSPNodeRC9Transform` | math/Geo | 82.83 | 17.17 | – | – | permuter-grind |
| 23 | `__ls__16DebugNotifyOncerFPCc` | bandobj/VocalTrackDir | 82.85 | 17.15 | store_then+positive | – | AST: store_then_compound_add |
| 24 | `UsbReadInstrCallback__6UsbWiiFP9HIDDevicelPUcUlUl` | usbwii/UsbWii | 82.91 | 17.09 | bitpack+switch_case | – | AST: bitpack_or_reorder |
| 25 | `FrameToBeat__8CharClipCFf` | char/CharClip | 83.06 | 16.94 | switch_case_reorder | – | AST: switch_case_reorder |
| 26 | `__dt__12BudgetScreenFv` | BudgetScreen | 83.06 | 16.94 | – | – | permuter-grind |
| 27 | `BuildNGCone__9SpotlightFRQ29Spotlight7BeamDefi` | world/Spotlight | 83.62 | 16.38 | bitpack+demorgan | FMA×1 | fma_reorder |
| 28 | `Multiply__FRC3BoxfR3Box` | math/Geo | 83.69 | 16.31 | – | – | permuter-grind |
| 29 | `reserve__Q211stlpmtx_std97_Vector_impl<Q213DataEventList6CompEv...>` | midi/DataEventList | 83.74 | 16.26 | store_then+positive | – | AST: store_then_compound_add |
| 30 | `reserve__Q211stlpmtx_std87_Vector_impl<Q210MidiParser4Note...>` | midi/MidiParser | 83.74 | 16.26 | demorgan_guard | – | AST: demorgan_guard |
| 31 | `FinishWrite__13BufStreamNANDFv` | utl/BufStreamNAND | 83.75 | 16.25 | switch_case_reorder | – | AST: switch_case_reorder |
| 32 | `DrawMeterScale__5SynthFRf` | synth/Synth | 84.05 | 15.95 | positive_branch_invert | – | AST: positive_branch_invert |
| 33 | `LensSym_to_FOV__F6Symbol` | world/CameraShot | 84.20 | 15.80 | positive+demorgan | – | AST: positive_branch_invert |
| 34 | `reserve__Q211stlpmtx_std65_Vector_impl<7Vector3...>` | bandobj/NoteTube | 84.39 | 15.61 | demorgan_guard | – | AST: demorgan_guard |
| 35 | `Poll__17CharLipSyncDriverFv` | char/CharLipSyncDriver | 84.53 | 15.47 | – | – | permuter-grind |
| 36 | `CacheData__14BoxMapLightingFRQ214BoxMapLighting16LightParams_Spot` | rndobj/BoxMap | 84.54 | 15.46 | switch_case_reorder | – | AST: switch_case_reorder |
| 37 | `Relativize__16CharBonesSamplesFP8CharClip` | char/CharBonesSamples | 84.58 | 15.42 | makestring_wrap+positive | FMA×5 | fma_reorder |
| 38 | `reserve__Q211stlpmtx_std79_Vector_impl<Q27RndMesh4Face...>` | bandtrack/GemRepTemplate | 84.71 | 15.29 | switch_case_reorder | – | AST: switch_case_reorder |
| 39 | `IsValid__16StoreStringTableFi` | meta/StorePackedMetadata | 86.11 | 13.89 | member_readback+bitpack | – | AST: member_readback |
| 40 | `Poll__20StoreMetadataManagerFv` | meta/StorePackedMetadata | 87.38 | 12.62 | member_readback+bitpack | – | AST: member_readback |
| 41 | `ParseAndStripLyricText__10SongParserFPCcR9VocalNote` | beatmatch/SongParser | 90.54 | 9.46 | member_readback+store_then | – | AST: member_readback |
| 42 | `UpdateAnimation__16CalibrationPanelFv` | meta_band/CalibrationPanel | 99.88 | 0.12 | – | r29↔r30 fixable | Decl reorder |
| 43 | `PostLoad__11UIComponentFR9BinStream` | ui/UIComponent | 99.82 | 0.18 | – | r27↔r29 fixable | Decl reorder |
| 44 | `ListAnimGroups__13BandCharacterFi` | bandobj/BandCharacter | 99.66 | 0.34 | – | r28↔r29 fixable | Decl reorder |
| 45 | `UpdateOverlay__21SongSectionControllerFv` | bandobj/SongSectionController | 99.58 | 0.42 | – | r29↔r31 fixable | Decl reorder |
| 46 | `ImportSettingsFromFont__14UIFontImporterFP7RndFont` | ui/UIFontImporter | 99.54 | 0.46 | – | r28↔r29 fixable | Decl reorder |
| 47 | `UTF8FilterString__FPciPCcPCcc` | utl/UTF8 | 99.28 | 0.72 | – | r24↔r25 fixable | Decl reorder |
| 48 | `ApplyArrowStyle__13VocalTrackDirFPQ23Hmx6Object` | bandobj/VocalTrackDir | 99.21 | 0.79 | – | r28↔r29 fixable | Decl reorder |
| 49 | `Poll__15GemTrainerPanelFv` | game/GemTrainerPanel | 99.21 | 0.79 | – | r27↔r28 fixable | Decl reorder |
| 50 | `SetRandomSongs__12MusicLibraryFiRQ211SongSortMgr10SongFilter` | meta_band/MusicLibrary | 99.18 | 0.82 | – | r29↔r31 fixable | Decl reorder |

### 99%+ fixable regswap queue (RECLASSIFIED — IPA-locked, not quick wins)

**Wave E1 audit (2026-05-27)**: All 12 attempted. Permuter (declaration_reorder + statement_reorder + all patterns, 8-12 rounds) + manual edits → 0 wins. These are IPA-file-level callee-save allocation decisions that resist intra-function source rewrites. `SetRandomSongs` not found in target binary. Mark all 12 as at-limit.

These 12 functions were identified by regswap_classify as having fixable callee-saved register swaps. Original estimate: declaration reorder. Actual result: permuter-class IPA-locked.

| Symbol | Unit | Match% | Pair | Types |
|---|---|---|---|---|
| `UpdateAnimation__16CalibrationPanelFv` | meta_band/CalibrationPanel | 99.88 | r29↔r30 | global_addr×param_save |
| `PostLoad__11UIComponentFR9BinStream` | ui/UIComponent | 99.82 | r27↔r29 | member_load×const |
| `ListAnimGroups__13BandCharacterFi` | bandobj/BandCharacter | 99.66 | r28↔r29 | param_save×const |
| `UpdateOverlay__21SongSectionControllerFv` | bandobj/SongSectionController | 99.58 | r29↔r31 | global_addr×param_save |
| `ImportSettingsFromFont__14UIFontImporterFP7RndFont` | ui/UIFontImporter | 99.54 | r28↔r29 | member_load×const |
| `UTF8FilterString__FPciPCcPCcc` | utl/UTF8 | 99.28 | r24↔r25 | const×param_save |
| `ApplyArrowStyle__13VocalTrackDirFPQ23Hmx6Object` | bandobj/VocalTrackDir | 99.21 | r28↔r29 | member_load×const |
| `Poll__15GemTrainerPanelFv` | game/GemTrainerPanel | 99.21 | r27↔r28 | const×param_save |
| `SetRandomSongs__12MusicLibraryFiRQ211SongSortMgr10SongFilter` | meta_band/MusicLibrary | 99.18 | r29↔r31 | param_save×param_save |
| `OnCompareEdgeVerts__7RndMeshFPC9DataArray` | rndobj/Mesh | 99.05 | r16↔r17 | member_load×member_load |
| `AnalyzeTrackList__10SongParserFv` | beatmatch/SongParser | 99.05 | r17↔r18 | param_save×param_save |
| `IsReasonToAutoload__15SaveLoadManagerFv` | meta_band/SaveLoadManager | 99.03 | r30↔r31 | param_save×const |

## Unit hot spots

Top 10 units by `n_candidates × avg_headroom` — best targets for `batch_auto` sweeps.

| Rank | Unit | Cands | Avg headroom | Score | AST hits | Fixable RS | Top patterns |
|---|---|---|---|---|---|---|---|
| 1 | `system/math/Geo` | 12 | 10.38 | 124.6 | 9 | 0 | bool_materialize, cache_repeated_call |
| 2 | `system/meta/StorePackedMetadata` | 23 | 4.45 | 102.2 | 43 | 0 | bool_materialize, positive_branch_invert, switch_case_reorder |
| 3 | `system/movie/Movie` | 19 | 5.18 | 98.5 | 11 | 1 | bool_materialize, member_readback |
| 4 | `system/math/Rot` | 9 | 10.21 | 91.9 | 1 | 0 | bool_materialize |
| 5 | `system/bandobj/BandPatchMesh` | 16 | 5.66 | 90.5 | 13 | 0 | bool_materialize, cache_repeated_call |
| 6 | `band3/bandtrack/GemManager` | 11 | 8.03 | 88.4 | 37 | 1 | bool_materialize, symbol_str_compare, store_then_compound_add |
| 7 | `system/world/Spotlight` | 12 | 6.44 | 77.2 | 12 | 0 | bool_materialize, switch_case_reorder, bitpack_or_reorder |
| 8 | `system/rndobj/Utl` | 18 | 4.29 | 77.2 | 22 | 0 | cache_repeated_call, bool_materialize, symbol_str_compare |
| 9 | `system/beatmatch/SongParser` | 11 | 6.59 | 72.4 | 56 | 1 | bool_materialize, positive_branch_invert, switch_case_reorder |
| 10 | `system/utl/MemMgr` | 15 | 4.77 | 71.6 | 27 | 0 | bool_materialize, positive_branch_invert, makestring_wrap_literal |

## Suggested next dispatch

### Immediate: 99%+ regswap queue (manual, 5 minutes)
Try declaration reorders in these source files — each is 1–2 lines:
```bash
# Example workflow (do NOT touch src/ except in a worktree):
tools/setup-worktree.sh wave-e-rs99
# Then in worktree, edit each TU and check:
#   CalibrationPanel.cpp   → UpdateAnimation (r29↔r30, global_addr vs param_save)
#   UIComponent.cpp        → PostLoad (r27↔r29, member_load vs const)
#   BandCharacter.cpp      → ListAnimGroups (r28↔r29, param_save vs const)
#   SaveLoadManager.cpp    → IsReasonToAutoload (r30↔r31, param_save vs const)
```

### Parallel permuter sweeps (3 units, independent)

**Dispatch A — `system/math/Geo` (highest headroom, FMA signals)** — COMPLETE (Wave E2a, 2026-05-27)
```bash
tools/setup-worktree.sh wave-e-geo && \
  cd ../wt-wave-e-geo && \
  python3 -m scripts.permuter.batch_auto --target unit --unit 'system/math/Geo' --limit 90
```
Rationale: 12 candidates, avg 10% headroom, FMA fixable hits on `Intersect` and `MultiplyEq`. math/Geo is self-contained (no header hotspots).

**Outcome**: 3 wins / 14 processed. Wall-clock: ~2.8 min (170s). Committed: `831a57ed` in `wt-wave-e2a-geo`.
| Symbol | Before | After | Delta | Pattern |
|---|---|---|---|---|
| `BSPFace::Update` | 89.5% | 92.9% | +3.4pp | decl_reorder + varinline (FMA-adjacent) |
| `Multiply` (Box×f) | 83.7% | 84.7% | +1.0pp | asgn_swap + decl_reorder |
| `Clip` (Polygon/Ray) | 97.1% | 97.5% | +0.4pp | null_guard_insert |
| 11 remaining | — | — | 0 | FPR/FMA volatile-regswap class, permuter-ceiling |

**Dispatch B — `band3/bandtrack/GemManager` (37 AST hits, symbol_str_compare)**
```bash
tools/setup-worktree.sh wave-e-gem && \
  cd ../wt-wave-e-gem && \
  python3 -m scripts.permuter.batch_auto --target unit --unit 'band3/bandtrack/GemManager' --limit 90
```
Rationale: 37 AST hits including 6 `symbol_str_compare` (proven quick win from MEMORY: `.Str()` pattern fixes). `Hit__10GemManagerFfii` at 89.57% and `PartialHit__10GemManagerFfiUii` at 86.84% are structural targets.

**Results**: 0 wins, +0.00% total. Wall-clock: 135s. Worktree: `wt-wave-e2b-gem`.
- 8 candidates processed: IsSpotlightGem (98.1%), UpdateLeftyFlip (96.8%), DrawTrackMasks (96.3%), SetupGems (95.4%), AddChordBracket (91.2%), TrackDir::SmasherPlate (90.3%), Hit (89.6%), PartialHit (86.8%)
- All 8: callee-saved regswap diagnosis (fixability 0.18–0.53), all patterns skipped (symbol_str_compare not triggered in source), BUILD FAILs on compose patterns
- `symbol_str_compare` pattern not triggered by triage scan — AST hits were likely false positives or the candidates selected already had the `.Str()` fix applied (AddChordBracket was already improved by previous commit `426b91f3` to 90.2%)
- Structural targets Hit/PartialHit: volatile+callee regswaps, no fixable source shape found
- **Mark all 8 AT_LIMIT** — no permuter-accessible improvements remain

**Dispatch C — `system/beatmatch/SongParser` (56 AST hits, mixed patterns)** — DONE (wave-e2c, 2026-05-27)
```bash
tools/setup-worktree.sh wave-e-parser && \
  cd ../wt-wave-e-parser && \
  python3 -m scripts.permuter.batch_auto --target unit --unit 'system/beatmatch/SongParser' --limit 90
```
Rationale: 56 AST hits (most of any in-scope unit), 11 candidates, `switch_case_reorder` × 6 (strong track record from VocalPlayer). `HandleRGGemStart` at 80% has FMA signal from cached diffs. `AnalyzeTrackList` at 98.8% has a fixable r17↔r18 regswap.

**Results**: 2 wins, +1.12% total. Wall-clock: 158.5s.
- `HandleRGGemStop`: 94.1% → 95.0% (+0.89%) — decl reorder + nested-if restructure
- `ParseAndStripLyricText`: 90.5% → 90.8% (+0.23%) — decl reorder + loop condition expr
- `HandleRGGemStart` (80%): no change — volatile regswaps, BUILD FAILs on most patterns
- `AnalyzeTrackList` (99.0%): no change — noise-only diagnosis (r17↔r18 callee-saved but no fixable source shape found)
- Committed: `ec87d4ab` on `wt-wave-e2c-parser`

### Before dispatching: populate batch_pattern_scan cache
Run `run_objdiff` on top-30 candidates to populate `/tmp/claude/diff_*.json`, then re-run:
```bash
python3 scripts/analysis/batch_pattern_scan.py --min 80 --max 99.9 --limit 100 --json > /tmp/wave-e/asm_patterns_v2.json
```
This will catch `extrwi`/`cmp`-encoding hits that the initial pass missed.

## Skip / known AT_LIMIT

These candidates appeared in the top 50 but show structural blockers from function_health. Re-mark AT_LIMIT if permuter confirms:

| Symbol | Match% | Ceiling% | Reason |
|---|---|---|---|
| `UpdateScrolling__10VocalTrackFf` | 80.07 | 42.4 | 794 regswap + 454 insert/delete mismatches — massive structural diff; known VocalTrack complex |
| `DoFancyElbow__14BandIKEffectorFR7QuatXfmf` | 80.03 | 20.7 | Ceiling 20.7% — already below current match. Permuter cannot help; FMA signal but ceiling is inverted |
| `UsbReadInstrCallback__6UsbWiiFP9HIDDevicelPUcUlUl` | 76.07 | 48.2 | USB/Wii hardware driver — marginally in-scope but Wii-specific logic |
| `AddDevice__6UsbWiiFP9HIDDevice7UsbType` | 79.12 | 63.2 | Same — USB hardware layer |
| `Poll__13CharForeTwistFv` | 81.82 | 30.9 | 250 regswaps — FPR cascade from psq_/inline math; permuter-class |
| `Relativize__16CharBonesSamplesFP8CharClip` | 84.58 | 42.2 | 411 regswaps + FMA; ceiling 42.2% means most mismatches are structural — matches prior `CharBonesSamples` at-limit note in MEMORY |
| `Intersect__FRC7SegmentRC8TrianglebRf` | 80.23 | 39.7 | 50 regswaps + FMA×9 but ceiling 39.7%; likely Multiply/Dot inline-asm cascade (see `feedback_mesh_cpp_at_limit.md`) |
| `GrowToContain__6SphereFRC7Vector3f` | 81.18 | 35.6 | math/Geo — `psq_l`/`ps_*` inline FPR cascade; ceiling 35.6% |

## Wave F7 — `system/bandobj/BandPatchMesh` sweep (2026-05-28)

**Worktree**: `wt-wave-f7-patchmesh`
**Command**: `python3 -m scripts.permuter.batch_auto --target unit --unit 'system/bandobj/BandPatchMesh' --limit 90`
**Wall-clock**: 160.4s (~2.7 min)

**Results**: 0 wins / 14 processed. +0.00% total.

All 14 candidates exhausted with no improvements:
- Functions at 99%–100% (4): noise-only diagnosis — 0 diff_ops, only volatile FPR regswaps
- Functions at 95%–99% (6): all show volatile FPR + callee-saved GPR cascades (fixability 0.00–0.14); no applicable patterns; plateau at round 2/5
- `BandPatchMesh::FindXfm` (58.7%): 26 diff_ops, 53 GPR swaps, 49 clusters — structural diff; callee-saved cascade matches `feedback_mesh_cpp_at_limit.md` pattern exactly
- `BandPatchMesh::WorkVerts::ExtendTwin` (72.6%): 9 diff_ops, 32 GPR swaps — previously improved by commit `aa214985` to ceiling; no further movement
- `BandPatchMesh::WorkVerts::AddEdge` (76.4%): 12 diff_ops, 18 GPR swaps — fixability 0.14 but no source pattern resolves the callee-saved allocation

**Conclusion**: Unit confirmed permuter-class. The `Vec.h`/`Mtx.h`/`psq_` FPR cascade described in `feedback_mesh_cpp_at_limit.md` is the dominant blocker across the whole unit. All remaining sub-100% functions are IPA-locked at the TU level.

## Wave F6 — `system/movie/Movie` sweep (2026-05-28)

**Worktree**: `wt-wave-f6-movie`  
**Command**: `python3 -m scripts.permuter.batch_auto --target unit --unit 'system/movie/Movie' --limit 90`  
**Wall-clock**: 116.6s (~1.9 min)

**Results**: 0 wins / 25 processed. +0.00% total.

All 25 candidates exhausted with no improvements:
- Functions at 99%–100% (5): noise-only diagnosis — already matching within 1 instruction
- Functions at 94%–99% (13): all callee-saved or volatile regswaps (fixability 0.00–0.30); no pattern triggers in source shape; plateau at round 2/5 in every case
- `Movie::Impl::Poll` (85.9% → baseline 84.55%): 24 clusters, 20 GPR swaps, FPR volatile cascades — permuter-class structural diff
- `Movie::Impl::MovieLoader::StateName` (66.7%): 0 variants generated — switch statement shape, no applicable patterns
- `Movie::Impl::Begin` (88.3%): 12 clusters, 11 GPR swaps — no improvement despite 39-variant round

**Diagnosis**: Unit is permuter-ceiling across the board. Dominant blocker is volatile+callee regswap cascades in mid-sized Impl functions (BeginFrame, End, Begin, CheckOpen). `psq_l`/FPR FMA cascades appear in SetRect (5 lfd/psq_l diff_ops). No AST-pattern improvements materialized despite 11 AST hits — the relevant patterns were skipped as "not relevant" by the diagnosis engine for every function.

**All 19 remaining candidates marked AT_LIMIT.** Do not re-attack without new manual structural analysis.
