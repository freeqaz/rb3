# Permuter mechanization roadmap

**Created**: 2026-05-27
**Origin**: 4-agent audit of recent hand-applied LLM decomp commits — see audit summary below.
**Owner**: tracked here; sub-agents update task boxes as they finish.

## Audit one-paragraph summary

AST-pattern layer is at parity with DC3 (110 each). Where RB3 is bare is the
**scanner / orchestration layer** (`scripts/analysis/` = 6 files vs DC3's ~25),
plus ~10 mechanizable-but-still-hand-applied transformations in MEMORY that
have no `patterns/*.py`. The synthesis engine itself runs `constraint_solver`
only at round 1 — looping it through later rounds is the highest-impact single
engine change. Fastest wins are not new patterns — they're porting DC3
scanners so existing patterns get applied at every real candidate site.

Full audit lives in conversation history; condensed targets below.

---

## Wave A — Port DC3 scanners (Sonnet, parallel)

Mechanical work: copy DC3 file → swap `build/373307D9/` → `build/SZBE69_B8/`
→ verify it runs against current RB3 `report.json` → smoke-test output.

DC3 source dir: `/home/free/code/milohax/dc3-decomp/scripts/analysis/`
RB3 dest dir:  `/home/free/code/milohax/rb3/scripts/analysis/`

- [x] **A1** `function_health.py` — per-fn dossier fusing diagnosis + ceiling + pattern suggestions. Single entry point: "what should I try on this symbol?" *Medium effort* — depends on `scripts/permuter/diagnosis.py` (already in RB3) and other helper imports.
- [x] **A2a** `header_cluster.py` — clusters non-100% fns sharing match% across TUs → shared-header root causes.
- [x] **A2b** `mismatch_cluster.py` — buckets AT_LIMIT fns by prologue / insert-cluster / offset-shift signature (reads cached `/tmp/claude/diff_*.json`).
- [x] **A3a** `batch_pattern_scan.py` — instruction-pattern detector across match% bands (`extrwi`, `subic`, `clrlwi`, `cmp` encoding).
- [x] **A3b** `regswap_classify.py` — classifies callee-saved regswap pairs by var role; output feeds `declaration_reorder.py` directly. Re-enable `psq_*` opcodes (DC3 stripped them; RB3 needs them).
- [x] **A3c** `ceiling_calculator.py` — "how much further can this fn go" — prioritization input.
- [x] **A4** `reclassify_at_limit.py` — bulk-demotes stale AT_LIMIT back to workable when newer classifier flags fixability. *Medium effort* — needs `decomp.db` schema parity check.

**Acceptance**: each script runs end-to-end against RB3 with `--help` and a real query. No hard-coded DC3 paths remain.

## Wave B — New permuter patterns (Opus, parallel)

Each pattern is a new file in `scripts/permuter/patterns/`; auto-registered via
`Pattern.__init_subclass__`. Canonical shape: `member_readback.py` (~250
lines). Each agent: write file, run `python -m scripts.permuter.pattern_scan
--patterns <name>` to validate detection, dispatch single hill-climb to confirm
a real win, document in `docs/decomp/patterns/`.

- [x] **B1** `return_this_op_assign.py` — ref-returning `operator=` missing `return *this;` → append it. **6 confirmed candidate sites**: Gem.cpp, MatAnim.cpp, Mesh.cpp, DateTime.cpp, Time.cpp ×2. *Smallest possible new pattern — canonical example for the next ones.*
- [x] **B2** `makestring_wrap_literal.py` — `TheDebug << "literal"` → `TheDebug << MakeString("literal")` (puts ~0x82C FormatString in caller frame). ~34 sites. Needs asm signal (frame size short by ~0x82C) to gate.
- [x] **B3a** `lwzu_idiom.py` — `*(uint*)p; p+=4;` → `*((uint*&)p)++` for {int,short,char}. Trivial AST; rare but unambiguous asm signal (`lwzu` in target).
- [x] **B3b** `pragma_pool_data_wrap.py` — wrap fn in `#pragma pool_data off`/`reset` when asm shows callee-saved BSS-base reg + `addi rX, rBASE` cluster. Pure source wrap; ~14 in-tree, broader asm-signal candidate pool.
- [x] **B4** `inline_lerp_collapse.py` — collapse N parallel `dN = e.fN - tmp; rN = f*dN + tmp; m.fN = rN;` triples into inline `m.fN = f*(e.fN - tmp) + tmp;` per field. ~10s of leaf animate fns. Proved big (83.8→100%).
- [x] **B5** `switch_case_reorder.py` — reorder `case` clauses in source to match target jump-table body emission order. *Hardest* — needs target `.s` jump-table parse. Big-yield (+24pp wins) but tooling-heavy.

**Acceptance**: each pattern file imports cleanly; `pattern_scan` finds ≥1 hit
in tree; doc page added under `docs/decomp/patterns/`.

## Wave C — Engine improvements (Opus, sequential after A+B)

- [x] **C1** Loop `constraint_solver` into rounds 2+ of `hill_climber.py`. Currently round-1 only (`hill_climber.py:914-918`); after that, blind pattern search resumes. Re-firing the deterministic Ghidra/m2c-oracle synthesis after each round lets new asm state inform later constraint resolution. **Done** — design A (run every round + cross-round source-bytes dedup). See `docs/plans/constraint-solver-loop.md`. Escape hatch: `--no-loop-constraints` / `PERMUTER_LOOP_CONSTRAINTS=0`.
- [ ] **C2** *(stretch)* Add stack-slot oracle to `constraint_solver`. MWCC DWARF recompile already gives slot names (`scripts/analysis/dwarf_locals.py`); not wired into the solver yet.

## Wave D — Apply at scale (mixed, after A+B)

With scanners + new patterns live, run sweeps to actually move match%.

- [~] **D1** Run `function_health.py --top 50` to surface workable functions; dispatch a fleet of permuter hill-climbs (`batch_auto`) on the top picks. *Partial: scanners ran, 3 permuter hill-climbs executed (TransAnim 97.1%+0, TaskMgr 97.6%+0, SaveLoadManager 95.6% TBD); no wins from permuter this wave. function_health `--top N` currently returns AT_LIMIT entries from decomp.db — see infra issue in wave-d-followup.md.*
- [~] **D2** Run `mismatch_cluster.py --mode all` to surface header-cluster signatures; identify shared-header fixes that would unlock 10s-100s of functions. *Partial: clusters captured. Top-5 root causes documented in wave-d-followup.md. #1 cluster (178 fns@99.9% across 106 TUs) is the headliner — root cause not yet identified. See action item #1 in followup doc.*
- [ ] **D3** Run `batch_pattern_scan.py --pattern extrwi --min 90 --max 99.9`; apply the suggested patterns in bulk via `scan_and_permute.py`.
- [~] **D4** Run new `B1`-`B4` patterns repo-wide via `pattern_scan`; commit each match% win individually. *Partial: return_this_op_assign confirmed +2.9pp on RndMatAnim::TexKeys::operator= (commit d15f0f79 in wt-wave-d-apply). switch_case_reorder detected 5 fns in SaveLoadManager.cpp but permuter not yet applied. makestring_wrap_literal: 10 fns detected, asm gate not firing (wrong delta). lwzu/inline_lerp: 0 in-tree hits.*

## Wave E — Ranked target scan (Sonnet 4.6, 2026-05-27)

Applied `reclassify_at_limit.py --apply`, which reopened 1,035 functions (NULL verdict, 80–99.9%,
non-Wii-specific units). Four scanners run:

- **pattern_scan (AST)**: 5,025 total hits → 4,153 in-scope after filtering sdk/network/rndwii/os/synthwii/lib.
  Top patterns: `bool_materialize` (2,459), `symbol_str_compare` (481), `cache_repeated_call` (405),
  `switch_case_reorder` (301).
- **batch_pattern_scan (asm encoding)**: 0 hits — expected, as fresh-reopened fns have no cached
  `/tmp/claude/diff_*.json`. Re-run after populating cache via `run_objdiff`.
- **regswap_classify**: 264 functions with CS swap; 37 matched to candidates; 12 are fixable
  `param_save`/`member_load` pairs at ≥99% match — highest-ROI quick edits.
- **function_health**: 30 sampled; all `at_limit` verdict; 6 have FMA fixable category (fma_reorder
  pattern applicable).

Full ranked report → **[docs/plans/wave-e-targets.md](wave-e-targets.md)**

Top 3 unit dispatch targets: `system/math/Geo` (score 124.6, FMA signals), `band3/bandtrack/GemManager`
(score 88.4, 37 AST hits, symbol_str_compare wins), `system/beatmatch/SongParser` (score 72.4, 56 AST
hits, switch_case_reorder × 6). 12 functions ≥99% fixable via declaration reorder (regswap queue).

---

## Concurrency rules for sub-agents

1. **Don't touch unrelated `src/` files** — there is in-progress work (see `git status`). Wave A and B only modify `scripts/analysis/` and `scripts/permuter/patterns/` and possibly `docs/decomp/patterns/`.
2. **Build via `tools/ninja-locked`**, never bare `ninja` (concurrent ninja corrupts `.ninja_log`).
3. **No `git stash`** in the main repo.
4. **No commits** unless explicitly told — leave each wave's work staged or as new files; the operator commits batches.
5. If your task touches `src/` for verification (running a pattern against a real function), do it in a **worktree** via `tools/setup-worktree.sh <name>`.
6. Each finishing agent **edits this doc** to tick its box and adds a 1-2 line outcome note.

## Outcome log

(Sub-agents append below as they finish.)

### Wave J (2026-05-28) — tooling correctness + targeted sweeps

Coordinator dispatched 4 tooling fixes + 4 sweeps. **The scanner had silently regressed to fully blind** (`--require-asm-signal` returned 0 `asm_signal_match`, all 1971 hits `unknown`) — J1 fixed it (root cause: `_load_match_info_multi` raised on the 5,239 NULL-`demangled` rows now in the DB and swallowed it into an empty dict; restored to **158 asm_signal_match**). J2 fixed a data-integrity bug (`ingest_report` NULL'd scores for the 7,184 unmeasurable functions on `--db-sync`; 2 live rows incl. a COMPLETE one would have been wiped). J3 fixed the STL-param over-exclusion (substring→scope-prefix; unblocked ChordShapeGenerator BuildSpan/BuildEndCap). J4 fixed `variable_inline` same-line decl+use crash. Sweeps: J7 BandWardrobe **2 wins** (FindBestScoringHint 96.0→97.0%, LoadMainCharacters 94.2→94.6%, committed `26686b8d`); J5/J6/J8 0 wins (permuter-class IPA/FPR cascades). BuildSpan/BuildEndCap marked AT_LIMIT (J8 confirmed). **Lesson reinforced: tooling correctness has the highest leverage — the scanner was blind and would have wasted every targeted sweep.** Tooling fixes COMMITTED (operator approved cross-repo): rb3 `4cd8e9bd` (J2 database.py), dc3-decomp `ab0d707d` (J1/J3/J4 + tests, 35 cases green).

### Wave K (2026-05-28) — sweeps on trustworthy (repaired-scanner) targets

With the scanner fixed (158 `asm_signal_match`), dispatched 4 Sonnet sweeps on untouched sub-97% structural-signal units. **2 wins**: K1 Color MakeColor 85.1→86.7% (switch-case reverse + decl hoist) + MakeHSL 93.6→93.8% (commute Max args, flip equality) — committed `b8c6ef03`. K2 Trans, K3 VocalPart, K4 CharSleeve = 0 wins (volatile/callee-saved FPR cascades). K4 confirmed the whole CharTwist/CharIK family is permuter-class (Vec.h/Mtx.h inline-asm FPR scheduling — same blocker as Mesh.cpp/CharIKFingers). Marked 8 fns AT_LIMIT (VocalPart::GetNoteSliceWeight, CharCuff::DeformMesh, CharForeTwist/CharIKHead/CharSleeve/CharUpperTwist::Poll, CharServoBone::Regulate, RndTransformable::ApplyDynamicConstraint) + ChordShapeGenerator BuildSpan/BuildEndCap. AT_LIMIT 216→226, workable 1195→1185. **New finding: store_then_compound_add / demorgan_guard / cache_repeated_call fired as `asm_signal_match` but produced 0 wins on VocalPart/CharSleeve — their `relevant()` gates are too loose (an opcode appears but the real mismatch is an FPR cascade). Next-highest-leverage tooling task: tighten those gates to cut false positives.**

### Wave L (2026-05-28) — gate tightening + final structural sweep

L1 (Opus): added a principled FPR-cascade veto to `store_then_compound_add`/`demorgan_guard`/`cache_repeated_call` `relevant()` gates — new `classifier.is_fpr_cascade_dominated()` counts multi-instruction FPR↔FPR swap pairs, vetoes above threshold 10. Deliberately NOT a fixability/ratio gate (that would have killed the MakeColor win, which has lower fixability than the vetoed cases). `asm_signal_match` 104→78 (−26 false positives); GetNoteSliceWeight + CharSleeve::Poll now correctly excluded; MakeColor + FindBestScoringHint preserved. +11 tests. Committed dc3-decomp `bd5aca35`. L2 (Sonnet): MeshDeform **2 wins** — Print 97.7→98.5%, Reskin 85.7→86.9% (unsigned→int loop index + decl reorder); committed `501f2c29`. **Session J+K+L total: 6 decomp wins committed (BandWardrobe ×2 `26686b8d`, Color ×2 `b8c6ef03`, MeshDeform ×2 `501f2c29`), 5 tooling bugs fixed across 3 commits (rb3 `4cd8e9bd`; dc3 `ab0d707d`, `bd5aca35`), 10 fns AT_LIMIT'd, ~57 new regression tests. Scanner went from fully blind → 78 high-precision asm_signal_match.**

| Wave | Item | Agent | Status | Notes |
|---|---|---|---|---|
| A | A1 | function_health.py | DONE | Ported DC3→RB3; fixed `--symbol` flag→positional arg, replaced decomp.db query with report.json fallback (db empty), swapped 373307D9→SZBE69_B8; `--symbol __ct__3AppFiPPc` produces full mismatch breakdown + pattern suggestions |
| A3 | A3a batch_pattern_scan.py | Sonnet 4.6 | DONE | Ported DC3→RB3; swapped build path to SZBE69_B8; smoke-tested: loads report.json, diffs 5 fns, exits 0 |
| A3 | A3b regswap_classify.py | Sonnet 4.6 | DONE | Ported DC3→RB3; added psq_l/psq_st/ps_* to WRITE_POS0_OPS + classify_def; confirmed psq_st appears in output against 598 cached diffs |
| A3 | A3c ceiling_calculator.py | Sonnet 4.6 | DONE | Ported DC3→RB3; removed `excluded` column refs (not in RB3 schema); analyzed 5 AT_LIMIT fns, shows effective completion 77.1% |
| A2 | A2a header_cluster.py | Sonnet 4.6 | DONE | Ported DC3→RB3; swapped 373307D9→SZBE69_B8 default; loads 2128 non-complete fns, top opportunity: 177 fns@99.9% across 106 TUs |
| A2 | A2b mismatch_cluster.py | Sonnet 4.6 | DONE | No DC3-specific paths (reads /tmp/claude/diff_*.json); copied verbatim; loads 590 cached diffs, prologue delta -2 cluster = 4 fns |
| B | B2 makestring_wrap_literal.py | Opus 4.7 | DONE | New pattern. Gates on ~0x800-0x880 frame-size delta on stwu/addi/subi r1 (+ AST pre-check for `TheDebug << "lit"`). pattern_scan finds 10 functions / ~50 sites across rndobj/{Tex,Line,MeshDeform,Overlay}, char/CharBonesSamples, utl/{Loader,MemMgr}, band3/{VocalTrack,StoreInfoPanel}. Doc: docs/decomp/patterns/makestring-wrap-literal.md |
| A4 | A4 reclassify_at_limit.py | Sonnet 4.6 | DONE | Schema parity: RB3 missing verdict_reason col (gracefully skipped); scripts/permuter symlink→DC3 required overriding build_object/run_objdiff/load_unit_source_map locally; dry-run: 551 AT_LIMIT fns, 20 sampled at 90-99.9% → 20 REOPEN (80% MIXED, 20% REGSWAP_PLUS), 0 KEEP |
| B | B1 return_this_op_assign.py | Opus 4.7 | DONE | New pattern. AST gate: ref-return + name ends in `operator=` + body has zero `return_statement` descendants. Asm gate liberal (mr/cmp on r3/r4 OR any reg-swap). pattern_scan finds 5 of the 6 expected sites (Gem.cpp correctly skipped — it returns `(Gem &)g`); also correctly skips Quazal::String::operator= overloads that have if/else returns. Handles both multi-line and one-line bodies. Doc: docs/decomp/patterns/return-this-op-assign.md |
| B | B4 inline_lerp_collapse.py | Opus 4.7 (1M) | DONE | New pattern. AST triple-match: `T d=expr;` + `T r=f*d+addend;` + `lvalue=r;` with d/r local to the triple. Groups contiguous triples sharing factor; emits full + partial-collapse variants. Asm gate: fmuls/fadds/fmadds/fsubs/fmsubs diff_ops, callee-saved FPR swaps, or `fpr_save_delta != 0`. pattern_scan: 0 in-tree hits (both historical wins — LightPreset SpotlightDrawerEntry::Animate 83.8→100%, AnimateSpotlightDrawerFromPreset 95.2→98.8% — were applied in `e1aea8bf`); validated on synthetic candidate that mirrors pre-fix shape (correctly emits 4-triple and 3-triple partial variants). Doc: docs/decomp/patterns/inline-lerp-collapse.md |
| B | B3a lwzu_idiom.py | Opus 4.7 (1M) | DONE | New pattern. AST: two adjacent stmts, `<lhs>? = *(unsigned <T>*)p;` (assignment / declaration / bare expr) + `p += K;` where K matches `sizeof(unsigned <T>)` (4/2/1). Emits `*((unsigned <T> *&)p)++`. Asm gate: any diff_op with `lwzu`/`lhzu`/`lbzu` on either side (priority 0.8). Synth.cpp / usbmic.c already use the idiom, so in-tree `pattern_scan` finds 0 hits today; synthetic 4-fn smoke test fires on 3 candidates (int/short/declaration forms) and correctly skips the mismatched-size case. Doc: docs/decomp/patterns/lwzu-idiom.md |
| B | B3b pragma_pool_data_wrap.py | Opus 4.7 (1M) | DONE | New pattern, `opt_in = True` (surgical fix; off by default). AST: function definitions whose preceding 4 KB does not already contain `#pragma pool_data {off|on|reset}`. Emits TWO variants per fn: `off`/`reset` wrap (the CacheWii::WriteAsync polarity) and `on`/`reset` wrap (the CustomizePanel::RotatePatch inverse-polarity case). Asm gate: strong on prologue mismatch with negative `gpr_save_delta`, weak on any addi/addis/lis diff_op or any cluster. `pattern_scan --source src/system/world/CameraShot.cpp` finds 68 functions; `Cache_Wii.cpp` 16; functions inside an existing pragma block are correctly skipped (4 KB look-back). Doc: docs/decomp/patterns/pragma-pool-data-wrap.md |
| B | B5 switch_case_reorder.py | Opus 4.7 (1M) | DONE | New pattern. **Phase 1** (always): for switches with >=3 hard-terminated case groups, emits up to 6 permutation variants (reverse + adjacent swaps + 2 random, seeded). **Phase 2** (when `build/<id>/asm/<unit>.s` is available): scans the function body for `"@NNNNN"@ha` jump-table refs, parses the matching `.obj` block's `.rel ..., .L_<hex>` entries, detects the default label as the most-common destination, sorts source case groups by ascending body address → emits one asm-guided variant. Safety: refuses fall-through cases; refuses cross-case `goto`s (correctly skips `StreamReceiver::Play`). `pattern_scan --source src/band3/meta_band/SaveLoadManager.cpp` finds 5 functions (Poll: 30-case switch, SetState: 6-case + nested 81-case, GetDialogOpt1: 11-case, GetDialogOpt2: 7-case, SaveLoadErrorSetState: 4-case). Phase 2 cleanly bails on symbolic-enum cases (e.g. `kS_Start`) — Phase 1 still fires. Synthetic JT test confirms Phase 2 reorders a numeric switch to match target. Doc: docs/decomp/patterns/switch-case-reorder.md |
| D | D1-D4 | Sonnet 4.6 | PARTIAL | All 4 scanners run; results in docs/plans/wave-d-followup.md. Win: `RndMatAnim::TexKeys::operator=` 96.5→99.4% via return_this_op_assign (commit d15f0f79 in wt-wave-d-apply). 3 permuter runs (TransAnim 97s, TaskMgr 65s, SaveLoadMgr 357s) — 0 wins, 310-503 variants tried per fn. Infra: worktree missing bin/objdiff-cli symlink (fixed manually via ln -s; should go in setup-worktree.sh). function_health --top picks AT_LIMIT entries (score=0) — needs workability_score > 0 filter. reclassify_at_limit: 466 REOPEN (dry-run only). |
| C | C1 loop constraint_solver | Opus 4.7 (1M) | DONE | hill_climber.py: replaced `round_num == 1` synth gate with `(loop_constraints or round_num == 1)`; added `synth_seen_sources: set[bytes]` for cross-round source-md5 dedup; `skip_reason` still fatal only on round 1; new param `loop_constraints=True` + CLI `--loop-constraints`/`--no-loop-constraints` + env `PERMUTER_LOOP_CONSTRAINTS=0`. 7 new tests in `tests/test_loop_constraints.py` (API surface, every-round firing, dup-skip, env override) — all green. Infra fix: added `tools/compiler_trace` symlink to dc3-decomp (reduced rb3 permuter test failures 77→18; remaining 18 are pre-existing test-code drift, 15 of which fail identically in dc3). Design doc: `docs/plans/constraint-solver-loop.md`. |
| E | Wave E target scan | Sonnet 4.6 | DONE | 1,035 reopened fns scanned. 4,153 in-scope AST hits; batch_pattern_scan 0 (no cached diffs for fresh-reopened fns — re-run after cache warm); regswap_classify 37 matches, 12 fixable at ≥99%; function_health 30 sampled, 6 FMA-fixable. Top units: Geo (124.6), GemManager (88.4), SongParser (72.4). Report: docs/plans/wave-e-targets.md |
| E1 | 99%+ regswap queue manual sweep | Sonnet 4.6 | DONE/0 wins | Attempted all 12 targets in wt-wave-e1-regswap. `SetRandomSongs` not found in target binary (skip). Remaining 11: permuter (declaration_reorder + statement_reorder + all patterns, 8-12 rounds each) returned 0 improvement on all. Manual edits (IsReasonToAutoload early-return hoist, UIComponent gNullStr cache) regressed or had no effect. Root cause: regswaps are IPA-file-level prologue decisions — MWCC assigns callee-saves based on whole-TU live-range analysis, not declaration order within the function. The `bool false + member load` prologue swap pattern (ApplyArrowStyle, ImportSettingsFromFont, PostLoad) and the `two-variable in different branches` pattern (CalibrationPanel, ListAnimGroups) both resist source-level rewrites. **Updated classification: these 12 are permuter-class IPA-locked, not quick wins.** |
| F4 | AT_LIMIT bulk-mark (Wave E confirmed) | Sonnet 4.6 | DONE | Marked 19 functions AT_LIMIT in decomp.db (85→104). 11 from Wave E1 (IPA-locked callee-saved regswaps: CalibrationPanel, UIComponent, BandCharacter, SongSectionController, UIFontImporter, UTF8, VocalTrackDir, GemTrainerPanel, RndMesh, SongParser, SaveLoadManager). 8 from Wave E2b GemManager (callee-saved regswaps 86-98%: IsSpotlightGem, UpdateLeftyFlip, DrawTrackMasks, SetupGems, AddChordBracket@91.2%, SmasherPlate, Hit, PartialHit). `SetRandomSongs` not in binary — skipped. AddChordBracket improved to 91.2% but still permuter-class; marked. Backup: decomp.db.wave-f4-backup. |
| F5 | `system/meta/StorePackedMetadata` sweep | Sonnet 4.6 | DONE/0 wins | 45 candidates (52–100%), 292s (4.87 min). 0 improvements. Top 3 diagnosed: ~StoreMetadataManager (80.7%) — 4 callee-saved regswaps (r28↔r30, r3↔r30, r30↔r31), fixability 0.19, only 7 variants generated, all failed/neutral; IsValid (86.1%) — volatile regswaps r0↔r3/r4, fixability 0.00, only 2 variants, both FACT_AGREED; Poll (87.4%) — single extrwi/rlwinm. diff_op, 0 regswaps, 6 variants all neutral/regressing. Functions at ≥96%: all noise_only. Functions at <80% (EndianFix family 52–73%): all volatile regswap / structural lbz↔lhz mismatches. Unit is permuter-class — structural struct-layout mismatches (EndianFix) + IPA callee-saved regswaps (manager fns). No commits made in wt-wave-f5-store. |
| F3 | BUILD FAILED variant investigation (HandleRGGemStart) | Opus 4.7 (1M) | DONE | Fixed two patterns emitting invalid C++ when libclang compdb is unavailable. (1) `variable_extraction.py` emitted `int _tmp = RGGemInfo(...)` (record-to-int) for `info.mRGGemsInfo[uc-24] = RGGemInfo(...)` — added `_is_assignment_rhs_to_complex_lvalue` + `_syntactic_record_return` guard (PascalCase callee w/o Get/Find/Is/Has prefix). (2) `bool_cast.py` Pattern 3 wrapped any assignment RHS in `bool(...)` regardless of LHS type — now requires LHS to be a plain identifier (`_bool_assignable_lvalue`). BUILD FAILED variants on the trigger symbol: **2 → 0**. Regression tests: `test_variable_extraction_record_guard.py` (8 tests) + `test_bool_cast_lvalue_guard.py` (7 tests), all passing. Root cause: `clang_types.is_available()` only checks `clang.cindex` import, not compdb presence — existing typed-resolution guards never fired in worktrees. Findings appended to `docs/plans/build-failed-investigation.md`. |
| F2 | Scanner tightening (IPA penalty + asm-signal gating) | Opus 4.7 (1M) | DONE | `regswap_classify.py`: new `ipa_penalty_class` field + `--ipa-aware`/`--no-ipa-aware`/`--show-ipa-penalty`/`--json`. Resolves diff-JSON `unit` (e.g. `meta_band/CalibrationPanel`) to objdiff canonical (`main/band3/meta_band/CalibrationPanel`) via suffix match. **Wave E1's 12 regswap candidates: `fixable=True` → `False` after IPA penalty** (all `ipa_locked`/`ipa_partial`). `pattern_scan.py`: new `--require-asm-signal`/`--include-unmatched-asm`/`--fresh-objdiff`/`--diff-cache-dir`; per-hit `confidence` field (`ast_only`/`asm_signal_match`/`unknown`/`excluded`). Fixed pre-existing `_load_match_info` schema bug (column is `current_percent`, not `match_percent`). **Wave E AST hit count: 4,157 → 4,040 with `--require-asm-signal`** (117 excluded by asm signal mismatch, 254 confirmed `asm_signal_match`). All 36 cached `symbol_str_compare` hits excluded — confirms GemManager false-positive class. Docs: `docs/decomp/patterns/scanner-confidence.md`. |
| H2 | Cache warming (objdiff diff JSON population) | Sonnet 4.6 | DONE | 1,015 in-scope NULL-verdict 80-99.9% candidates; 369 already cached; **646 newly warmed** (0 failures) in 9.9s via 4-worker parallel objdiff. Cache grew from 825→1,471 files. Re-ran tightened pattern_scan (12 patterns): `asm_signal_match` 96→**155** (+59), `unknown` 578→**453** (-125), `excluded` 53→**118** (+65). Top newly-revealed units: StorePackedMetadata (6), PropAnim (6), PropKeys (5), VocalTrack (4), GemPlayer (4), SaveLoadManager (4), TokenRedemptionPanel (4), SongParser (4), MemMgr (3), ChordShapeGenerator (3). Summary: `/tmp/wave-h/h2_cache_warm_summary.json`. Updated scan: `/tmp/wave-h/tight_ast_v2.json`. |
| H1 | Fix pattern_scan diagnosis lookup (overload disambiguation) | Opus 4.7 (1M) | DONE | `scripts/permuter/pattern_scan.py` last-write-wins qname dict was masking sub-100% overloads behind their 100% siblings; asm-signal filter then loaded the wrong overload's cached diff. New `_load_match_info_multi` returns `dict[qname, list[(pct,sym,unit)]]`; new `_resolve_hit_candidate` filters to in-TU candidates (via `_unit_matches_source` stem compare), picks the lowest sub-100%, flags `ambiguous_overload` when >1 sub-100% remains. asm-signal filter now demotes ambiguous hits to `confidence=unknown`, deferring to hill_climber's runtime per-symbol relevant() check. **14 newly-surfaced sub-100% hits** (TokenRedemptionPanel::OnMsg x3, CamShotCrowd::AddCrowdChars, RndParticleSys::InitParticle, RockCentral::OnMsg x3, PatchPanel::OnMsg, OutfitConfig::SetSkinTextures, EditSetlistPanel::OnMsg, CalibrationPanel::OnMsg, CheatsManager::OnMsg x2, SystemPreInit, GuitarController::OnMsg, PropSync) were previously masked. **6 hits flagged ambiguous** (RockCentral::OnMsg x3, PatchPanel::OnMsg, CharHair::Hookup, MeshAnim.cpp::Interp). Spot-check: AddCrowdChars 97.0% overload has real diff (stack offset 0x10↔0x18 + replace/delete cluster); old code was returning the 100% sibling's diff. Regression test: `scripts/permuter/tests/test_pattern_scan_asm_signal.py` (12 cases). Validation: `/tmp/wave-h/h1_validation.json`. Design notes in `docs/plans/wave-h-plan.md`. |
| I5 | AT_LIMIT bulk-mark (Waves F5/F6/F7/G1/H3/H4 confirmed) | Sonnet 4.6 | DONE | Marked 112 functions AT_LIMIT in decomp.db (104→216). Backup: decomp.db.wave-i5-backup. Per-unit: Movie/Movie=25 (volatile+callee-saved cascades), StorePackedMetadata=34 (IPA manager fns ≥95% only; excluded EndianFix family 52–73% and sub-95% manager fns), BandPatchMesh=27 (Vec.h/Mtx.h/psq_* cascades), PropAnim=5 specific fns (ForeachKeyframe/ValueFromIndex/LoadPre7/AdvanceFrame/SetKeyVal), ChordShapeGenerator=11 new (excluded BuildSpan 90.4%/BuildEndCap 91.3% for future re-test; ExtendProfile pre-existing AT_LIMIT), CameraShot=14 new (volatile+callee-saved; 1 pre-existing). Sanity spot-check: LensSym_to_FOV (84.2%) confirmed AT_LIMIT. |
