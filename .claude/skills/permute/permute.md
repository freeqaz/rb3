---
description: Run the source permuter to find better-matching code variants for a function
user-invocable: true
---

# /permute — Source Code Permuter

Generate and score source code permutations to improve match% for a function.

## Usage

The user provides a mangled symbol name. Run the permuter from the RB3 repo root:

```bash
# Basic permutation (generates variants, scores each)
python3 -m decomp_synth --symbol "SYMBOL" --max-variants 20

# Hill climber (iteratively improves, keeps best)
python3 -m decomp_synth.hill_climber --symbol "SYMBOL"

# With explicit source file
python3 -m decomp_synth --symbol "SYMBOL" --source src/path/to/file.cpp --max-variants 20

# Dry run (show variants without compiling)
python3 -m decomp_synth --symbol "SYMBOL" --dry-run
```

## How It Works

1. Takes the current source for a function
2. Generates permutations: declaration reordering, condition flipping, variable inlining, loop restructuring, etc.
3. Compiles each variant with the same MetroWerks CW flags used by the build
4. Runs objdiff to score each variant against the target binary
5. Reports the best-scoring variant and the diff

## Notes

- The permuter auto-detects RB3 vs DC3 based on the working directory
- Provided by the installed `decomp_synth` package (shared via the `venv` symlink to dc3-decomp)
- Best used on functions in the 70-95% range where small source changes can push the match higher
- If a variant scores better, apply its changes to the source file and verify with `ninja` + `mcp__orchestrator__run_objdiff`
