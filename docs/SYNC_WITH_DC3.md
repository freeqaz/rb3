# Sync plan: RB3 ← DC3

Permanent reference for keeping RB3 tooling in sync with the more mature DC3 sister project at `/home/free/code/milohax/dc3-decomp/`. Both projects are part of freeqaz's AI decomp + native engine rewrite series.

## Project relationship

DC3 is the lead project — the AI decomp methodology was built and proven there first on a harder target (Xbox 360 MSVC PowerPC, no DWARF, ICF, link-time pragmas). RB3 is the second application of the same methodology to a sister codebase (Wii Gekko/MWCC) sharing the Milo engine.

| Aspect | DC3 | RB3 |
|---|---|---|
| Platform | Xbox 360 (PowerPC Xenon) | Wii (PowerPC Gekko/Broadway) |
| Compiler | MSVC for Xbox 360 (ICF, LTCG-off debug) | MetroWorks CodeWarrior 4.3.172 (no LTO) |
| Symbols | None — Ghidra inferred | Full DWARF in debug ELF |
| Ghidra MCP port | 8000 | 8001 |
| Orig binary version | `373307D9` | `SZBE69_B8` |
| Native port | Linux WebGPU + WASM + HTTP debug | not started |

## Long-term goal

Shared toolkit (`milohax-decomp-toolkit` or similar) with both projects depending on it. Reduces duplication and makes improvements flow both ways. **Defer until** RB3 has caught up enough that the porting cost of "duplicate first, refactor later" exceeds the toolkit-extraction cost.

## Tiered sync plan

### Tier 1 — high-value, low-risk (do these first)

These are mostly file ports with minimal adaptation. Order matters: docs structure first so later items have a place to land.

- [ ] **Add `AGENTS.md`** (mirror of CLAUDE.md). DC3 keeps both with identical content; some agent runtimes prefer one over the other. RB3 currently has only `CLAUDE.md`.
- [ ] **Add `docs/INDEX.md` sitemap** matching DC3's pattern. Visitors and agents both benefit from a single entry point.
- [ ] **Restructure `docs/cw-compiler-patterns.md`** into `docs/decomp/patterns/INDEX.md` + per-pattern files (mirror DC3's `docs/decomp/patterns/` catalog: `fixable-bool-mask.md`, `fixable-casting.md`, `fixable-control-flow.md`, `fixable-loop-condition.md`, `fixable-struct-layout.md`, `harmful-avoid.md`, `unfixable-compiler.md`, etc.). Adapt MSVC-specific patterns to MWCC equivalents.
- [ ] **Port `scripts/measure_progress.sh`** — HEAD vs current diff, worktree-aware. Adapt path to `build/SZBE69_B8/report.json`.
- [x] **Port `scripts/setup_worktree.sh`** — configures ninja + symlinks compilers/tools/orig binary. Currently every worktree creation in RB3 fails its first build because of missing `orig/` symlinks and toolchain bootstrap. **High value** — proven painful in 2026-05-05 session. *Done 2026-05-07 (commit 83cde7bf), end-to-end tested.*
- [x] **Port `scripts/configure_existing_worktree.sh`** — sister script that configures an already-created worktree (e.g. one made by Claude Code's built-in isolation rather than `setup_worktree.sh`). Idempotent. *Done 2026-05-07.*
- [x] **Add `lookup_dc3` MCP tool** to RB3's orchestrator. Inverse of DC3's `lookup_rb3` — search the sister project's source for reference implementations of shared Milo engine functions. RB3 already has `scripts/dc3_compare.py` for offline comparison; this makes it an MCP tool callable from agents. *Done 2026-05-07.*
- [ ] **Port relevant slash commands** from `dc3-decomp/.claude/skills/`:
  - `recon` — quick function reconnaissance (objdiff + Ghidra summary)
  - `batch-check` — verify a list of functions still match after a header change
  - `ai-advise` — meta-advisor that suggests next strategy
  - `vtable` — dump vtable layouts
  - `resolve-vcall` — resolve indirect calls to concrete targets
  - Skip: `gpu-*`, `screenshot`, `xenia-gameplay`, `unicorn-query` (DC3 native-port specific)

### Tier 2 — medium-value (assess after Tier 1)

- [ ] **Port permuter framework BSF engine** — DC3's `docs/permuter/bsf-engine.md` (register allocation tracing, per-function isolation, color→GPR mapping). Currently RB3 has basic source permuter integration; the BSF engine is the next leap.
- [ ] **Port `run_analyze_function`** to RB3's orchestrator MCP — combines objdiff with struct offset resolution for field-level mismatch context.
- [ ] **Port `clean_stale_objects.sh`** — finds .o files older than the PCH; useful when ninja's dependency tracking gets confused.
- [ ] **Add `docs/decomp/PRAGMA_INDEX.md`** — MWCC pragma reference, mirror of DC3's MSVC pragma index. Less critical because MWCC has fewer matching-relevant pragmas than MSVC, but still useful.
- [ ] **Port `merger_agent.py`** orchestrator component — DC3 uses it to merge concurrent agent outputs intelligently.
- [ ] **Port `worktree_pool.py`** — managed worktree pool for batch agent runs.

### Tier 3 — long-term goal

- [ ] **Extract shared toolkit** — refactor common scripts/MCP tools into a separate repo (e.g., `freeqaz/decomp-toolkit-shared`) that both DC3 and RB3 depend on. Trigger this when porting cost of new improvements exceeds extraction cost.
- [ ] **Native port for RB3** — Wii is sandboxed (different from Xenon's POSIX-ish env); needs a different approach than DC3's WebGPU port. Likely a years-of-work side project; defer unless decomp gets close to 100%.
- [ ] **Cross-project regression CI** — when DC3 source changes for shared engine code, run RB3's objdiff against affected functions to catch regressions.

### Explicit non-goals

- Don't port DC3's VMX128 Ghidra processor (Xenon SIMD; Wii doesn't have it).
- Don't port DC3's Unicorn function runner — niche; Wii has full DWARF so dynamic execution is rarely needed.
- Don't port DC3's HTTP debug server / DTA overlay system / native port specifics until RB3 has its own native port effort.
- Don't port DC3's `agent-home/` until we understand what it provides.

## Adaptation notes

When porting DC3 → RB3, expect to change:

| What | DC3 value | RB3 value |
|---|---|---|
| Build version | `373307D9` | `SZBE69_B8` |
| Ghidra MCP port | 8000 | 8001 |
| Compiler ID in scripts | MSVC | MWCC (mwcceppc 4.3.172) |
| Pattern names | MSVC-specific (e.g., `fixable-fsel-fma`) | MWCC equivalent or skip |
| RB reference flag | `lookup_rb3` (DC3 looks at RB3) | `lookup_dc3` (RB3 looks at DC3) |
| Path constants | `dc3-decomp` | `rb3` |
| Pragma references | MSVC pragmas | MWCC pragmas |

## Status tracking

Mark items complete in this doc as they ship. When the file gets long enough, split per-tier into separate files under `docs/sync/`.

Last updated: 2026-05-07 (Tier 1: setup_worktree.sh, configure_existing_worktree.sh, lookup_dc3 MCP tool shipped).

## Related

- Session log: `docs/sessions/2026-05-05-readme-rewrite-and-dc3-sync.md`
- DC3 docs index: `/home/free/code/milohax/dc3-decomp/docs/INDEX.md`
- DC3 patterns catalog: `/home/free/code/milohax/dc3-decomp/docs/decomp/patterns/INDEX.md`
- DC3 agents context: `/home/free/code/milohax/dc3-decomp/AGENTS.md`
