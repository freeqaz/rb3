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
   | **PERMUTED** | Same offset, same fingerprint, but the two sides touch it at **different program points** | Same slot *set*, variables assigned differently — MWCC slot-allocation shaping. Read the `↔ base 0x..` note for the mapping. **Not** a missing/extra local. |
   | **SHIFTED** | Same fingerprint, offset differs by the dominant Δ | One side has an extra local pushing the rest; find the extra local |
   | **TGT_ONLY** | Slot exists only on target | Target spills a temp that our build keeps in a register (or vice versa) |
   | **BASE_ONLY** | Slot exists only on our build | Our build is spilling extra; usually a register-pressure symptom |
   | **MATCH** | Same offset, same fingerprint, **and** same aligned access rows | Hidden by default; pass `--show-equal` to see |

   ⚠ **`MATCH` did not always mean this.** Before 2026-08-04 a row was MATCH
   whenever the offset and the `(kind,size,loads,stores)` fingerprint agreed. For
   a run of same-typed locals that fingerprint is **constant**, so a pure
   permutation of variables across identically-shaped slots read as MATCH.
   Measured over **N = 1,769** SZBE69_B8 functions that have at least one
   exact-offset paired user slot (drawn from all 2,744 partial-match functions in
   units with a base `.o`): **425 (24.0%)** had at least one such false MATCH, and
   **2,276 of 19,156 MATCH rows (11.9%)** moved MATCH → PERMUTED. No other verdict
   count changed. `SaveLoadManager::GetDialogMsg` went from "MATCH 105" to
   "30 MATCH / 75 PERMUTED". Any pre-2026-08-04 stack-layout reading of "slots all
   match" should be re-run before being trusted.

   Read the **signature discriminating power** line under the summary: it says how
   many target slots share a fingerprint with another. Where that number is high,
   any *fingerprint-based* pairing (SWAPPED, SHIFTED) is arbitrary within the
   group and is flagged `⚠ ambiguous`. On `GetDialogMsg` it is 230 of 231.

3b. **Frame size can now REFUSE.** If the prologue cannot be decoded the tool
   prints `UNKNOWN` (never `0x0`) and exits **2** with no frame verdict, because
   the callee-save slot filter is derived from the frame size. Pass
   `--allow-unknown-frame` to force exit 0. Previously an unparsed prologue
   defaulted to 0 on both sides and printed "→ Frame sizes match" — a vacuous
   `0 == 0`. That fired for real: 28 of the 2,744 functions above (notably the
   MWCC over-aligned `clrlwi/subfic/stwux` frames such as `ESP_GetTmd`, where the
   target allocates 0x140 and our build allocates nothing at all). The **refusal**
   path itself fires on **0 of 2,744** today — it is insurance, not something
   catching anything now.

   Self-check with no toolchain, objdiff or filesystem needed:
   ```bash
   python3 scripts/analysis/stack_layout.py --selftest   # expect PASS, 31 checks
   ```

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
- `--allow-unknown-frame` — exit 0 instead of 2 when the frame size could not be determined.
- `--selftest` — run the in-memory regression fixtures (no toolchain, no objdiff, no filesystem) and exit.

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
