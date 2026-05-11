# Verifiable: Identical Code Folding (ICF)

ICF risks are not always fixable, but they ARE verifiable — once you confirm a function got folded into another, you know what kind of "fix" can or can't recover the match.

## LINKER_MERGED Functions

The linker merges identical function bodies. Making structural changes to match one function can cause it to merge with another, dropping match% dramatically.

**Example:** In `HDCache::Init`, changing bool materialization pattern caused ICF merge regression from 92% to 79%.

## Watch for ICF When Changing Conditions

If a function uses `!flag` and you change it to `flag == 0`, the generated code might become identical to another function, triggering ICF.

## Verifying

Compare the function's symbol address against neighbors. Look for:

- Function bodies that compile identical to one earlier in the link order.
- objdiff's mismatches concentrating around the entry/exit instructions.
- The match% being suspiciously close to "everything except the symbol name".
