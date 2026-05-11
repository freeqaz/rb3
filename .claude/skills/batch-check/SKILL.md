---
name: batch-check
description: Sweep a unit or glob of units for already-matching functions. Reads build/SZBE69_B8/report.json (no per-function objdiff invocations), auto-marks 100% matches as COMPLETE in decomp.db, and reports partial-match candidates ranked by closeness to 100%. Use this after a header change, a build-system change, or to find a unit's untouched workable functions.
argument-hint: "[unit-pattern] [--dry-run] [--json]"
allowed-tools: Bash(python3 scripts/batch_check.py *), Read, Grep, Glob
---

# Batch Check

Sweep a unit (or glob of units) and classify every function in it: complete (100%), partial, or unimplemented. Auto-mark 100% matches as `COMPLETE` in `decomp.db` so the orchestrator stops handing them out as work.

## Arguments

`$ARGUMENTS`

The first positional arg is a unit pattern. It can be:

- A specific unit: `system/char/CharBones`
- A glob: `system/char/*`, `band3/*`, `system/*/Char*`
- With or without the `main/` prefix (the script normalizes either way)

## Steps

1. **Run the sweep.** By default this updates `decomp.db` to mark new 100% matches as `COMPLETE`:

   ```bash
   python3 scripts/batch_check.py '$0'
   ```

   Add `--dry-run` if the user wants to preview without modifying the DB.
   Add `--json` if you need to parse the result programmatically.

2. **Read the output.** The text format gives:

   - **Pattern**: how the script normalized the user's input.
   - **Units matched**: how many units the glob hit.
   - **Functions checked**: total functions across those units.
   - **Complete / Partial / Unimplemented** counts.
   - **Marked COMPLETE**: how many functions got their DB verdict flipped.
   - **Partial matches** (top 30 by %): ranked candidates for fine-tuning — closer to 100% means lower-hanging fruit.
   - **Unimplemented** (top 20 by size): biggest remaining targets to start fresh decomp on.

3. **Suggest follow-ups.** Translate the result into next-step recommendations:

   - High-percent partials (≥95%): try `/permute SYMBOL` or `/analyze-function SYMBOL` and a short manual pass.
   - Mid partials (50–95%): `/analyze-function` first to understand the structural gap.
   - Big unimplemented funcs: `/analyze-function` + `/dc3-pair` to find a DC3 reference.

## When to Use

- After bumping a header that touches a class layout — sweep the affected unit to find regressions and new 100%s.
- When picking up work on a new area: `batch-check system/somemodule/*` shows what's done vs workable.
- Before running the orchestrator's `query_functions` so the DB reflects the current build's match state.

## Tips

- This script does NOT run objdiff per function — it reads the match% straight out of `build/SZBE69_B8/report.json`. That means it's fast (a glob over hundreds of units takes seconds) but the report must be current. If you just edited a source file, run `ninja` first so report.json reflects your changes.
- Partial-match display rounds to 2 decimals, so a function at 99.997% will show as "100.00%" but still be listed under partials. The classifier uses an actual `pct >= 100 - 1e-6` check, not the displayed value.
- The script counts a function as `unimplemented` if it has size 0 OR a 0% match. Real "no .o file yet" cases all roll into this bucket.

## Compared to DC3's batch-check

DC3 re-runs objdiff per function (which works around their report.json not always being fresh and gives them fields like LINKER_MERGED detection). RB3 uses a simpler report.json read because:

- RB3 has DWARF symbols, so the report's per-function data is reliable.
- Wii MWCC doesn't ICF the way MSVC does, so we don't need DC3's ICF-disambiguation pass.
- `ninja` already regenerates report.json as part of every build, so it's always fresh.

If you need DC3's stricter per-function objdiff behavior on RB3 someday, the data flow would be: parallel objdiff invocations via `bin/objdiff-cli diff` per symbol, parsing the output, then the same DB-update step.
