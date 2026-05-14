# resolve-vcall skill — RB3 design

Status: design only, no implementation yet.
Related: [docs/plans/vtable-skill.md](vtable-skill.md) (sibling plan).

## 0. Revision needed before implementation — data source conflict with vtable plan

This plan as written reads vtable bytes from `band_r_wii.elf` (the Bank 5 debug
ELF). The sibling vtable-skill plan empirically discovered that **the Bank 5
debug ELF's vtable layouts differ from the SZBE69_B8 (Bank 8) build target** —
e.g. `__vt__9Character` is 604 bytes in the debug ELF vs 628 bytes in
`build/SZBE69_B8/obj/system/char/Character.o`. Reading vtables from Bank 5
would silently return wrong slot indices for any class that changed layouts
between banks, with no easy way to detect it.

**Before implementing this skill, revise § 3 step 3 onward to:**

- Read the `__vt__N<class>` symbol's bytes and relocations from
  `build/SZBE69_B8/obj/<unit>.o` via pyelftools (the approach decided in
  vtable-skill.md § 1, option (a)). This is the authoritative source for the
  build we're matching.
- Keep the Bank 5 DWARF only for class-hierarchy enrichment (inheritance
  offsets, base-class names) — that data IS consistent across banks since
  the source declarations don't change, only the generated vtable bytes do.
- Recover sub-object boundaries from the `.o` relocation walk (consecutive
  `__RTTI__N<class>` relocs mark sub-table boundaries) rather than from
  parsing raw bytes out of `.data`.

The rest of the plan (input parsing, output shape, failure modes, worked
examples) stands. The worked example offsets/symbols may need refresh once
implementation reads from Bank 8 `.o` files — the *algorithm* doesn't change,
only the byte source does.

---

## 1. Relationship to the `/vtable` skill

Both skills must answer "what is the per-slot, per-sub-object layout of class X's vtable?".
`/vtable` returns the *whole table*; `resolve-vcall` returns *one slot of one sub-object*. They
share:

- DWARF walking to find `DW_TAG_class_type{name="X"}` and its `DW_TAG_inheritance` records
  (for sub-object offsets and base class identities).
- ELF symbol-table lookup of `__vt__N<Class>` and address-to-symbol resolution for the
  function pointers stored inside that vtable's `.data` bytes.
- Demangling of MWCC names for human-readable output.

**Decision: keep them in a single `scripts/dump_vtable.py`** with `resolve` as a subcommand
(same shape as DC3 today). Rationale:

- DC3 took this path because the COFF parser was expensive to set up once. For RB3 the same
  argument applies even more strongly: parsing DWARF + symtab takes ~3-5 seconds for the full
  ELF, and we want resolve calls to be cheap iterations on top of an already-loaded model.
  If the two were separate scripts they'd each rebuild the same caches.
- The CLI surface mirrors DC3 exactly, which keeps muscle memory across the sister repos
  (see docs/SYNC_WITH_DC3.md).
- The skill wrapper (`/resolve-vcall`) stays trivial: invoke `python3 scripts/dump_vtable.py
  resolve $ARGUMENTS` and pretty-print.

The downside DC3 hit — that `resolve` and the default subcommand share global argparse state
and need careful routing — is mild. We replicate DC3's "peek argv[1] before argparse" trick.

## 2. Input parsing

`<class_name> <sub_object_offset> <vtable_slot>`, identical to DC3.

- `class_name`: the most-derived class (e.g., `CharEyes`, `RndDrawable`, `GemTrackDir`).
- `sub_object_offset`: byte offset from `this` to the vtable load. Accept `0`, `8`, `0x8`,
  `0x20`. Hex with `0x` prefix or decimal.
- `vtable_slot`:
  - **Keep DC3's `>= 100` → byte-offset / 4 rule.** It's the only ergonomic on-ramp from raw
    asm where the user has `lwz rN, 0x1c(vtable)` in front of them. The threshold is safe —
    no realistic Milo vtable has 100+ virtual slots (largest in our sample, `GemTrackDir`, is
    ~228 slot-bytes = 57 slots, and even RndDir+BandTrack composites are well under 100).
  - Negative integers reject early. Non-integer reject.

## 3. Resolution algorithm with DWARF

### Single-inheritance case (most common)

1. **Locate the class DIE.**
   - Read `.debug_info` once (cache to `/tmp/claude/dwarf_rb3.pkl` keyed by ELF mtime).
   - Iterate `DW_TAG_class_type` / `DW_TAG_structure_type` looking for `DW_AT_name ==
     class_name`. Index by name on first build for O(1) future lookups.

2. **Walk inheritance to a flat sub-object list.**
   - Collect every `DW_TAG_inheritance` child of the class DIE.
   - For each: read `DW_AT_data_member_location` (DWARF location expression, usually
     `DW_OP_plus_uconst N` — extract `N` as the sub-object offset) and follow `DW_AT_type` to
     the base class DIE for the base name.
   - Treat `DW_AT_virtuality == 1` on the inheritance as "virtual base"; record but do not
     fail. Virtual bases work the same for resolution because MWCC stores the virtual-base
     vptr at the same offset shown in `data_member_location` of the *class* DIE — see the
     `RndDrawable` worked example below.

3. **Locate the linked vtable symbol.**
   - In the ELF symtab, look up `__vt__<len><class_name>` (MWCC mangling: ASCII length prefix
     + name). For `CharEyes` that's `__vt__8CharEyes`; for `RndDrawable` it's
     `__vt__11RndDrawable`.
   - The symbol's `st_value` is the runtime address; `st_size` is the table length in bytes.

4. **Parse the `__vt__` blob into sub-object sub-tables.**
   - Read `st_size` bytes from `.data` starting at `st_value` (use pyelftools or `readelf
     -x`).
   - The blob is a sequence of sub-tables, one per non-empty sub-object in the inheritance
     graph. Each sub-table starts with an **8-byte header**:
     - `+0`: 32-bit pointer to `__RTTI__N<class>` (the most-derived class's RTTI, repeated
       per sub-table).
     - `+4`: signed 32-bit `offset_to_top` — the negated sub-object offset. The primary
       sub-table has `0`; secondary sub-tables have `-8`, `-32`, etc. matching the
       DWARF inheritance offsets.
   - After the header, consecutive 4-byte slots are function pointers until either:
     - the next sub-table header is detected (next word is a known `__RTTI__` address), or
     - `st_size` is reached.

5. **Match the query sub-object offset to a sub-table.**
   - For each sub-table, compute `sub_object_offset = -offset_to_top`. Find the one matching
     the query.
   - If no match: error path (d) below.

6. **Index into the matched sub-table at the requested slot.**
   - Slot 0 = first function entry after the 8-byte header.
   - Resolve the 32-bit address to a symbol via the symtab (preload an `addr -> name` map
     once; multiple symbols can share an address — prefer FUNC, then prefer the longer/
     non-WEAK one, then any).
   - Demangle through `c++filt` (MetroWorks uses GNU-compatible-ish mangling: `c++filt -n`
     handles most; have a manual fallback for the `__ct__` / `__dt__` / `@N@` thunk forms).
   - Note thunk: if the resolved name is `@N@Foo__C...`, the entry is the adjusting thunk
     installed for non-primary sub-objects; report both the thunk name *and* the underlying
     `Foo__C...` symbol (strip the `@N@` prefix).

### Multiple-inheritance case

Same algorithm; step 4 finds multiple sub-tables and step 5 picks the right one by offset.
For `CharEyes` (inherits RndHighlightable @ 0, CharWeightable @ 8, CharPollable @ 32) the
single `__vt__8CharEyes` symbol contains three sub-tables back-to-back with `offset_to_top`
values `0`, `-8`, `-32`.

### Virtual-inheritance case (e.g., RndDrawable)

`class RndDrawable : public virtual RndHighlightable` produces a class DIE with **both**:
- A `DW_TAG_inheritance` of RndHighlightable at offset 0 with `DW_AT_virtuality: 1`.
- A `DW_TAG_member` of *type* RndHighlightable at offset 96 (the actual storage of the
  virtual base inside an RndDrawable instance).

For vtable resolution we use the `DW_TAG_inheritance` offset (`0`), because that's where the
vtable pointer load lands in the assembly. The actual virtual-base-storage offset (96) is
irrelevant to vcall resolution — it's a member-access offset, not a vptr offset.

## 4. Output shape

On success (stdout, agent-readable plain text):

```
Resolved: RndDrawable::Copy(const Hmx::Object*, Hmx::Object::CopyType)
Symbol:   Copy__11RndDrawableFPCQ23Hmx6ObjectQ33Hmx6Object8CopyType
Vtable:   __vt__11RndDrawable   (200 bytes, address 0x80d4c0c0)
Sub-object: offset 0, base RndDrawable (primary)
Slot:     [5] at +0x1c (within sub-table starting at +0x00)
Sub-table function count: 22
Confidence: high

All slots in this sub-table:
  [ 0] +0x08  RndDrawable::ClassName()
  [ 1] +0x0c  RndDrawable::SetType(Symbol)
  [ 2] +0x10  RndDrawable::Handle(DataArray*, bool)
  [ 3] +0x14  RndDrawable::SyncProperty(DataNode&, DataArray*, int, PropOp)
  [ 4] +0x18  RndDrawable::Save(BinStream&)
  [ 5] +0x1c  RndDrawable::Copy(...)        <<
  ...
```

**Confidence model on RB3.** MWCC does *not* perform ICF the way MSVC for Xenon does (see
docs/decomp/patterns/verifiable-icf.md). The Wii build has thunks (`@8@__dt__8CharEyesFv`)
and weak symbols, but distinct functions get distinct addresses; identical machine code is
not deduplicated across symbols. Therefore on RB3:

- `confidence: high` whenever the address resolves to exactly one global/weak FUNC symbol
  whose mangled name decodes cleanly.
- `confidence: medium` only in two narrow cases:
  1. Address resolves to multiple co-located symbols (rare; usually means a section start
     overlap) and we picked one heuristically.
  2. Demangling failed — we emit the raw mangled name but can't pretty-print it.
- The `confidence: low` tier from DC3 (ICF positional guessing) does not apply.

Recommendation: emit `confidence: high` by default and just *omit* the field unless it's
medium. Keeps output noise down.

## 5. Failure modes

| Condition | Exit | Message |
|---|---|---|
| (a) Class not found in DWARF | 2 | `Error: no DW_TAG_class_type named '<X>' in band_r_wii.elf` + suggest `--list` or grep hint |
| (b) Sub-object offset doesn't match any sub-table | 2 | `Error: no sub-object at offset <N> for <X>` followed by `Available offsets: [0 (RndDrawable, primary), 96 (RndHighlightable, virtual base)]` from the DWARF inheritance walk |
| (c) Slot past end of sub-table | 2 | `Error: slot <N> out of range; sub-table has <K> function slots (slot 0 starts at offset +0x08 within the sub-table)` |
| (d) `__vt__N<X>` symbol missing | 3 | `Error: no __vt__<len><X> symbol in band_r_wii.elf — class is likely abstract or not instantiated. Closest matches: __vt__N<sibling>...` |
| (e) Vtable bytes don't parse (no recognizable RTTI header) | 4 | `Error: __vt__<X> at 0x... does not begin with an __RTTI__ pointer; refusing to guess` |
| (f) ELF unreadable / DWARF malformed | 5 | propagate pyelftools exception with file path context |

All errors should print the available-vtable list when relevant (mirrors DC3's diagnostic
behavior) so the agent can self-correct its query.

## 6. Worked example: `resolve-vcall RndDrawable 0 5`

> **Refreshed 2026-05-12** to read from Bank 8 `.o` files per § 0. The
> earlier version of this section used Bank 5 debug-ELF runtime addresses
> (`0x80d4c0c0`) and Bank-5-specific bytes (vtable size 200, sub-table
> boundary at +0x60, secondary `offset_to_top = -0x34`). The Bank 8
> ground truth is different — vtable size 208, primary sub-table ends at
> +0x6c, secondary sub-table is at sub-object offset +32 (`offset_to_top
> = -32`), and there's a third sub-table at offset +64. The algorithm is
> unchanged; only the byte source moved.

**Canonical answer (locked, used as the Step 1.4 success criterion in
[tooling-roadmap.md](tooling-roadmap.md)):**

```
$ python3 scripts/dump_vtable.py resolve RndDrawable 0 5
Resolved: RndDrawable::Copy(const Hmx::Object*, Hmx::Object::CopyType)
Symbol:   Copy__11RndDrawableFPCQ23Hmx6ObjectQ33Hmx6Object8CopyType
Vtable:   __vt__11RndDrawable   (208 bytes, build/SZBE69_B8/obj/system/rndobj/Draw.o:.data+0x08)
Sub-object: offset 0, base RndDrawable (primary)
Slot:     [5] at +0x1c (within sub-table starting at +0x00)
Sub-table function count: 23
```

DWARF for `RndDrawable` (from `band_r_wii.elf`; cross-bank-stable):
```
DW_TAG_class_type  name=RndDrawable  byte_size=0x70
  DW_TAG_inheritance  data_member_location=0  type=<RndHighlightable>  virtuality=virtual
  DW_TAG_member       data_member_location=96 type=<RndHighlightable>  name=RndHighlightable
  DW_TAG_member       data_member_location=8  type=bool                 name=mShowing
  DW_TAG_member       data_member_location=16 type=<Sphere>             name=mSphere
  DW_TAG_member       data_member_location=48 type=float                name=mOrder
```

Symbol: `__vt__11RndDrawable` lives in
`build/SZBE69_B8/obj/system/rndobj/Draw.o` at `.data+0x08`, size 208 bytes.
The 208 bytes break into three sub-tables:

| Sub-table | File range | Bytes | offset_to_top | Sub-object offset | Slot count |
|---|---|---|---|---|---|
| Primary   | `.data+0x08` … `+0x6b` | 100 | `0`           | 0  | 23 |
| Secondary | `.data+0x6c` … `+0xc7` | 92  | `-32` (`0xffffffe0`) | +32 | 21 |
| Tertiary  | `.data+0xc8` … `+0xd7` | 16  | `-64` (`0xffffffc0`) | +64 | 2  |

Primary sub-table's first six slot relocations (from
`readelf -W -r build/SZBE69_B8/obj/system/rndobj/Draw.o`, `.rela.data`):

```
.data+0x08: R_PPC_ADDR32 -> __RTTI__11RndDrawable                                 (header: rtti)
.data+0x0c:                  literal 0x00000000                                   (header: offset_to_top)
.data+0x10: R_PPC_ADDR32 -> ClassName__11RndDrawableCFv                           (slot 0)
.data+0x14: R_PPC_ADDR32 -> SetType__11RndDrawableF6Symbol                        (slot 1)
.data+0x18: R_PPC_ADDR32 -> Handle__11RndDrawableFP9DataArrayb                    (slot 2)
.data+0x1c: R_PPC_ADDR32 -> SyncProperty__11RndDrawableFR8DataNodeP9DataArrayi6PropOp (slot 3)
.data+0x20: R_PPC_ADDR32 -> Save__11RndDrawableFR9BinStream                       (slot 4)
.data+0x24: R_PPC_ADDR32 -> Copy__11RndDrawableFPCQ23Hmx6ObjectQ33Hmx6Object8CopyType (slot 5) <<
```

Step-by-step:

1. Parse arguments: class=`RndDrawable`, offset=`0`, slot=`5` (< 100, so treated as slot
   index, not byte offset).
2. DWARF lookup of `RndDrawable` succeeds; inheritance offsets are `{0}` (the virtual-base
   inheritance record). The byte-size-96 RndHighlightable member is a virtual-base storage
   slot — present in the type but not a vptr-bearing sub-object — so not added to the resolve
   table. (DWARF read from `band_r_wii.elf`; the inheritance graph is consistent across
   banks, only generated vtable bytes differ.)
3. Symtab: `__vt__11RndDrawable` -> `Draw.o:.data+0x08`, size 208.
4. Read 208 bytes from `Draw.o`'s `.data`; walk `.rela.data` for relocations whose offset
   falls in `[0x08, 0xd8)`. Identify sub-table boundaries by consecutive
   `__RTTI__11RndDrawable` relocations at `+0x08`, `+0x6c`, `+0xc8`. Read the literal word
   immediately after each as `offset_to_top` (`0`, `-32`, `-64`).
5. Match: query offset `0` matches sub-table with `offset_to_top = 0` → primary sub-table.
6. Slot 5 -> 4-byte word at `Draw.o:.data + 0x08 + 0x08 + 5*4 = +0x24`. The relocation at
   that offset points at `Copy__11RndDrawableFPCQ23Hmx6ObjectQ33Hmx6Object8CopyType`.
7. (No address-to-symbol step needed; `.o` relocations name the symbol directly. This is
   simpler than the Bank-5 runtime-address path the earlier draft used.)
8. Demangle: `RndDrawable::Copy(const Hmx::Object*, Hmx::Object::CopyType)`.

Final output as shown above.

For contrast, `resolve-vcall RndDrawable 32 0` (sub-object offset +32) would land at the
secondary sub-table's slot 0 = `@32@28@__dt__11RndDrawableFv` — an MWCC adjusting thunk.
The output should report both the thunk name and the underlying `__dt__11RndDrawableFv`
(strip the `@N@M@` prefix). The `@32@28@` prefix encodes a two-step `this` adjustment
characteristic of MWCC's MI thunks; the implementation should treat any leading `@…@…@`
(one or more groups) as a thunk marker.

## 7. Open questions / verification needed

1. **Sub-table delimiter heuristic robustness.** I'm assuming each sub-table begins with a
   pointer into a `__RTTI__*` symbol and a small negative `offset_to_top`. Need to confirm
   this against a class with 4+ bases (e.g., `CharEyes` has 3; `GemTrackDir` may have
   more). If any class has a sub-table whose first function entry happens to be in the same
   address range as an RTTI symbol, the heuristic breaks. Mitigation: cross-check by reading
   DWARF inheritance offsets first and pre-computing expected `offset_to_top` values to
   look for.

2. **Virtual inheritance vptr placement.** Verified for RndDrawable that the
   `DW_TAG_inheritance` with `virtuality=1` lives at offset 0 — the vtable load in code uses
   that offset. Not yet verified for classes that *also* have a non-virtual base at offset
   0 (would shift the virtual base's vptr). Would need to find such a class to confirm; may
   not exist in Milo.

3. **Thunk slot semantics.** The `@N@Foo__C...` thunks adjust `this` by `-N` and tail-call
   into `Foo__C`. Confirm that an indirect call hitting that thunk is *semantically* "Foo"
   from the caller's perspective — i.e., we should report `C::Foo`, not the thunk, as the
   resolved function. I believe yes (the thunk's only job is to fix `this`), but worth
   stating.

4. **Caching strategy.** Loading the full DWARF takes a few seconds. Recommendation: pickle
   a `{class_name -> (inheritance_offsets, vt_symbol_addr, vt_bytes)}` index keyed by ELF
   mtime in `/tmp/claude/rb3-vt-index.pkl`. Need to verify this fits in memory (~3000
   classes); if not, switch to a SQLite-backed index in `struct_db.sqlite` (already used by
   `tools/struct_db.py`).

5. **Skill wrapper allowed-tools.** DC3's SKILL.md uses
   `Bash(python3 scripts/dump_vtable.py *)`. RB3's `.claude/skills/` use `Bash(bin/<cmd> *)`
   wrappers more often. Decide whether to add a `bin/resolve-vcall` shim for symmetry with
   `bin/analyze-function`.
