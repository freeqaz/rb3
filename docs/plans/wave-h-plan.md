# Wave H — Diagnosis divergence fix + cache warming + more sweeps

**Created**: 2026-05-28
**Origin**: Wave G surfaced a divergence between pattern_scan's scan-time diagnosis (used by `--require-asm-signal`) and hill_climber's runtime `relevant()`. G1 PropAnim agent: "the ASM signal scan's 4 hits appear to have been superseded by already-matching at 100% (Load, Handle, SyncProperty). The remaining sub-100% functions in PropAnim don't have the signal."

Also: 578 of the 674 tightened pattern_scan hits are `confidence=unknown` because they lack a cached `/tmp/claude/diff_*.json`. Warming the cache for those would unlock more high-confidence targets.

## H1 — Fix pattern_scan diagnosis lookup (Opus)

### Problem
`pattern_scan --require-asm-signal` reports a hit as `asm_signal_match` when the cached diff for the **enclosing TU's symbol** signals match. But the actual target of hill_climber is the **sub-100% function inside that file** — which may have a different diagnosis (or none).

In PropAnim's case, `pattern_scan` found switch_case_reorder hits in functions Load/Handle/SyncProperty (these are at 100%), passed the asm-signal filter via some lookup, and incorrectly reported them as actionable. The sub-100% functions (ForeachKeyframe, ValueFromIndex, etc.) had no such pattern hit but they're what hill_climber actually targets.

### Fix
Two-stage filter in `pattern_scan.py`:

1. **Enclosing-function filter** — already exists (`--incomplete-only` filters by file/function match%). VERIFY it works at function-granularity not file-granularity (likely the bug). If it's file-level, fix it to function-level using AST scope (every hit is inside a `function_definition` node — extract its mangled symbol).
2. **Asm-signal filter per AST hit's enclosing function** — for each AST hit, identify the *enclosing function symbol* (not just any symbol in the file). Look up THAT symbol's diagnosis. Apply `pattern.relevant(diagnosis)`.

### Implementation hints
- The AST hit already has a `function_name` field (verify in `pattern_scan.py:ScanHit`). Convert it to a mangled symbol either via:
  - The MWCC mangling rule (kebab → `Foo__7BarClassFv`-style). Risky.
  - OR look up the mangled symbol from `report.json` by matching `demangled_name` containing `function_name`. Easier; that's what we already partially do.
- Then key the diagnosis lookup by mangled-symbol → cached diff path.
- After fix, a hit on `PropAnim::Load__7PropAnimFR9DataArray` (100%) should be EXCLUDED even when `--require-asm-signal` is off (via `--incomplete-only` granularity fix). And hits on sub-100% PropAnim functions should be the only ones surfaced.

### Validation
Re-run on the Wave G in-scope set:
```bash
python3 -m scripts.permuter.pattern_scan \
  --patterns switch_case_reorder,cache_repeated_call,member_readback,positive_branch_invert,demorgan_guard,store_then_compound_add,bitpack_or_reorder,symbol_str_compare \
  --incomplete-only --min-pct 80 --max-pct 99.9 \
  --require-asm-signal --json > /tmp/wave-h/h1_validation.json
```
- Compare to /tmp/wave-g/tight_ast.json's 96 `asm_signal_match` count.
- Specifically PropAnim's 6 hits should drop to ≤1.
- Spot-check 3 newly-revealed targets that hill_climber.relevant() *agrees* are actionable.

### Acceptance
- Function-granularity filtering verified or fixed.
- Re-validation shows a smaller, more accurate `asm_signal_match` count.
- Outcome row in `docs/plans/permuter-mechanization-roadmap.md`.
- Brief design note appended to this doc.

## H2 — Cache warming via fresh objdiff (Sonnet)

### Goal
578 in-scope AST hits are `confidence=unknown` because there's no cached diff. Run `--fresh-objdiff` to populate `/tmp/claude/diff_*.json` for all functions in the in-scope 80-99.9% match band, then re-run the tightened pattern_scan.

### Method
1. Get the list of in-scope candidate symbols from `decomp.db` (NULL verdict, 80-99.9%).
2. For each, run `bin/objdiff-cli diff -u <unit> <symbol> --format json -o /tmp/claude/diff_<symbol>.json`. The exact path convention may already be encoded in pattern_scan's `_diff_filename_for_symbol`; reuse it.
3. Re-run `pattern_scan --require-asm-signal` and compare counts.
4. Time-budget: 30 min wall-clock; warm as many as possible in that window.

### Output
- New entries in `/tmp/claude/diff_*.json`.
- A summary report at `/tmp/wave-h/h2_cache_warm_summary.json`: how many fns warmed, how many had cached diffs already, runtime stats.
- Updated `tight_ast.json` at `/tmp/wave-h/tight_ast_v2.json` showing the new `by_confidence` counts.

### Acceptance
- ≥300 new cached diffs (out of 578 unknown).
- Updated tight_ast shows the new `asm_signal_match` count.
- Brief summary appended to roadmap doc.

## H3 — Sweep ChordShapeGenerator (Sonnet, parallel)
`system/bandobj/ChordShapeGenerator` — 3 asm-confirmed cache_repeated_call hits, 3 symbols, avg 94.1%, untouched.

```bash
cd /home/free/code/milohax/rb3 && tools/setup-worktree.sh wave-h3-chord
cd ../wt-wave-h3-chord
python3 -m scripts.permuter.batch_auto --target unit --unit 'system/bandobj/ChordShapeGenerator' --limit 90
```

30-min cap. Commit each win individually.

## H4 — Sweep CameraShot (Sonnet, parallel)
`system/world/CameraShot` — 3 asm-confirmed hits (demorgan_guard + cache_repeated_call×2), 2 symbols, avg 92.5%, untouched.

```bash
cd /home/free/code/milohax/rb3 && tools/setup-worktree.sh wave-h4-camera
cd ../wt-wave-h4-camera
python3 -m scripts.permuter.batch_auto --target unit --unit 'system/world/CameraShot' --limit 90
```

30-min cap. Commit each win individually.

## Concurrency notes
- H1 modifies `scripts/permuter/pattern_scan.py` only — safe parallel to H2/H3/H4.
- H2 reads decomp.db + writes to `/tmp/claude/diff_*.json` — safe parallel.
- H3/H4 in worktrees — safe parallel.

## H1 design notes (2026-05-28)

**Root cause** — `pattern_scan._load_match_info()` keyed `qualified_name -> (pct, symbol)` using last-write-wins. When a class has multiple overloads of the same method (e.g. `TokenRedemptionPanel::OnMsg(ButtonDownMsg)` at 100% and `TokenRedemptionPanel::OnMsg(RockCentralOpCompleteMsg)` at 99.5%), only ONE survived in the dict — whichever decomp.db emitted last. The asm-signal filter then loaded that single resolved symbol's cached diff, which could belong to the *wrong* overload (the AST hit's actual enclosing function).

Hill_climber's runtime `relevant(diagnosis)` already runs per-symbol so it correctly rejected these — the divergence was only at the scan stage.

**Fix** in `scripts/permuter/pattern_scan.py`:
- New `_load_match_info_multi()` returns `dict[qname, list[(pct, symbol, unit)]]` sorted by ascending match%.
- New `_resolve_hit_candidate(name, source_path, candidates)` filters to in-TU candidates (via `_unit_matches_source` stem compare), picks the lowest sub-100% match, and flags `ambiguous` when >1 sub-100% overload remains.
- `_scan_file()` accepts `match_info_multi` and resolves per AST-hit, attaching the new `ScanHit.ambiguous_overload` flag.
- `main()` short-circuits the asm-signal filter to `confidence=unknown` when `ambiguous_overload` is True — letting hill_climber's runtime per-symbol filter make the final call.
- Legacy `_load_match_info()` preserved as a thin wrapper.

**Validation numbers** (H2 cache-warming ran in parallel, so totals are not strictly comparable):

| | Before fix | After fix |
|---|---|---|
| total hits | 604 | 622 |
| asm_signal_match | 145 | 161 |
| unknown | 459 | 461 |
| excluded | 114 | 119 |
| ambiguous_overload flagged | (n/a) | 6 |

14 newly-surfaced sub-100% hits were previously masked by 100% siblings — including `TokenRedemptionPanel::OnMsg` x3, `CamShotCrowd::AddCrowdChars`, `RndParticleSys::InitParticle`, `RockCentral::OnMsg` x3, `PatchPanel::OnMsg`, `OutfitConfig::SetSkinTextures`, `EditSetlistPanel::OnMsg`, `CalibrationPanel::OnMsg`, `CheatsManager::OnMsg` x2, `SystemPreInit`, `GuitarController::OnMsg`, `PropSync`. 6 hits are now correctly flagged `ambiguous_overload=true` (multi-sub-100% overloads sharing qname: `RockCentral::OnMsg` x3, `PatchPanel::OnMsg`, `CharHair::Hookup`, `MeshAnim.cpp::Interp`) and demoted to `confidence=unknown`.

Spot-check via `bin/analyze-function`: `AddCrowdChars__12CamShotCrowdF...list<...>` confirmed at 97.0% with real diff (stack-offset 0x10↔0x18 shifts + replace/delete clusters) — pattern_scan now correctly attributes the hit to this overload, not its 100% sibling `AddCrowdChars__12CamShotCrowdFv`.

**Tests** — `scripts/permuter/tests/test_pattern_scan_asm_signal.py` (12 cases, all pass): single-candidate, sub-100/100% overload pair, multi-sub-100% ambiguity, cross-TU template-instantiation preference, fallback paths, `_unit_matches_source` stem normalization.

## Outcome log
| Wave | Item | Agent | Status | Outcome |
|---|---|---|---|---|
| H1 | Fix pattern_scan diagnosis lookup | Opus 4.7 | DONE | `pattern_scan.py` last-write-wins qname dict was masking sub-100% overloads behind their 100% siblings (e.g. `TokenRedemptionPanel::OnMsg`, `CamShotCrowd::AddCrowdChars`, `RndParticleSys::InitParticle`). New `_load_match_info_multi` + `_resolve_hit_candidate` pick the in-TU sub-100% overload; multi-sub-100% cases get `ambiguous_overload=true` → demoted to `confidence=unknown` (matches hill_climber's per-symbol behavior). 14 newly-surfaced sub-100% targets across `meta_band/`, `bandtrack/`, `world/`, `rndobj/`, `meta/`. 6 ambiguity flags. Regression test added. |
| H3 | ChordShapeGenerator permuter sweep | Sonnet | DONE | 0 wins / 7 candidates in 59s. 3 asm-confirmed targets (BuildSpan 90.4%, BuildEndCap 91.3%, ExtendProfile 91.9% AT_LIMIT) are FILTERED by batch_auto's `stlpmtx_std::` demangled-name exclusion — their ChordShapeGenerator:: methods take `stlpmtx_std::map` params. Remaining 7 are volatile_regswap-class (GPR/FPR scheduling) or psq_st/stfd paired-singles mismatches (InterpolateXfm). 1 error: GetCrossSection generator overlapping-edits crash. Note: the 3 "cache_repeated_call" scan hits were false positives from H1 pattern_scan regression — the functions never reached hill_climber. Unit is permuter-class at all remaining sub-100% targets. |
| H4 | CameraShot permuter sweep | Sonnet | DONE | 0 wins / 10 candidates in 117s. All sub-100% fns (LensSym_to_FOV 84.2%, Shake 90.2%, SetFrame 93.4%, BuildTransform 94.6%, LookAt 95.0%) are volatile_regswap-class (FPR/GPR scheduler conflicts) — not fixable from source. demorgan_guard pattern fires but regresses; cache_repeated_call INVALID at build. Unit is permuter-class at all remaining sub-100% targets. |
| I1 | MemMgr permuter sweep | Sonnet | DONE | 0 wins / 24 candidates in 46.6s. Most of the unit is already at 100% (20/24 noise-only at ~99.97%). The 4 remaining sub-100% fns — MemMgr::Poll (91.2%), MemMgr::Dump (91.4%), Heap::Init (90.1%), Heap::Truncate (89.7%) — are all volatile_regswap-class (GPR scheduling) with fixability score ≤0.05. The 3 asm-confirmed pattern_scan hits were false positives; no pattern variant improved over baseline. Unit is permuter-class at all remaining targets. |
| I3 | BandSongMgr permuter sweep | Sonnet | DONE | 1 win / 6 candidates in 182.5s. `AllowContentToBeAdded` 87.6%→92.9% (+5.32%) via `typewidth_0` (int→unsigned int count) + `boolret_0`. Remaining 5 fns are callee-saved-regswap-class (ContentDone 99.9%, IsSongUnplayable 99.6%) or have 12+ GPR swaps with low fixability score (RemoveOldestCachedContent 97.0%, ReadCachedMetadataFromStream 94.4%). No `kSongID_Invalid` constant issues encountered in these specific targets. |
