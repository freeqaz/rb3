---
name: analyze-function
description: Full analysis of a function combining objdiff match%, Ghidra decompilation, and m2c output. The go-to command before starting decomp work. Shows current state, target code structure, and a starting point for implementation.
argument-hint: "[symbol] [-u unit]"
allowed-tools: Bash(bin/analyze-function *), Bash(python3 tools/analyze_function.py *), Read, Grep, Glob
---

# Analyze Function

Run combined objdiff + Ghidra + m2c analysis on a function. This is the primary
reconnaissance tool for RB3 decomp work.

## Arguments

`$ARGUMENTS`

## Steps

1. **Run the analysis:**
   ```bash
   bin/analyze-function $ARGUMENTS
   ```

   Common patterns:
   ```bash
   # By mangled symbol (auto-finds unit)
   bin/analyze-function "Handle__7RndWindFP9DataArrayb"

   # With explicit unit (faster, avoids asm search)
   bin/analyze-function -u system/rndobj/Wind "Handle__7RndWindFP9DataArrayb"

   # Ghidra only (fastest)
   bin/analyze-function --no-objdiff --no-m2c "Handle__7RndWindFP9DataArrayb"

   # objdiff only (no network needed)
   bin/analyze-function --no-ghidra --no-m2c "Handle__7RndWindFP9DataArrayb"
   ```

2. **Interpret the output:**
   - **objdiff section**: Match%, verdict, instruction mismatches
   - **Ghidra section**: Decompiled C with DWARF symbol names and types
   - **m2c section**: Mechanical PPC-to-C translation (good for structure, not style)

3. **Present key findings:**
   - Current match percentage
   - What the function does (from Ghidra)
   - Key types and member accesses
   - Mismatch locations if partially matched

## Tips

- Always run this before starting work on a function
- Ghidra output has real variable names from DWARF — use these in your decomp
- m2c output shows the raw structure but needs cleanup for style
- If Ghidra is unavailable, use `--no-ghidra` to get objdiff + m2c only
- The unit flag `-u` speeds things up by avoiding a full asm directory search
- Unit paths are like `system/rndobj/Wind`, `band3/game/GemPlayer`, etc.
