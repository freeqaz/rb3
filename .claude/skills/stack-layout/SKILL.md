---
name: stack-layout
description: Diff stack-frame layouts between target and base for a function. Labels base-side slots with source variable names from a MWCC DWARF recompile. Identifies SWAPPED pairs (decl-reorder candidates), SHIFTED slots, DIFFER (different variables in same slot), and TGT_ONLY / BASE_ONLY (extra/missing locals). Filters out callee-save slots.
argument-hint: "<symbol_name> [-u <unit>]"
allowed-tools: Bash(python3 scripts/analysis/stack_layout.py *), Bash(python3 scripts/analysis/dwarf_locals.py *), Bash(python3 scripts/analysis/diff_inspect.py *), Read, Grep, Glob
---

# Stack Layout Diff Skill

Compare stack-frame layouts between target and our build for a single function. Returns frame size and callee-save counts at the top, then a per-slot diff table with verdicts that point to specific source fixes.

## Arguments

`$ARGUMENTS` — the mangled symbol name. Append `-u <unit>` if the symbol is ambiguous (e.g. `-u system/world/Crowd`).

## Steps

1. **Run the diff**:
   ```bash
   python3 scripts/analysis/stack_layout.py --symbol "$ARGUMENTS" --project-dir .
   ```

2. **Read the prologue summary** at the top:
   - `Frame Δ` and `Callee-saved GPR/FPR Δ`
   - If frame Δ is **fully explained by callee-save counts** → AT_LIMIT (the script says so explicitly). Not source-fixable.
   - Otherwise the structural Δ remainder is the real lever.

3. **Read the verdict table**. Rows are sorted with the most actionable first:

   | Verdict | Meaning | Action |
   |---|---|---|
   | **SWAPPED** | Two slots' fingerprints exchanged across sides | Reorder the two source declarations |
   | **DIFFER** | Same offset, different fingerprint (float-vs-int, different access pattern) | A different variable lives at that slot on each side — usually a declaration-reorder |
   | **SHIFTED** | Same fingerprint, offset differs by the dominant Δ | One side has an extra local pushing the rest; find the extra local |
   | **TGT_ONLY** | Slot exists only on target | Target spills a temp that our build keeps in a register (or vice versa) |
   | **BASE_ONLY** | Slot exists only on our build | Our build is spilling extra; usually a register-pressure symptom |
   | **MATCH** | Hidden by default; pass `--show-equal` to see |

4. **Fingerprint columns** (`kind sz=N L=loads S=stores A=accesses [first_idx..last_idx]`) help you guess the source type:
   - `float sz=4` → `float`
   - `float sz=8` / `paired sz=8` → `Vector2` / `double` / paired-single store
   - `int sz=4` → `int`, pointer, `bool`, or 32-bit member
   - `addr sz=0` → an `addi rN, r1, off` taking address-of (passed to a callee)

5. **"base var" column** is the source variable name our build allocates at that offset, extracted from a MWCC DWARF recompile. Use it to identify exactly which declaration to reorder. Target-side names are not available (no debug ELF for bank 8), but you usually only need the base side — that's the side you're editing.

## When to Use

- A function's diff shows many `[off:+N]` annotations.
- Frame sizes don't match between target and our build.
- You suspect a declaration reorder is the fix.
- You want a one-shot verdict on "AT_LIMIT vs structural" without reading the asm.

## Output knobs

- `--no-names` — skip the MWCC DWARF recompile + name extraction. The "base var" column disappears; ~0.5s faster.
- `--show-equal` — include MATCH rows (useful to confirm a fix didn't break already-aligned slots).
- `--show-callee-save` — include prologue/epilogue callee-save slots (hidden by default; they're not source-fixable).
- `--json-file <path>` — skip the objdiff invocation if you already have the JSON cached at `/tmp/claude/diff_*.json`.

## How name extraction works

By default the tool recompiles the function's source file with `-sym dwarf-2,full` to `/tmp/claude/stack_dwarf/<file>.dwarf.o`, parses the DWARF, and maps each `DW_OP_fbreg N` to a variable name. Cached by source mtime — second runs are ~0.5s.

Limits:
- **Base side only**: there's no debug ELF for the bank-8 target. TGT_ONLY rows show no name. You usually only need names for your own build anyway.
- **Compiler temps unnamed**: `$tvN` and similar compiler-introduced scratches aren't in DWARF. Empty "base var" cell ≠ "unknown variable" — it's "no source declaration."
- **Same name in nested scopes**: declarations like three sequential `noteRef`s each get their own slot, all named `noteRef`. Look at the row's `kind sz=N` to disambiguate.
- **MWCC compile fails / unsupported file**: name extraction warns + degrades to no-names mode (no crash).

## Detection limits

- Callee-save heuristics handle: `_save(gpr|fpr|gprlr)_NN` helpers, `stmw`, manual `stfd`/`psq_st`/`psq_stx`/`stw`. Unusual prologue shapes may under- or over-count; verify against the function's actual asm if the frame summary looks off.
- For purely structural diffs (no slot mismatches but persistent regressions), run `/compare-asm` next to spot non-stack causes.

## Tips

- After a declaration reorder, re-run to confirm SWAPPED rows resolve and MATCH count rises.
- `Dominant body-offset shift` reported separately from frame Δ — the dominant shift is what SHIFTED rows are normalized against.
- If verdicts are all DIFFER with no clean SHIFT/SWAP, the function is mid-reflow (many small changes, no single lever). Try `/compare-asm` or `/diff-inspect --diagnose`.
