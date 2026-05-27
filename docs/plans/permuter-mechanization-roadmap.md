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
- [ ] **B5** `switch_case_reorder.py` — reorder `case` clauses in source to match target jump-table body emission order. *Hardest* — needs target `.s` jump-table parse. Big-yield (+24pp wins) but tooling-heavy.

**Acceptance**: each pattern file imports cleanly; `pattern_scan` finds ≥1 hit
in tree; doc page added under `docs/decomp/patterns/`.

## Wave C — Engine improvements (Opus, sequential after A+B)

- [ ] **C1** Loop `constraint_solver` into rounds 2+ of `hill_climber.py`. Currently round-1 only (`hill_climber.py:914-918`); after that, blind pattern search resumes. Re-firing the deterministic Ghidra/m2c-oracle synthesis after each round lets new asm state inform later constraint resolution.
- [ ] **C2** *(stretch)* Add stack-slot oracle to `constraint_solver`. MWCC DWARF recompile already gives slot names (`scripts/analysis/dwarf_locals.py`); not wired into the solver yet.

## Wave D — Apply at scale (mixed, after A+B)

With scanners + new patterns live, run sweeps to actually move match%.

- [ ] **D1** Run `function_health.py --top 50` to surface workable functions; dispatch a fleet of permuter hill-climbs (`batch_auto`) on the top picks.
- [ ] **D2** Run `mismatch_cluster.py --mode all` to surface header-cluster signatures; identify shared-header fixes that would unlock 10s-100s of functions.
- [ ] **D3** Run `batch_pattern_scan.py --pattern extrwi --min 90 --max 99.9`; apply the suggested patterns in bulk via `scan_and_permute.py`.
- [ ] **D4** Run new `B1`-`B4` patterns repo-wide via `pattern_scan`; commit each match% win individually.

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
