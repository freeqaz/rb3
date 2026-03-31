---
name: ghidra-decompile
description: Decompile a function from Ghidra (RB3 Wii debug ELF). Shows decompiled C code with full DWARF symbols. Use when investigating function structure, types, or control flow before starting decomp work.
argument-hint: "[function-name-or-address]"
allowed-tools: Bash(python3 tools/analyze_function.py *), Bash(bin/analyze-function *), Read, Grep, Glob
---

# Ghidra Decompile (RB3)

Decompile a function from the RB3 debug ELF using Ghidra's decompiler via MCP.

The RB3 Ghidra instance runs on **port 8001** (ghidra.local:8001) with the debug ELF
(`~/code/milohax/milo-executable-library/rb3/Wii Proto (Bank 5) (Debug)/band_r_wii.elf`)
which has full DWARF symbols.

## Arguments

`$ARGUMENTS`

## Steps

1. **Resolve the function** - The argument can be:
   - A MetroWerks mangled symbol (e.g., `Handle__7RndWindFP9DataArrayb`)
   - A qualified C++ name (e.g., `RndWind::Handle`)
   - An address (e.g., `0x80123456`)

2. **Run the combined analysis** (preferred — gives objdiff match% + Ghidra + m2c):
   ```bash
   bin/analyze-function "$ARGUMENTS"
   ```

   If you know the unit:
   ```bash
   bin/analyze-function -u system/rndobj/Wind "$ARGUMENTS"
   ```

3. **For Ghidra only** (faster, skip objdiff and m2c):
   ```bash
   bin/analyze-function --no-objdiff --no-m2c "$ARGUMENTS"
   ```

4. **Present results** including:
   - Decompiled C code from Ghidra (with DWARF symbol names)
   - Match percentage from objdiff (if included)
   - m2c decompilation attempt (if included)

## Tips

- The debug ELF has full DWARF symbols — Ghidra decompilation is symbol-rich with real variable names and types
- RB3 uses MetroWerks name mangling (e.g., `Foo__6MyObjFv`) not MSVC mangling
- Ghidra must be running: `./tools/ghidra/pyghidra-service.sh start`
- The MCP client connects to `http://ghidra.local:8001/mcp`
- Use `bin/analyze-function` for the combined workflow — it handles symbol resolution, objdiff, Ghidra, and m2c in one command
- Large functions (>3000 bytes) may take longer to decompile
- Skip the sandbox for network operations if Ghidra appears unavailable

## When to Use

- Before starting decomp work on a function — see what the original code looks like
- When debugging type or struct issues — Ghidra shows real types from DWARF
- When you need to understand control flow (switch statements, nested conditions)
- Cross-referencing with m2c output for PowerPC-specific patterns
