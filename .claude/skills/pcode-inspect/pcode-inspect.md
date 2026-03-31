---
name: pcode-inspect
description: Analyze a function's switch statements, cast operations, and type extensions from Ghidra. Uses both decompiled C output and raw PPC instruction analysis. Useful for understanding control flow patterns and type coercion in the target binary.
argument-hint: "<function-name-or-address> [--switches] [--casts]"
allowed-tools: Bash(python3 tools/ghidra/pcode_inspect.py *), Read, Grep, Glob
---

# P-code / Switch / Cast Inspector (RB3)

Analyze a function's switch statements, cast operations, and type extensions
using Ghidra's decompilation and raw PowerPC instruction analysis.

## Arguments

`$ARGUMENTS`

## Steps

1. **Run the analysis:**
   ```bash
   python3 tools/ghidra/pcode_inspect.py $ARGUMENTS
   ```

   Common patterns:
   ```bash
   # By CW mangled symbol
   python3 tools/ghidra/pcode_inspect.py "Handle__7RndWindFP9DataArrayb"

   # By C++ name (searches for matching symbols)
   python3 tools/ghidra/pcode_inspect.py "RndWind::Handle"

   # By address
   python3 tools/ghidra/pcode_inspect.py "0x80123456"

   # Switches only
   python3 tools/ghidra/pcode_inspect.py "Handle__7RndWindFP9DataArrayb" --switches

   # Casts only
   python3 tools/ghidra/pcode_inspect.py "Handle__7RndWindFP9DataArrayb" --casts

   # Skip decompilation, analyze raw bytes only
   python3 tools/ghidra/pcode_inspect.py "0x80123456" --no-decompile --raw-bytes 4096
   ```

2. **Interpret the output:**
   - **Switch statements**: Detected from Ghidra annotations and raw `bctr`/`mtctr`/`lwzx` patterns
   - **Cast operations (raw bytes)**: `extsb`/`extsh`/`extsw` (sign extension), `rlwinm` masks (zero extension)
   - **Cast operations (decompiled)**: Ghidra's `(int)`, `(uint)`, `SUBn()`, `SEXTn()`, `ZEXTn()` patterns
   - **Full decompiled output**: Shown in default mode

3. **Present key findings:**
   - Number of switch statements and their case counts
   - Sign/zero extension patterns that reveal the original C++ types
   - Cast patterns that indicate where type narrowing or widening occurs

## When to Use

- Investigating a function with switch statements to determine case count/structure
- Understanding type coercion patterns before writing decomp code
- Diagnosing cast-related mismatches in objdiff output
- Cross-referencing Ghidra's type inference with the expected C++ types

## Tips

- The script searches for CW-mangled symbols (e.g., `Foo__6MyObjFv`)
- `extsb` = `(signed char)`, `extsh` = `(short)`, `extsw` = `(int)` from larger types
- `rlwinm` with mask 24,31 = `(unsigned char)` or `& 0xFF`
- `rlwinm` with mask 16,31 = `(unsigned short)` or `& 0xFFFF`
- For large functions, use `--raw-bytes 8192` to analyze more instructions
- Ghidra must be running: `./tools/ghidra/pyghidra-service.sh start`
