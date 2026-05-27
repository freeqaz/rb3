---
name: data-diff
description: Diff a DATA symbol (vtable, pointer/jump table, string pool, static initializer) between the target and the decompiled build, showing byte differences and — for each relocation slot — which function/symbol each side points to. Use when a data symbol is below 100%, or to see exactly where your decompiled vtable diverges from the target (which slot resolves to the wrong function). This is the diff counterpart to /vtable and /resolve-vcall, which only read the target.
argument-hint: "SYMBOL [--unit UNIT]"
allowed-tools: Bash(bin/objdiff-cli *), Bash(nm *), Read, Grep, Glob
---

# Data Diff

Structured byte- and relocation-level diff of a **data** symbol, target vs the
decompiled build. objdiff's normal output diffs code; `--include-data` (a
milohax-fork feature) handles data: vtables, RTTI, pointer/jump tables, string
pools, and static initializers.

Where `/vtable` and `/resolve-vcall` read the **target** `.o` to answer "what does
this vtable contain," `/data-diff` answers "**where does my decompiled build
diverge from the target**" — slot by slot, byte by byte.

## Arguments

`$ARGUMENTS`

- First positional: the data symbol (mangled, e.g. `__vt__8FilePath`, `__RTTI__9Character`, or a string/array symbol).
- `--unit UNIT` — objdiff unit (e.g. `main/App`, `system/char/Character`). Usually needed for data symbols, which are local to a translation unit.

## Steps

1. **Run the data diff:**

   ```bash
   bin/objdiff-cli diff -p . -u <unit> "$0" -f json-pretty --include-data
   ```

   If you don't know the unit, find which `.o` defines the symbol:

   ```bash
   nm build/SZBE69_B8/obj/<unit>.o | grep '__vt__8FilePath'
   ```

2. **Read `data_diff.relocations`** — the actionable part for vtables/pointer
   tables. Each entry is one pointer slot:

   ```jsonc
   { "offset": 12, "size": 4, "kind": "equal",
     "target_symbol": "Print__6StringFPCc" }
   ```

   - `target_symbol` — function/symbol the **target** slot points to.
   - `base_target_symbol` — where the **decompiled** build points, shown only
     when it differs. `kind: "replace"` + a `base_target_symbol` means **this slot
     resolves to the wrong function** — the highest-signal vtable mismatch.
   - A slot present only in the base build appears as `kind: "insert"` with an
     empty `target_symbol` and a `base_target_symbol` (an extra/misordered entry).
   - `addend` / `base_addend` flag a pointer-into-symbol offset mismatch.

3. **Read `data_diff.segments`** for raw (non-pointer) bytes — string contents,
   enum tables, packed structs:

   ```jsonc
   { "offset": 4, "size": 8, "kind": "replace",
     "bytes": "00000010...", "base_bytes": "00000020..." }
   ```

   `bytes` is the target side, `base_bytes` the decompiled side (present only when
   they differ). Equal runs carry no bytes. Use this for string-literal typos and
   wrong initializer values.

4. **Act on it.** A `replace` reloc → the source references the wrong
   function/global, or the class header has the wrong virtual-function order (use
   `/vtable CLASS` to see the full target layout and fix ordering). A `replace`
   segment → wrong literal/init value at that offset.

## Example

```
$ bin/objdiff-cli diff -p . -u main/App "__vt__8FilePath" -f json-pretty --include-data
  data_diff.relocations:
    +0x00  equal  __RTTI__8FilePath
    +0x08  equal  __dt__8FilePathFv
    +0x0c  equal  Print__6StringFPCc      # slot resolves identically on both sides
```

A 100% vtable shows every slot `equal`. When a slot differs you get e.g.
`+0x0c replace  target=Print__6StringFPCc  base=Print__8FilePathFv` — the
decompiled build wired slot 0x0c to the wrong method.

## When to Use

- A vtable/RTTI/data symbol is stuck below 100% and you need to know *which* slot.
- Verifying a header's virtual-function order produced the right compiled vtable
  (pair with `/vtable` for the authoritative target layout).
- A string-pool or static-initializer data symbol mismatches and you need the
  differing bytes.

## Tips

- `--include-data` is a no-op on code symbols (no `data_diff` emitted), so it's
  safe to add when unsure whether a symbol is code or data.
- For the vtable *slot ↔ asm offset* mapping (`lwz r12, OFF(r12)` → slot index),
  use `/resolve-vcall`; this skill is about the target-vs-base divergence.
- Full schema (including the instruction branch graph): `docs/decomp/objdiff-json-extensions.md`.
