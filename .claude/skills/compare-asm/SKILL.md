---
name: compare-asm
description: Side-by-side target vs base assembly comparison with mismatch annotations. Shows aligned instructions with match markers, register swap details, and offset deltas. Use when diagnosing the last few percent of a function.
argument-hint: "<symbol_name>"
allowed-tools: Bash(python3 scripts/analysis/diff_inspect.py *), Bash(bin/objdiff-cli *), Read, Grep, Glob
---

# Compare ASM Skill

Side-by-side target vs base assembly comparison with annotations.

## Arguments

`$ARGUMENTS` - The function symbol name (MetroWerks mangled).

## Steps

1. **Run the comparison**:
   ```bash
   python3 scripts/analysis/diff_inspect.py --symbol "$ARGUMENTS" --compare-asm --project-dir .
   ```

2. **Read the output**:
   - Match markers: `=` equal, `~` diff_arg (same opcode, different args), `!` diff_op, `+` insert (target only), `-` delete (base only), `X` replace
   - Cluster boundaries mark contiguous insert/delete regions
   - `[reg:rN->rM]` annotations show register swap details
   - `[off:+N]` annotations show offset/immediate deltas
   - Consecutive equal instructions are collapsed (first 2 + last 2 shown)

3. **Diagnosis summary** at the end shows:
   - Mismatch type counts
   - Register swap pairs with frequency
   - Dominant offset delta

## Other Analysis Modes

The diff_inspect.py script supports several modes:

```bash
# Root cause diagnosis
python3 scripts/analysis/diff_inspect.py --symbol "$ARGUMENTS" --diagnose --project-dir .

# Mismatch clusters (insert/delete regions)
python3 scripts/analysis/diff_inspect.py --symbol "$ARGUMENTS" --clusters --project-dir .

# Register swap pairs
python3 scripts/analysis/diff_inspect.py --symbol "$ARGUMENTS" --regswaps --project-dir .

# Offset shift analysis
python3 scripts/analysis/diff_inspect.py --symbol "$ARGUMENTS" --offsets --project-dir .

# Summary counts by match type
python3 scripts/analysis/diff_inspect.py --symbol "$ARGUMENTS" --summary --project-dir .
```

## When to Use

- Working on a function's "last 10%" and need to see exactly where mismatches are
- Want to correlate register swaps with specific instruction sequences
- Need to understand the structure of insert/delete clusters
- Comparing the effect of a source change on assembly output

## Tips

- CW declaration order affects register allocation — try reordering variable declarations to fix register swaps
- Dominant offset deltas often indicate stack frame layout differences
- Use `--offsets` first to see the delta histogram, then `--compare-asm` for full context
- Combine with `/ghidra-decompile` to map registers to variable names
