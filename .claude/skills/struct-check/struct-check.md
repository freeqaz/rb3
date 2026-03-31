---
name: struct-check
description: Compare C++ header struct layouts against Ghidra's DWARF-inferred layouts. Shows field-by-field offset comparison to detect layout mismatches. Requires struct_db.sqlite (built from annotated headers).
argument-hint: "<ClassName> | --unit <unit-path> | --all [--pattern 'Rnd*']"
allowed-tools: Bash(python3 tools/ghidra/struct_check.py *), Bash(python3 tools/struct_db.py *), Read, Grep, Glob
---

# Struct Layout Check (RB3)

Compare C++ header struct layouts against Ghidra's DWARF-inferred layouts.
Detects field offset mismatches between our decomp headers and the debug binary.

## Arguments

`$ARGUMENTS`

## Prerequisites

The struct_db.sqlite must exist. If it doesn't, build it first:
```bash
python3 tools/struct_db.py build src/
```

## Steps

1. **Run the comparison:**
   ```bash
   python3 tools/ghidra/struct_check.py $ARGUMENTS
   ```

   Common patterns:
   ```bash
   # Check a single class
   python3 tools/ghidra/struct_check.py Character

   # Check all classes in a translation unit
   python3 tools/ghidra/struct_check.py --unit system/char/Character

   # Check all classes matching a pattern
   python3 tools/ghidra/struct_check.py --all --pattern 'Rnd*'

   # Check all classes (comprehensive)
   python3 tools/ghidra/struct_check.py --all

   # JSON output for scripting
   python3 tools/ghidra/struct_check.py Character --json
   ```

2. **Interpret the output:**
   - **OK**: Field at same offset in both our headers and Ghidra
   - **NAME_DIFF**: Field at same offset but different name (often Ghidra auto-naming)
   - **GHIDRA_MISSING**: We have a field at this offset but Ghidra doesn't
   - **OURS_MISSING**: Ghidra has a field at this offset but we don't
   - Offset mismatches between corresponding fields indicate inheritance or layout errors

3. **Present findings:**
   - Summary of matches vs mismatches per class
   - Specific fields with offset discrepancies
   - Suggestions for fixing layout issues

## Struct DB Management

```bash
# Build struct_db from annotated headers
python3 tools/struct_db.py build src/

# Look up a field at a specific offset
python3 tools/struct_db.py lookup Character 0x1e8

# Show full class info
python3 tools/struct_db.py info Character

# List all classes
python3 tools/struct_db.py list --pattern 'Char*'
```

## When to Use

- After adding offset annotations to a header file -- verify they match Ghidra
- When objdiff shows offset mismatches -- check if the struct layout is wrong
- Before starting work on a class's methods -- ensure the layout is correct
- Auditing struct layouts across a module (e.g., all Rnd* classes)

## Tips

- The debug ELF has full DWARF symbols with accurate struct layouts
- Offset mismatches between our headers and Ghidra usually mean:
  - Wrong base class size (check parent class offsets)
  - Missing padding or alignment fields
  - Wrong member order
- RB3 uses ILP32 (Wii/Gekko): pointers are 4 bytes, ints are 4 bytes
- Headers need `// 0xHEX` offset annotations on members to appear in struct_db
- Ghidra must be running: `./tools/ghidra/pyghidra-service.sh start`
