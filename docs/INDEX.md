# RB3 Docs Index

Top-level sitemap for the RB3 decomp documentation. Mirrors the structure of [DC3's docs/INDEX.md](../../dc3-decomp/docs/INDEX.md); only docs that actually exist in this repo are listed.

## Project Overview

| Doc | Description |
|-----|-------------|
| [../README.md](../README.md) | Project overview, status, build/diff instructions, acknowledgments |
| [../CLAUDE.md](../CLAUDE.md) | Agent context: tooling, commands, conventions, symbol naming |
| [../AGENTS.md](../AGENTS.md) | Same content as CLAUDE.md (symlinked) — for agent runtimes that prefer this filename |
| [SYNC_WITH_DC3.md](SYNC_WITH_DC3.md) | Tiered tooling sync plan with the sister DC3 project |

## Decomp Knowledge

| Doc | Description |
|-----|-------------|
| [decomp/patterns/INDEX.md](decomp/patterns/INDEX.md) | MetroWorks CodeWarrior codegen pattern catalog — fixable patterns + ICF risks |
| [decomp/objdiff-json-extensions.md](decomp/objdiff-json-extensions.md) | Fork-only objdiff-cli JSON: data-symbol diffs (`--include-data`, vtables/init data) and the instruction branch graph. See also the `/data-diff` skill. |

## Sessions & Analysis

Session logs and per-function decomp analyses live under [sessions/](sessions/):

| Path | Description |
|------|-------------|
| [sessions/2026-05-05-readme-rewrite-and-dc3-sync.md](sessions/2026-05-05-readme-rewrite-and-dc3-sync.md) | README rewrite + DC3 sync planning session |
| [sessions/decomp-analysis/](sessions/decomp-analysis/) | Per-function decomp investigations (AppendDeltas, BuildTransform, GemPlayer_Pass) |

## Cross-Project Reference

DC3 (the sister project) maintains a deeper catalog at `/home/free/code/milohax/dc3-decomp/docs/`:

- DC3's [docs/INDEX.md](../../dc3-decomp/docs/INDEX.md) — full sitemap
- DC3's [docs/decomp/patterns/INDEX.md](../../dc3-decomp/docs/decomp/patterns/INDEX.md) — MSVC pattern catalog (many filenames map 1:1 to ours)

When looking up shared Milo engine code, prefer DC3's reference implementation via the `lookup_dc3` MCP tool or `scripts/dc3_compare.py`.
