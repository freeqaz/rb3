# 2026-05-26 — REVIEW closure sweep

Closure pass over the 14 in-scope REVIEW candidates from the normalized-masking audit
(`docs/sessions/2026-05-26-objdiff-metric-audit.md`). For each function: fresh per-symbol
`objdiff-cli diff`, inspect the residual mismatches (now `match_type:diff_arg` in the JSON
schema), and classify.

## Summary

| Disposition  | Count |
|--------------|------:|
| FIXED        | 1 |
| AT-LIMIT     | 11 |
| OUT-OF-SCOPE | 2 |
| NEEDS-WORK   | 0 |
| **Total**    | **14** |

One real bug found and fixed: `SetlistSort::BuildSetlistTree` had a `(SongSortNode *)` cast
that silently selected the wrong virtual overload. Fixed → 100%. All other items are either
documented codegen-locked patterns or Wii-only out-of-scope code.

Two items the task brief asked to re-verify came back NOT 100%:
- `Enter__14BandStorePanelFv` — still 99.98%, single `lwz r3, 0x0/0x4, r3` virtual-base offset diff after `__dynamic_cast`. The wave-2 commit `bbdebf1a` lifted it from 91→98%; the residual is virtual-inheritance vbase-resolution codegen, not a logic bug.
- `HandleEventResponse__15SaveLoadManager` — still 99.78%, switch-table range mismatch (`cmplwi 0x5f` vs `cmplwi 0x61`) plus 6 string-pool offset deltas. Tried two source-shape variants (default/case reorder, separating case 0x66/0x67 bodies); both kept the wrong jump-table range or regressed badly.

## FIXED (1)

| Symbol | Unit | Before | After | Notes |
|---|---|---:|---:|---|
| `BuildSetlistTree__11SetlistSortF...` | `band3/meta_band/SongSort` | 99.9953% | **100.00%** | Removed bogus `(SongSortNode *)newSetlist` cast in `SongSort.cpp:233`. The cast caused MWCC to dispatch `NewShortcutNode(LeafSortNode*)` at slot 0xc4 instead of `NewShortcutNode(SetlistSortNode*)` at slot 0xf0. One-line edit. |

## AT-LIMIT (11)

| Symbol | Unit | Raw % | Reason |
|---|---|---:|---|
| `__ct__14BoxMapLightingFv` | `system/rndobj/BoxMap` | 98.33 | `psq_l`/`ps_*` SIMD inline-asm cascade in `ApplyLight`/Vec.h reaches up into the ctor's pool slot scheduling. Documented in `feedback_boxmap_psq_blocker`. |
| `NextName__FPCcP9ObjectDir` | `system/obj/Utl` | 97.97 | `_savegpr_N` / `_restgpr_N` frame-helper register-count difference. Pure callee-saved span regalloc. |
| `Mat__19QuestFilterProvider...` | `band3/tour/QuestFilterPanel` | 99.43 | Same `_savegpr_N` span difference. |
| `Text__12MusicLibrary...` | `band3/meta_band/MusicLibrary` | 99.52 | Same `_savegpr_N` span difference. |
| `SetVolume__7RndMeshFQ27RndMesh6Volume` | `system/rndobj/Mesh` | 99.96 | Target makes a stack copy of `Volume` POD before the call; we read direct from the ref arg. Pure scheduling/redundant-store choice. |
| `FillSetlistWithAccomplishmentSongs__19AccomplishmentPanelF6Symboli` | `band3/meta_band/AccomplishmentPanel` | 99.60 | TGT `bl GetAccomplishmentProgress` vs SRC `bl _outline_GetAccomplishmentProgress`. MWCC outlining-heuristic difference; unfixable from this TU. |
| `Enter__14BandStorePanelFv` | `band3/meta_band/BandStorePanel` | 99.98 | Single `lwz r3, 0x0, r3` (TGT) vs `lwz r3, 0x4, r3` (BASE) after `__dynamic_cast<LocalBandUser>`. `LocalBandUser : public virtual BandUser, public virtual LocalUser` — virtual-inheritance vbase resolution chose different sub-object indirection. Wave-2 already lifted 91→98% via dynamic_cast direction; residual is layout-level codegen. |
| `AddUpgradeData__14SongUpgradeMgr...` | `band3/meta_band/SongUpgradeMgr` | 99.57 | Already shadowed via local-const trick; residual `r4/r5` reg-swap cluster around idx ~99. |
| `HandleEventResponse__15SaveLoadManagerFP9LocalUseri` | `band3/meta_band/SaveLoadManager` | 99.78 | Switch range: TGT `cmplwi r0, 0x61` (table 6..0x67), BASE `cmplwi r0, 0x5f` (table 6..0x65). Cases 0x66/0x67 are stacked with `default:` which MWCC strips from the jump table. Splitting the body regresses to 92%. 6 of 10 remaining diffs are unrelated stringBase0 pool offset shifts. |
| `ContentDone__11BandSongMgrFv` | `band3/meta_band/BandSongMgr` | 99.78 | `std::pair<float,float>` temps occupy swapped stack slots. Pure regalloc. |
| `OnMsg__18ContentDeletePanelFRC23UITransitionCompleteMsg` | `band3/meta_band/ContentDeletePanel` | 99.90 | Misleading surface diff; after `Symbol::Symbol` ctor, `r3 == r1+0x8`. Stack-slot swap. |

## OUT-OF-SCOPE (2)

Per `feedback_scope_native_port`: Wii-specific code that gets replaced wholesale in the native port. Match-grinding here teaches nothing about porting.

| Symbol | Unit | Raw % | Reason |
|---|---|---:|---|
| `CreateVFCache__11CacheMgrWiiFv` | `system/utl/CacheMgr_Wii` | 99.21 | Wii NAND virtual-filesystem cache. |
| `UnmountAsync__11CacheMgrWiiF...` | `system/utl/CacheMgr_Wii` | 98.55 | Wii NAND unmount path. |

## NEEDS-WORK (0)

None. The three task-brief-flagged re-verifications resolved as follows:

- #9 BandStorePanel::Enter — confirmed wave-2 commit did land; residual is AT-LIMIT virtual-inheritance codegen, not a missing fix.
- #10 BuildSetlistTree — wave-2 was at 99.9953%, not 100%. The single remaining vtable-slot mismatch was a real bug (bogus `(SongSortNode *)` cast). Fixed in this pass.
- #12 HandleEventResponse — fix did not land successfully. Attempted two additional source shapes (default-after-cases reorder, separate case body); neither produced target's wider jump table. AT-LIMIT under MWCC's switch-tabling heuristics.

## Diff JSON artifacts

All fresh diffs saved to `/tmp/closure/0[1-9]_*.json` and `/tmp/closure/1[0-4]_*.json`.
Each was produced with:

```
bin/objdiff-cli diff -p . <SYMBOL> -u <UNIT> --include-instructions -f json -o /tmp/closure/NN_name.json
```

Note: the current JSON schema uses `match_type` (values `equal`, `diff_arg`, ...) on the
top-level instruction entry and `target`/`base` for the two sides, not the `diff_kind` /
`target_instruction` / `base_instruction` shape some older scripts expect.
