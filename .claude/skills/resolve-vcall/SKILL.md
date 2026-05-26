---
name: resolve-vcall
description: Resolve a single virtual-call slot to its function symbol. Given `class, sub-object offset, slot`, returns the mangled symbol at that slot of the named class's MWCC vtable. Use when objdiff shows a vcall mismatch and you have the three numbers off the asm but don't want to dump the whole vtable.
argument-hint: "CLASS SUB_OBJECT_OFFSET SLOT [--unit UNIT] [--obj PATH] [--json] [--demangle]"
allowed-tools: Bash(python3 scripts/dump_vtable.py *), Read, Grep, Glob
---

# Resolve Vcall

Pinpoint the virtual function that a single `lwz r12, OFF(r12)` instruction calls into. Same backing script as `/vtable` (`scripts/dump_vtable.py`), but returns one slot instead of the whole table.

## Arguments

`$ARGUMENTS`

Positional args (all three required):

1. **CLASS** — the most-derived class (e.g. `RndDrawable`, `Character`, `CharEyes`). Mangled `__vt__11RndDrawable` is also accepted.
2. **SUB_OBJECT_OFFSET** — byte offset from `this` to the vtable load. `0` for the primary table; positive values for secondary bases. Decimal or `0x`-prefixed hex.
3. **SLOT** — slot index (e.g. `5`), or a live-vptr byte offset `>= 100` (e.g. `0x1a0` → divided by 4 → slot 104). Values `< 100` are always slot indices.

Optional:
- `--unit UNIT` / `--obj PATH` — explicit .o lookup (same as `/vtable`).
- `--json` — emit JSON (includes the vtable header and the single resolved slot).
- `--demangle` — pretty-print `Class::method`.

## Steps

1. **Translate the asm.** A typical vcall in MWCC PowerPC is two `lwz`s:

   ```
   lwz   r12, 0x20(r3)     # load vptr from sub-object at this+0x20
   lwz   r12, 0x14(r12)    # call slot at live-vptr offset 0x14
   ```

   - First `lwz` offset (`0x20`) = sub-object offset (pass as second argument).
   - Second `lwz` offset (`0x14`) = live-vptr byte offset from the START of the `__vt__` symbol.

   **Critical ABI detail:** The `__vt__` symbol includes an 8-byte header before the first function slot:
   - `vptr + 0x00` = RTTI pointer (not a function)
   - `vptr + 0x04` = offset_to_top (not a function)
   - `vptr + 0x08` = slot 0
   - `vptr + 0x0c` = slot 1
   - `vptr + 0x14` = **slot 3** (NOT slot 5!)

   Slot index = **(OFF − 8) / 4**.  Example: `lwz r12, 0x14(r12)` → (0x14 − 8) / 4 = **slot 3**.
   The old formula `OFF / 4 = 5` is WRONG — it ignores the 8-byte header.

2. **Resolve.** Pass the 0-based slot index as the third argument:

   ```bash
   python3 scripts/dump_vtable.py resolve '$0' '$1' '$2'
   ```

   Or pass the raw vptr byte offset using `--vptr-offset`:

   ```bash
   python3 scripts/dump_vtable.py resolve '$0' '$1' 0x14 --vptr-offset
   ```

   Example: to look up `lwz r12, 0x14(r12)` on `RndDrawable` (vptr offset 0x14 = slot 3):

   ```
   $ python3 scripts/dump_vtable.py resolve RndDrawable 0 3
   Symbol: SyncProperty__11RndDrawableFR8DataNodeP9DataArrayi6PropOp
   Vtable: __vt__11RndDrawable (208 bytes,
           build/SZBE69_B8/obj/system/rndobj/Draw.o:.data+0x08)
   Slot 3 of primary sub-table (offset_to_top=0)
   ```

   (Alternatively: `resolve RndDrawable 0 0x14 --vptr-offset` gives the same result.)

   To look up `lwz r12, 0x1c(r12)` (vptr offset 0x1c = slot 5):

   ```
   $ python3 scripts/dump_vtable.py resolve RndDrawable 0 5
   Symbol: Copy__11RndDrawableFPCQ23Hmx6ObjectQ33Hmx6Object8CopyType
   Vtable: __vt__11RndDrawable (208 bytes,
           build/SZBE69_B8/obj/system/rndobj/Draw.o:.data+0x08)
   Slot 5 of primary sub-table (offset_to_top=0)
   ```

3. **Read the answer.** `Symbol:` is the mangled function the slot points to. If the slot is a thunk, the output adds:

   ```
   Thunk: @32@28@  underlying: __dt__11RndDrawableFv
   ```

   meaning the slot's literal entry is the adjusting thunk `@32@28@__dt__11RndDrawableFv` but it tail-calls into `__dt__11RndDrawableFv` after fixing `this`. From the caller's perspective the effective callee is the underlying symbol.

## When to Use

- objdiff shows a `r12` offset mismatch on a virtual call and you need the symbol on either side.
- Verifying that a header change moved an override to the slot the binary expects.
- Identifying thunks in MI sub-object vtables (`@N@` and `@N@M@` forms — MWCC's two-step `this` adjustment).

## Errors

| Exit | Meaning |
|---|---|
| 2 | Bad input (sub-object offset doesn't match any sub-table, slot out of range, non-4-aligned byte offset). |
| 3 | `__vt__<class>` symbol not found in any `.o`; class is likely abstract or not instantiated. Near matches are listed in the error message. |
| 4 | Vtable bytes don't parse cleanly (no leading RTTI relocation). |
| 5 | I/O or ELF malformed. |

When sub-object offset is wrong, the error lists all available offsets — match against the first `lwz`'s immediate.

## Tips

- Vtable bytes come from Bank 8 (`build/SZBE69_B8/obj/<unit>.o`), not Bank 5 (`band_r_wii.elf`). Layouts differ between banks.
- MWCC doesn't ICF, so there's no "could be any of N identical functions" ambiguity — slot lookups always give one symbol.
- For the whole table at once, use `/vtable CLASS`.
