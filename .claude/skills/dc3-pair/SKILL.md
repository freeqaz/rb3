---
name: dc3-pair
description: Find DC3 reference implementation for an RB3 unit. Shows function overlap, DC3 match status, and optionally the DC3 source code. Use to leverage DC3's ~87% complete decomp for shared Milo engine code.
argument-hint: "[unit-name] [--source]"
allowed-tools: Bash(python3 *), Read, Grep, Glob
---

# DC3 Pair Skill

Find the corresponding DC3 source file for an RB3 unit and show compatibility info.
DC3 and RB3 share the Milo engine -- most `system/` code is similar or identical.

## Arguments

`$ARGUMENTS` - An RB3 unit name (e.g., `CharBones`, `system/char/CharBones`). Add `--source` to include the full DC3 source code.

## Steps

1. **Find the DC3 source file** for the given RB3 unit:
   ```bash
   python3 scripts/dc3_compare.py --filter "$ARGUMENTS" --sort rb3
   ```

   This shows functions that DC3 has at 100% that RB3 can potentially port.

2. **If `--source` is requested**, read the DC3 source file:
   - DC3 source is at `/home/free/code/milohax/dc3-decomp/src/system/`
   - Map the RB3 unit path to DC3: `system/char/CharBones` -> `/home/free/code/milohax/dc3-decomp/src/system/char/CharBones.cpp`

3. **For a unit summary** showing which files have the most DC3-ported potential:
   ```bash
   python3 scripts/dc3_compare.py --units --filter system/
   ```

4. **For very low match functions** that DC3 has solved:
   ```bash
   python3 scripts/dc3_compare.py --filter system/ --max-rb3 50 --sort rb3
   ```

## DC3 vs RB3 Differences

When porting DC3 code to RB3, watch for:
- **Name mangling**: DC3 uses MSVC mangling, RB3 uses MetroWerks (CW)
- **Compiler**: DC3 targets Xbox 360 (MSVC PPC), RB3 targets Wii (CW PPC/Gekko)
- **Compiler flags**: RB3 uses `-O4,p -inline noauto -ipa file`; DC3 uses MSVC `/O2`
- **Class layouts**: May differ slightly due to different compilers/alignment rules
- **Includes**: Header paths are generally the same for `system/` code
- **Assertions**: Both use `MILO_ASSERT` but line numbers differ

## Porting Workflow

1. Run `/analyze-function` on the RB3 symbol to see Ghidra + current state
2. Run `/dc3-pair unit-name --source` to see DC3's implementation
3. Adapt the DC3 source for RB3 (fix includes, member names, assertions)
4. Build with `ninja` and check match with objdiff

## Tips

- DC3 decomp is ~87% complete -- excellent reference for shared engine code
- DC3 source: `/home/free/code/milohax/dc3-decomp/src/system/`
- Only `system/` code overlaps significantly; `band3/` and `network/` are game-specific
- Use `bin/analyze-function` first to understand what the original RB3 code does
- Always verify with objdiff after porting -- identical C++ can compile differently under CW vs MSVC
