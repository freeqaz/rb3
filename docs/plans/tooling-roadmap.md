# RB3 Tooling Roadmap — Post-Tier-1 Sync

Plan for the next round of RB3 decomp tooling work, picking up where the
2026-05-11 Tier 1 sync (`docs/SYNC_WITH_DC3.md`) left off.

Last updated: 2026-05-11.

## Prioritization principle

Rank by **impact-per-hour on the AI agent decomp loop**:

1. Does it unblock something agents currently get stuck on? (Highest priority.)
2. Does it tighten the edit → build → diff → decide cycle?
3. Does it raise the ceiling on match% (fewer AT_LIMIT calls)?
4. Does it make tooling self-maintaining (less manual bookkeeping)?

Native port work and shared-toolkit extraction are explicitly deferred — see
"Out of scope" at the bottom.

---

## Phase 1 — Implement Tier 2 vtable skills (~1 day)

**Why first:** plans are written (`docs/plans/vtable-skill.md`,
`docs/plans/resolve-vcall-skill.md`), the empirical findings are fresh, and
the shipped tooling immediately resolves a class of objdiff mismatches that
agents currently can't debug without manual disassembly reading.

### Step 1.1 — Verify the "one `__vt__N<class>` symbol per class" invariant (~30 min)

Both planning agents flagged this as their top open question. Cheap to check:

```bash
for f in build/SZBE69_B8/obj/**/*.o; do
    nm -g "$f" 2>/dev/null | awk '/__vt__/ {print FILENAME, $3}'
done | sort | uniq -c | sort -rn | awk '$1 > 1' | head
```

**Success criterion:** zero classes have more than one `__vt__N<class>`
symbol. If any do, document them in `docs/plans/vtable-skill.md` § 4 and plan
the multi-symbol fallback before implementation.

### Step 1.2 — Add `pyelftools` to `requirements.txt` (~5 min)

The vtable plan calls this out. Currently installed but undeclared.

### Step 1.3 — Implement `scripts/dump_vtable.py` (~3-4 hr)

Following `docs/plans/vtable-skill.md` § 1 (option a — `.o` relocations) and
§ 3 (CLI surface). Target ~250-350 LOC.

Reference fixture: `build/SZBE69_B8/obj/system/char/Character.o` (628-byte
vtable, multi-base inheritance, `@<delta>@` thunks).

**Success criteria:**
- `python3 scripts/dump_vtable.py Character` prints all 4 sub-object tables.
- `python3 scripts/dump_vtable.py Character --offset 0x48` returns the
  matching slot.
- `python3 scripts/dump_vtable.py Character --json` is parseable JSON
  matching the schema in `docs/plans/vtable-skill.md` § 2.
- Test against one virtual-inheritance class (TBD; sample with
  `grep -l 'DW_AT_virtuality' …` from the debug ELF).

### Step 1.4 — Add `resolve` subcommand (~1-2 hr)

Following `docs/plans/resolve-vcall-skill.md` (with the § 0 revision applied
— read vtable bytes from Bank 8 `.o` files, NOT the Bank 5 debug ELF).

**Success criterion:** `python3 scripts/dump_vtable.py resolve RndDrawable 0 5`
returns `RndDrawable::Copy` (or the actual slot 5 — verify with a manual
read of the `.o`).

### Step 1.5 — Wire `.claude/skills/vtable/SKILL.md` and `resolve-vcall/SKILL.md` (~30 min)

Two thin SKILL.md files following the format of `.claude/skills/batch-check/SKILL.md`.

### Step 1.6 — Update `docs/SYNC_WITH_DC3.md` (~5 min)

Mark Tier 2 vtable/resolve-vcall items complete, link to the shipped script
and skill files.

---

## Phase 2 — BSF permuter port (~1-2 days)

**Why second:** the [MWCC pattern catalog](../decomp/patterns/INDEX.md) is
dominated by register-allocation patterns —
[Variable Declaration Order](../decomp/patterns/fixable-declarations.md#variable-declaration-order),
[Local Pointer Cache for Register Hoisting](../decomp/patterns/fixable-declarations.md#local-pointer-cache-for-register-hoisting),
[Pre-Loading Member Before Loop](../decomp/patterns/fixable-declarations.md#pre-loading-member-before-loop-for-register-hoisting).
At RB3's current 79%
cross-platform fuzzy match, a meaningful fraction of the remaining gap is
register-swap residuals that a naive permuter can't target. DC3's BSF engine
traces which callee-saved register the compiler assigned each variable,
isolates per-function register pressure, and lets the permuter make targeted
reg-allocation mutations instead of blind shuffling.

### Step 2.1 — Read DC3's BSF design (~1-2 hr)

Source: `/home/free/code/milohax/dc3-decomp/docs/permuter/bsf-engine.md` and
the implementation under `dc3-decomp/scripts/permuter/`. Understand the
color → GPR mapping and how it integrates with the rest of the permuter.

### Step 2.2 — Plan the port to RB3's permuter (~2-3 hr)

Write `docs/plans/bsf-permuter-port.md` covering:
- What RB3's existing permuter (`scripts/permuter/`) does today.
- Which BSF components transfer directly (the tracing logic should be
  compiler-agnostic) vs which need MWCC-specific adaptation (instruction
  scheduling, register naming).
- Integration points with `scripts/orchestrator/` so the orchestrator can
  request a "register-targeted" permutation.

### Step 2.3 — Implement and validate (~half-to-full day)

Pick 3-5 known-stuck functions (95-99% match, register-swap-dominated) as
the validation set. Run BSF-targeted permutation; success means at least one
hits 100% without manual intervention.

Add the validation set to `docs/plans/bsf-permuter-port.md` as a regression
suite for future BSF changes.

---

## Phase 3 — Quick agent-loop wins (parallelizable, ~half day each)

These are independent and can be done in any order. Each is small,
self-contained, and improves something agents do dozens of times per day.

### Step 3.1 — `mcp__orchestrator__next_target` tool (~3-4 hr)

Add to `scripts/orchestrator/mcp_server.py`. Queries `decomp.db` for
workable functions ranked by:

```
score = closeness_to_100 × log(fan_in + 1) / (attempts + 1)
```

Filters out `verdict IN ('COMPLETE', 'AT_LIMIT')`. Inputs: optional unit
glob, optional `min_percent` floor. Returns top N with metadata (current %,
last attempt timestamp, last attempt outcome).

**Why:** today the agent loop spends context on "what should I work on
next?" via `/progress` + manual judgment. Making it a tool collapses that
to a single MCP call.

**Success criterion:** agent in a fresh session can ask "give me 5 RB3
functions to work on right now" and get a list ranked by ROI without any
preamble.

### Step 3.2 — Ninja hook for ambient `batch_check` (~1-2 hr)

Two implementation options, both small:

- **Option A:** Add a phony ninja target `batch-check-all` that runs
  `python3 scripts/batch_check.py 'system/*' 'band3/*'` after `report.json`
  regenerates. User runs `ninja batch-check-all` instead of `ninja`.
- **Option B:** Add a post-build action in `configure.py` so plain `ninja`
  always does the sweep. Risk: slows the build if `batch_check` ever gets
  expensive.

Recommend Option A — explicit, can't slow down hot edit-build-diff loops.

**Success criterion:** running `ninja batch-check-all` after a header
change leaves `decomp.db` reflecting the new 100% matches with no manual
intervention.

### Step 3.3 — `lookup_dc3` ranking by DC3 match% (~1-2 hr)

Current grep returns up to 1948 hits for common names. Improve by:

1. Load DC3's `build/373307D9/report.json` (cache on path mtime).
2. For each grep hit, find which DC3 unit it's in and look up the unit's
   match%.
3. Sort results by descending unit match% (higher = more reliable reference).
4. Add `--min-match` flag to filter to e.g. ≥90% units only.

**Success criterion:** `lookup_dc3 Poll --min-match 90` returns ≤20 hits,
all from DC3 units that decomp'd cleanly.

### Step 3.4 — `bin/sweep-untouched` script (~2 hr)

Already partially captured in `scripts/dc3_compare.py`. Promote a focused
subset to `bin/sweep-untouched`:

```
bin/sweep-untouched system/char/ --min-size 100 --max-attempts 0
```

Lists untouched (zero-decomp) functions in the unit glob, ranked by size
descending, with `--max-attempts N` to filter out things that already
failed N times. Pairs with `next_target` MCP tool for the picking-work-to-do
loop.

---

## Out of scope / deferred

| Item | Why deferred |
|---|---|
| **Native port scaffolding** | Tier 3 in SYNC_WITH_DC3.md. Premature until RB3 decomp is closer to 100% — DC3's WebGPU port came after they hit ~93% cross-platform code matched, RB3 is at ~79%. |
| **Shared `milohax-decomp-toolkit` extraction** | Tier 3. Wait until improvements flow both ways often enough that the duplicate-then-refactor cost exceeds the extraction cost. |
| **Cross-project regression CI** | Low signal until DC3 actively changes shared-engine code that needs RB3 verification. |
| **Pattern auto-classification from `decomp.db`** | Interesting but manual catalog scales fine at current pattern count (~30 documented). Revisit at ~100 patterns. |
| **Permuter cache infrastructure expansion** | `permuter_cache.db` exists; improve only after BSF is in and we know what cache shape is needed. |
| **`ai-advise` skill port** | Demoted in SYNC_WITH_DC3.md (LLM-heavy, disabled even in DC3). |

---

## Sequencing rationale

```
Phase 1 (vtable/resolve-vcall)
    │
    ├──► unblocks vtable-offset diagnosis in objdiff
    │
    ▼
Phase 2 (BSF permuter)
    │
    ├──► raises ceiling on register-swap-limited functions
    │    (current 79% fuzzy → meaningful push toward 90%+)
    │
    ▼
Phase 3 (quick wins, parallel)
    │
    └──► tightens agent loop on top of the new capabilities
```

Phase 1 is sequenced first because the plans are fresh and the verification
work (Step 1.1) is too cheap not to do immediately. Phase 2 is the biggest
match%-impact lever once vtable visibility exists. Phase 3 items are
independent and can interleave with Phase 2 — Step 3.1 (`next_target`) in
particular pairs well with whatever's done in Phase 2 since BSF-improved
functions become the new "ROI sweet spot."

## Status tracking

Mark steps as complete in this doc as they ship. If scope creep adds new
items, append them under the relevant Phase. When Phase 1 is done, the next
session should re-evaluate Phase 2 vs Phase 3 priorities based on whatever
the post-vtable state of the decomp.db looks like.

## Related

Docs from this session that this roadmap builds on:

- [docs/INDEX.md](../INDEX.md) — top-level sitemap (entry point for the rest)
- [docs/SYNC_WITH_DC3.md](../SYNC_WITH_DC3.md) — full DC3↔RB3 sync plan
  including Tier 3 items; this roadmap covers what was outstanding after the
  2026-05-11 close of Tier 1
- [docs/decomp/patterns/INDEX.md](../decomp/patterns/INDEX.md) — MWCC pattern
  catalog; Phase 2's BSF justification rests on the register-allocation
  patterns documented there
- [docs/plans/vtable-skill.md](vtable-skill.md) — Phase 1 design (vtable
  skill: data source, output shape, CLI, MI handling)
- [docs/plans/resolve-vcall-skill.md](resolve-vcall-skill.md) — Phase 1
  design (resolve subcommand; note the § 0 revision required before
  implementation)
- [docs/sessions/2026-05-05-readme-rewrite-and-dc3-sync.md](../sessions/2026-05-05-readme-rewrite-and-dc3-sync.md) —
  session log capturing the framing decisions that shaped this work stream

External references:

- DC3 BSF engine reference: `/home/free/code/milohax/dc3-decomp/docs/permuter/bsf-engine.md`
- DC3 pattern catalog (cross-pollination): `/home/free/code/milohax/dc3-decomp/docs/decomp/patterns/INDEX.md`
