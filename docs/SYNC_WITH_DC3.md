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

- [x] **Add `AGENTS.md`** (mirror of CLAUDE.md). DC3 keeps both with identical content; some agent runtimes prefer one over the other. *Done 2026-05-11: implemented as a symlink to `CLAUDE.md` so they can't drift.*
- [x] **Add `docs/INDEX.md` sitemap** matching DC3's pattern. Visitors and agents both benefit from a single entry point. *Done 2026-05-11.*
- [x] **Restructure `docs/cw-compiler-patterns.md`** into `docs/decomp/patterns/INDEX.md` + per-pattern files. *Done 2026-05-11 — 10 categorized files mirroring DC3's filename schema (`fixable-bool-mask.md`, `fixable-casting.md`, `fixable-comparison.md`, `fixable-control-flow.md`, `fixable-copy-ctor.md`, `fixable-declarations.md`, `fixable-fsel-fma.md`, `fixable-macros.md`, `fixable-operators.md`, `fixable-struct-layout.md`, `verifiable-icf.md`) plus the INDEX.*
- [x] **Port `scripts/measure_progress.sh`** — HEAD vs current diff, worktree-aware. *Verified 2026-05-11: RB3 already has its own adapted version (SZBE69_B8 paths, simplified symlink logic since RB3 has no PCH workaround needed). Not a stale copy — appropriate divergence.*
- [x] **Port `scripts/setup_worktree.sh`** — configures ninja + symlinks compilers/tools/orig binary. Currently every worktree creation in RB3 fails its first build because of missing `orig/` symlinks and toolchain bootstrap. **High value** — proven painful in 2026-05-05 session. *Done 2026-05-07 (commit 83cde7bf), end-to-end tested.*
- [x] **Port `scripts/configure_existing_worktree.sh`** — sister script that configures an already-created worktree (e.g. one made by Claude Code's built-in isolation rather than `setup_worktree.sh`). Idempotent. *Done 2026-05-07.*
- [x] **Add `lookup_dc3` MCP tool** to RB3's orchestrator. Inverse of DC3's `lookup_rb3` — search the sister project's source for reference implementations of shared Milo engine functions. RB3 already has `scripts/dc3_compare.py` for offline comparison; this makes it an MCP tool callable from agents. *Done 2026-05-07.*
- [x] **Port relevant slash commands** from `dc3-decomp/.claude/skills/`:
  - [x] `batch-check` — verify a list of functions still match after a header change. *Done 2026-05-11.* RB3 version reads `build/SZBE69_B8/report.json` instead of re-running objdiff per function — simpler/faster than DC3 because we have full DWARF and no ICF complications. See `scripts/batch_check.py` + `.claude/skills/batch-check/SKILL.md`.
  - [N/A] `recon` — DC3's recon adds Unicorn behavioral comparison + DB diagnosis on top of objdiff. RB3's existing `/analyze-function` already covers objdiff + Ghidra (DWARF-rich) + m2c — the Unicorn layer isn't needed since RB3 has DWARF, and the DB diagnosis is provided by `mcp__orchestrator__run_diff_inspect`. Skip the direct port; treat `/analyze-function` as RB3's recon.
  - **Demoted to Tier 2** — `vtable`, `resolve-vcall`. DC3's versions parse MSVC COFF object files and read `??_R4` RTTI Complete Object Locators to recover sub-object offsets. Wii MWCC vtables have a completely different layout (`__vt__N<classname>`, no RTTI tail) and Wii MWCC doesn't ICF the way MSVC does. These would need a DWARF-based rewrite for RB3 rather than a port. Planning agents kicked off 2026-05-11; outputs at `docs/plans/vtable-skill.md` and `docs/plans/resolve-vcall-skill.md` (or noted as not produced if the agents bailed).
  - **Demoted to Tier 3** — `ai-advise`. DC3's version is heavy (LLM-driven, tied to permuter + tree-sitter) and is currently *disabled* in DC3 itself (`SKILL.md.disabled`). Reconsider only after RB3 has matching agent-home/ tooling.
  - **Skipped** (DC3 native-port specific, not applicable to Wii Gekko): `gpu-*`, `screenshot`, `xenia-gameplay`, `unicorn-query`.

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

Last updated: 2026-05-11 (Tier 1 complete: batch-check skill ported; recon is covered by `/analyze-function`; vtable/resolve-vcall demoted to Tier 2 with planning agents running; ai-advise demoted to Tier 3).

## Related

- Session log: `docs/sessions/2026-05-05-readme-rewrite-and-dc3-sync.md`
- DC3 docs index: `/home/free/code/milohax/dc3-decomp/docs/INDEX.md`
- DC3 patterns catalog: `/home/free/code/milohax/dc3-decomp/docs/decomp/patterns/INDEX.md`
- DC3 agents context: `/home/free/code/milohax/dc3-decomp/AGENTS.md`
