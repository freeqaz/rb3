---
name: vtable
description: Dump a class's MWCC vtable from build/SZBE69_B8/obj/*.o. Shows every sub-object table (primary + each base), slot index, byte offset, and the mangled function symbol. Use when objdiff shows a `lwz r12, OFF(r12)` mismatch and you need to map the offset to a virtual method, or when verifying a header's virtual-function order against the compiled binary.
argument-hint: "CLASS [--unit UNIT] [--obj PATH] [--offset N] [--sub-offset N] [--json] [--demangle]"
allowed-tools: Bash(python3 scripts/dump_vtable.py *), Read, Grep, Glob
---

# Vtable Dump

Show the layout of a class's MWCC vtable as compiled for `SZBE69_B8`. Reads `.rela.data` relocations from the `.o` that defines `__vt__<len><class>`, splits it into sub-object tables by `__RTTI__<len><class>` markers, and reports every function-pointer slot with its mangled symbol.

This is the authoritative source for the build we're matching — vtable layouts differ between Wii banks, so the Bank 5 debug ELF (Ghidra's source) is the wrong place to read vtable bytes from.

## Arguments

`$ARGUMENTS`

First positional arg is the class name (unmangled, e.g. `Character`, `RndDrawable`, `GemTrackDir`). Mangled `__vt__9Character` is also accepted.

Flags:
- `--unit UNIT` — explicit objdiff-style unit (e.g. `system/char/Character`). Resolves to `build/SZBE69_B8/obj/<unit>.o`.
- `--obj PATH` — explicit `.o` file path.
- `--offset N` — show only the slot at slot index N (or byte offset N if `N >= 100`, treated as a live-vptr asm offset).
- `--sub-offset N` — sub-object offset for `--offset` lookup (default 0 = primary).
- `--json` — emit JSON.
- `--demangle` — pretty-print `Class::method` (length-prefix unwrap; not a full demangler).

Auto-detection of the `.o` file (when `--obj`/`--unit` are not given): glob `build/SZBE69_B8/obj/**/<Class>.o`, try simple prefix strips (`Rnd`, `Ham`, `Ui`), then fall back to scanning every `.o` for the `__vt__` symbol.

## Steps

1. **Dump the vtable.** Default text mode:

   ```bash
   python3 scripts/dump_vtable.py '$0'
   ```

   The output starts with a one-line summary:

   ```
   Class:    RndDrawable
   Vtable:   __vt__11RndDrawable (208 bytes, build/SZBE69_B8/obj/system/rndobj/Draw.o:.data+0x08)
   Sub-tables: 3
   ```

   followed by one block per sub-object table. Each block lists every slot:

   ```
   Sub-table 0: offset_to_top=0 (primary), sub-object offset +0, 23 slot(s), header at +0x00
     [  0] +0x08  ClassName__11RndDrawableCFv
     [  1] +0x0c  SetType__11RndDrawableF6Symbol
     ...
     [  5] +0x1c  Copy__11RndDrawableFPCQ23Hmx6ObjectQ33Hmx6Object8CopyType
   ```

   - `[N]` — slot index within this sub-table (0-based)
   - `+0xNN` — byte offset within the whole vtable symbol (file offset; includes the 8-byte header)
   - `offset_to_top` — signed delta from this sub-object back to the primary; `0` = primary, negative = secondary base

2. **Translate from an objdiff mismatch.** When you see `lwz r12, OFF(r12)` in asm, OFF is a *live-vptr offset* (post-header). Slot index = OFF / 4. For example, `lwz r12, 0x14(r12)` = slot 5.

3. **Look up a single slot.** When you already know the slot index:

   ```bash
   python3 scripts/dump_vtable.py '$0' --offset 5
   ```

   For non-primary sub-objects, add `--sub-offset 32` (matching the value from `lwz r12, 0x20(r3)` style sub-object loads). See `/resolve-vcall` for the three-argument form.

## When to Use

- objdiff shows mismatched slot offsets on a `r12`-based virtual dispatch and you need to know which virtual method each side is calling.
- Verifying a header's virtual function order against the compiled binary (especially after editing the class declaration).
- Auditing multi-inheritance layouts before adjusting member or vtable order in a class header.

## Tips

- MWCC on Gekko does not ICF — every slot has a unique symbol, no disambiguation needed.
- Thunks have the form `@N@Method__...` (single-step) or `@N@M@Method__...` (two-step); the `--demangle` output strips them and shows the underlying method, but the unstripped symbol is what's actually in the vtable slot.
- Vtable bytes are read from Bank 8 `.o` files only. Do not cross-reference with Bank 5 (`band_r_wii.elf`) vtable bytes — layouts differ.
- DWARF inheritance graphs ARE stable across banks, so if you need richer base-class context, `/struct-check` and `/ghidra-decompile` can supplement this output.
