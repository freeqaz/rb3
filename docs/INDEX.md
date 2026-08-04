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
| [decomp/patterns/fixable-liveness.md](decomp/patterns/fixable-liveness.md) | **Register swaps are symptoms, not causes.** Corrects the "declaration order controls register allocation" framing in `permuter-roi.md` and `at-limit-mwcc.md`. Provenance is labelled per claim — MWCC-measured here vs ABI consequence vs MSVC-measured-and-unverified-on-MWCC. |
| [decomp/objdiff-json-extensions.md](decomp/objdiff-json-extensions.md) | Fork-only objdiff-cli JSON: data-symbol diffs (`--include-data`, vtables/init data) and the instruction branch graph. See also the `/data-diff` skill. |

## Knowledge Base (migrated agent-memory history)

Completed/closed investigations, campaign narratives, and dead-end catalogs
migrated out of the persistent agent-memory store into version control. See
[knowledge/INDEX.md](knowledge/INDEX.md) for the full sitemap.

| Doc | Description |
|-----|-------------|
| [knowledge/render-and-visual-history.md](knowledge/render-and-visual-history.md) | Render/visual fix history + engine-arch-review durable conclusions |
| [knowledge/web-port-history.md](knowledge/web-port-history.md) | Emscripten/WASM web port, audio-fidelity, and load-perf history |
| [knowledge/decomp-campaign-history.md](knowledge/decomp-campaign-history.md) | Decomp tooling/campaign narratives + completed native-port workstreams |
| [knowledge/at-limit-catalog.md](knowledge/at-limit-catalog.md) | Functions/TUs stuck below 100% — known dead ends to avoid re-grinding |
| [knowledge/infra-and-tooling-history.md](knowledge/infra-and-tooling-history.md) | Build/objdiff gotchas + assorted completed fixes |

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

There is also **`/home/free/code/milohax/rb3-xenon`** — Rock Band 3 for Xbox 360, i.e. *this
game* on *DC3's compiler*. For a shared-engine question it is often the most useful third
opinion, because it isolates compiler from game.

### RB3 is sometimes the arbiter, not the follower

`ObjectDir::Iterate` (`src/system/obj/Dir.cpp:767`) is the worked example. Both MSVC trees
had it gated on the **class** symbol rather than the optional **type** symbol; since the class
symbol is assigned on every path it is never null, so the guard degenerated to
`it->Type() == <class name>` — and `Object::Type()` returns a null `Symbol` whenever
`mTypeDef` is null, which is the common case. Every `{$dir iterate ...}` body therefore ran
**zero times**, silently: no error, no warning, and no match-percentage signal, because the
predicate is behaviour rather than codegen.

**Our reconstruction was correct all along**, in both the `MILO_DEBUG` and release branches
(`sym2.Null() || it->Type() == sym2`) — and being an independent decomp of the same game on a
different compiler is exactly what made it decisive. It is what settled the semantics for
dc3-decomp (`4e4cf851`) and rb3-xenon (`dd144927` / `5260e280`), where DC3's target asm then
confirmed it: the target seeds a register from `gNullStr` before the branch, overwrites it only
in the array branch, and null-tests *that* register.

Two standing consequences:

- **Do not "align" this function to `rjkiv/dc3-decomp`'s shape.** That tree
  (`/home/free/code/milohax/og-dc3-decomp`) still carries the bug, and its `s8` — the optional
  type symbol read from `a2->Sym(1)` — is assigned and never read, which is the dead-local tell.
  A high match% upstream is a statement about *expression shape*, not about whether a predicate
  is right.
- **When a shared-engine question is about logic rather than codegen, our reading is evidence.**
  Behavioural bugs of this class are invisible to every match-percentage-based scanner in all
  three repos at once; agreement between two independently reconstructed trees is the strongest
  signal available.

### objdiff pattern-doc links now resolve against this repo

objdiff-cli emits pattern-doc URLs relative to the **consuming** repo, detected by marker
filename in `docs/decomp/patterns/`. We carry `permuter-roi.md` / `at-limit-mwcc.md` and
resolve as `DocProject::Rb3`; DC3 and rb3-xenon carry `PERMUTER_ROI_ANALYSIS.md` /
`at-limit-systemic.md` and resolve as `Dc3`. Override with
`OBJDIFF_DOC_PROJECT={dc3,rb3,unknown}`.

Before this landed (`../objdiff` `1030000`), the URL table was DC3-relative and **29 of the 30
emittable URLs failed against our tree** — and `at-limit-mwcc.md`, which is *our* MWCC-specific
file, was being surfaced inside the MSVC repos. Both directions are fixed.

- **Anchor stability is a contract.** objdiff renders only the **first** URL per pattern, so
  renaming a heading those links point at degrades tool output silently. Verify any doc rename:
  `python3 ../objdiff/scripts/check_doc_links.py --rb3 . --allow-missing` (currently **25/25**).
- **One binary, three repos.** `bin/objdiff-cli` here, in `../dc3-decomp` and in `../rb3-xenon`
  are all symlinks to the same `../objdiff/target/release/objdiff-cli`. One
  `cargo build --release -p objdiff-cli` propagates to all three — and nothing propagates until
  someone runs it.
