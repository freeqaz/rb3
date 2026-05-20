#!/usr/bin/env python3
"""Dump MWCC vtables from RB3 (SZBE69_B8) `.o` files.

Two modes (one process, peek-argv routing for DC3-shaped CLI):

    dump_vtable.py CLASS [--obj PATH] [--unit UNIT] [--json] [--demangle]
    dump_vtable.py resolve CLASS SUB_OFFSET SLOT [...] [--json] [--demangle]

Data source per docs/plans/vtable-skill.md §1(a): read `.rela.data`
relocations from `build/SZBE69_B8/obj/<unit>.o`. The vtable symbol is
`__vt__<len><class>`; sub-tables are delimited by relocations against
`__RTTI__<len><class>`. The 4 bytes immediately after each RTTI reloc
hold the literal `offset_to_top` (signed). Slot 0 of each sub-table is
the first function relocation after the 8-byte header.

Slot encoding for `resolve`: `slot >= 100` is treated as a byte offset
(divided by 4); smaller values are slot indices.
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import re
import sys
from dataclasses import dataclass, field, asdict
from typing import Iterable

try:
    from elftools.elf.elffile import ELFFile
except ImportError:
    sys.stderr.write(
        "error: pyelftools not installed (pip install -r requirements.txt)\n"
    )
    sys.exit(5)


OBJ_ROOT = "build/SZBE69_B8/obj"

# ---------- mangling helpers ----------

THUNK_RE = re.compile(r"^(?:@-?\d+)+@")


def class_to_vt_symbol(class_name: str) -> str:
    """`RndDrawable` → `__vt__11RndDrawable`. Accepts mangled form verbatim."""
    if class_name.startswith("__vt__"):
        return class_name
    return f"__vt__{len(class_name)}{class_name}"


def class_to_rtti_symbol(class_name: str) -> str:
    if class_name.startswith("__vt__"):
        # Recover class from `__vt__<len><class>` (length-prefix scheme).
        cls = _strip_length_prefix(class_name[len("__vt__"):])
        return f"__RTTI__{len(cls)}{cls}"
    return f"__RTTI__{len(class_name)}{class_name}"


def _strip_length_prefix(s: str) -> str:
    """Parse a single length-prefixed name. `11RndDrawable` → `RndDrawable`."""
    m = re.match(r"^(\d+)", s)
    if not m:
        return s
    n = int(m.group(1))
    return s[m.end():m.end() + n]


def strip_thunk(sym: str) -> tuple[str, str | None]:
    """Strip leading `@N@` / `@N@M@...` groups. Returns (underlying, thunk_prefix or None)."""
    m = THUNK_RE.match(sym)
    if not m:
        return sym, None
    return sym[m.end():], m.group(0)


def pretty_demangle(sym: str) -> str:
    """Tiny pretty-printer: extract `Class::method` from MWCC mangling.

    Not a full demangler; just the length-prefix unwrap that makes vtable
    listings skim-readable. Falls back to the raw symbol on any surprise.
    """
    underlying, thunk = strip_thunk(sym)
    body = underlying
    # Special-case ctor/dtor: `__dt__9CharacterFv` -> `Character::~Character`
    if body.startswith(("__dt__", "__ct__")):
        kind = body[2:4]
        rest = body[6:]
        m = re.match(r"^(\d+)", rest)
        if not m:
            return sym
        n = int(m.group(1))
        cls = rest[m.end():m.end() + n]
        prefix = "~" if kind == "dt" else ""
        out = f"{cls}::{prefix}{cls}"
        return f"{thunk}{out}" if thunk else out
    # General: `Method__9CharacterFv` -> `Character::Method`
    m = re.match(r"^([A-Za-z_][\w$]*)__(\d+)(.+)$", body)
    if not m:
        return sym
    method, n_str, tail = m.group(1), m.group(2), m.group(3)
    n = int(n_str)
    if len(tail) < n:
        return sym
    cls = tail[:n]
    out = f"{cls}::{method}"
    return f"{thunk}{out}" if thunk else out


# ---------- vtable parsing ----------

@dataclass
class Slot:
    slot: int               # 0-based within sub-table
    offset_in_vt: int       # byte offset within the whole vtable symbol
    symbol: str             # mangled function symbol (may be a thunk)
    is_thunk: bool
    thunk_prefix: str | None
    underlying_symbol: str  # symbol with thunk prefix stripped

    def as_dict(self, demangle: bool) -> dict:
        d = asdict(self)
        if demangle:
            d["demangled"] = pretty_demangle(self.symbol)
            d["underlying_demangled"] = pretty_demangle(self.underlying_symbol)
        return d


@dataclass
class SubTable:
    index: int
    offset_in_vt: int       # byte offset of RTTI header within the vtable
    offset_to_top: int      # signed; primary is 0
    sub_object_offset: int  # = -offset_to_top
    slots: list[Slot] = field(default_factory=list)

    @property
    def is_primary(self) -> bool:
        return self.offset_to_top == 0

    def as_dict(self, demangle: bool) -> dict:
        return {
            "index": self.index,
            "offset_in_vt": self.offset_in_vt,
            "offset_to_top": self.offset_to_top,
            "sub_object_offset": self.sub_object_offset,
            "is_primary": self.is_primary,
            "slot_count": len(self.slots),
            "slots": [s.as_dict(demangle) for s in self.slots],
        }


@dataclass
class VTable:
    class_name: str
    vt_symbol: str
    obj_path: str
    section_name: str
    data_offset: int        # st_value: byte offset of vt in section
    total_size: int         # st_size
    sub_tables: list[SubTable] = field(default_factory=list)

    def as_dict(self, demangle: bool) -> dict:
        return {
            "class": self.class_name,
            "vtable_symbol": self.vt_symbol,
            "obj_file": self.obj_path,
            "section": self.section_name,
            "data_offset": self.data_offset,
            "total_size_bytes": self.total_size,
            "sub_tables": [st.as_dict(demangle) for st in self.sub_tables],
        }


def parse_vtable(obj_path: str, class_name: str) -> VTable:
    vt_sym_name = class_to_vt_symbol(class_name)
    rtti_sym_name = class_to_rtti_symbol(class_name)
    # Resolve display class name when caller passed `__vt__...`.
    display_class = class_name
    if class_name.startswith("__vt__"):
        display_class = _strip_length_prefix(class_name[len("__vt__"):])

    with open(obj_path, "rb") as f:
        elf = ELFFile(f)
        symtab = elf.get_section_by_name(".symtab")
        if symtab is None:
            raise VTError(f"no .symtab in {obj_path}", exit_code=5)
        vt_sym = None
        for sym in symtab.iter_symbols():
            if sym.name == vt_sym_name:
                vt_sym = sym
                break
        if vt_sym is None:
            raise VTError(
                f"no {vt_sym_name} symbol in {obj_path}", exit_code=3
            )

        shndx = vt_sym["st_shndx"]
        if not isinstance(shndx, int):
            raise VTError(
                f"{vt_sym_name} has special section index {shndx!r}",
                exit_code=4,
            )
        section = elf.get_section(shndx)
        sec_name = section.name
        rela_sec = elf.get_section_by_name(f".rela{sec_name}")
        if rela_sec is None:
            raise VTError(
                f"no .rela{sec_name} in {obj_path}; cannot read vtable slots",
                exit_code=4,
            )

        base = vt_sym["st_value"]
        size = vt_sym["st_size"]
        end = base + size
        sec_data = section.data()

        # Collect (offset, slot_symbol) for relocations within the vtable
        # range. r_offset is section-relative.
        relocs: list[tuple[int, str]] = []
        for r in rela_sec.iter_relocations():
            off = r["r_offset"]
            if base <= off < end:
                slot_sym = symtab.get_symbol(r["r_info_sym"])
                relocs.append((off, slot_sym.name))
        relocs.sort(key=lambda t: t[0])

        if not relocs or relocs[0][0] != base or relocs[0][1] != rtti_sym_name:
            raise VTError(
                f"{vt_sym_name} in {obj_path}:{sec_name}+0x{base:x} does not "
                f"begin with a relocation against {rtti_sym_name}; refusing "
                f"to guess",
                exit_code=4,
            )

        # Walk: every reloc whose target is the RTTI symbol marks a sub-table
        # header. The 4 bytes after it are the literal `offset_to_top`. All
        # other relocs are slot function pointers.
        sub_tables: list[SubTable] = []
        cur: SubTable | None = None
        for i, (off, name) in enumerate(relocs):
            rel_off = off - base
            if name == rtti_sym_name:
                ott_bytes = sec_data[off + 4:off + 8]
                offset_to_top = int.from_bytes(ott_bytes, "big", signed=True)
                cur = SubTable(
                    index=len(sub_tables),
                    offset_in_vt=rel_off,
                    offset_to_top=offset_to_top,
                    sub_object_offset=-offset_to_top,
                )
                sub_tables.append(cur)
                continue
            if cur is None:
                raise VTError(
                    f"slot relocation at +0x{rel_off:x} precedes any RTTI "
                    f"header in {vt_sym_name}",
                    exit_code=4,
                )
            underlying, thunk = strip_thunk(name)
            cur.slots.append(Slot(
                slot=len(cur.slots),
                offset_in_vt=rel_off,
                symbol=name,
                is_thunk=thunk is not None,
                thunk_prefix=thunk,
                underlying_symbol=underlying,
            ))

        return VTable(
            class_name=display_class,
            vt_symbol=vt_sym_name,
            obj_path=obj_path,
            section_name=sec_name,
            data_offset=base,
            total_size=size,
            sub_tables=sub_tables,
        )


# ---------- obj-file discovery ----------

class VTError(Exception):
    def __init__(self, msg: str, *, exit_code: int = 2):
        super().__init__(msg)
        self.exit_code = exit_code


_VT_INDEX_CACHE: dict[str, str] | None = None


def _scan_vt_index() -> dict[str, str]:
    """Build `{__vt__sym -> obj_path}` index by scanning every .o in OBJ_ROOT."""
    global _VT_INDEX_CACHE
    if _VT_INDEX_CACHE is not None:
        return _VT_INDEX_CACHE
    index: dict[str, str] = {}
    for path in glob.iglob(f"{OBJ_ROOT}/**/*.o", recursive=True):
        try:
            with open(path, "rb") as f:
                elf = ELFFile(f)
                symtab = elf.get_section_by_name(".symtab")
                if symtab is None:
                    continue
                for sym in symtab.iter_symbols():
                    if sym.name.startswith("__vt__"):
                        index.setdefault(sym.name, path)
        except Exception:
            continue
    _VT_INDEX_CACHE = index
    return index


def _strip_known_prefixes(name: str) -> list[str]:
    """Heuristic candidates: `RndDrawable` → [`RndDrawable`, `Drawable`, `Draw`].
    Order: most specific first."""
    cands = [name]
    for prefix in ("Rnd", "Ham", "Ui"):
        if name.startswith(prefix) and len(name) > len(prefix):
            tail = name[len(prefix):]
            cands.append(tail)
            # Also drop trailing "able"/"Base" etc. — too fragile; keep tail only.
    return cands


def find_obj_for_class(
    class_name: str, *, obj: str | None = None, unit: str | None = None
) -> str:
    """Return the .o path that defines `__vt__<len><class>`."""
    vt_sym = class_to_vt_symbol(class_name)
    if obj:
        if not os.path.isfile(obj):
            raise VTError(f"--obj path does not exist: {obj}", exit_code=2)
        return obj
    if unit:
        unit = unit[len("main/"):] if unit.startswith("main/") else unit
        path = f"{OBJ_ROOT}/{unit}.o"
        if not os.path.isfile(path):
            raise VTError(f"--unit resolves to missing file: {path}", exit_code=2)
        return path

    # Class-named glob first (cheap, covers `Character` → `Character.o`,
    # `RndDrawable` → `Drawable.o` if MWCC ever did that…)
    display_class = class_name
    if class_name.startswith("__vt__"):
        display_class = _strip_length_prefix(class_name[len("__vt__"):])
    candidates = _strip_known_prefixes(display_class)
    for cand in candidates:
        matches = glob.glob(f"{OBJ_ROOT}/**/{cand}.o", recursive=True)
        for m in matches:
            try:
                with open(m, "rb") as f:
                    elf = ELFFile(f)
                    symtab = elf.get_section_by_name(".symtab")
                    if symtab is None:
                        continue
                    for sym in symtab.iter_symbols():
                        if sym.name == vt_sym:
                            return m
            except Exception:
                pass

    # Full index fallback.
    index = _scan_vt_index()
    if vt_sym in index:
        return index[vt_sym]

    # Helpful diagnostic: list near-miss vtable symbols.
    near = sorted(s for s in index if display_class in s)[:10]
    hint = ""
    if near:
        hint = "\nNear matches:\n  " + "\n  ".join(near)
    raise VTError(
        f"no {vt_sym} found in any {OBJ_ROOT}/**/*.o{hint}",
        exit_code=3,
    )


# ---------- presentation ----------

def fmt_vtable_text(vt: VTable, *, demangle: bool) -> str:
    lines: list[str] = []
    lines.append(f"Class:    {vt.class_name}")
    lines.append(
        f"Vtable:   {vt.vt_symbol} ({vt.total_size} bytes, "
        f"{vt.obj_path}:{vt.section_name}+0x{vt.data_offset:02x})"
    )
    lines.append(f"Sub-tables: {len(vt.sub_tables)}")
    for st in vt.sub_tables:
        kind = "primary" if st.is_primary else "secondary"
        lines.append("")
        lines.append(
            f"Sub-table {st.index}: offset_to_top={st.offset_to_top} ({kind}), "
            f"sub-object offset +{st.sub_object_offset}, {len(st.slots)} slot(s), "
            f"header at +0x{st.offset_in_vt:02x}"
        )
        for slot in st.slots:
            line = f"  [{slot.slot:3d}] +0x{slot.offset_in_vt:02x}  {slot.symbol}"
            if demangle:
                line += f"   # {pretty_demangle(slot.symbol)}"
            lines.append(line)
    return "\n".join(lines)


def fmt_slot_text(
    vt: VTable, st: SubTable, slot: Slot, *, demangle: bool
) -> str:
    kind = "primary" if st.is_primary else "secondary"
    lines = [
        f"Symbol: {slot.symbol}",
        f"Vtable: {vt.vt_symbol} ({vt.total_size} bytes,",
        f"        {vt.obj_path}:{vt.section_name}+0x{vt.data_offset:02x})",
        f"Slot {slot.slot} of {kind} sub-table (offset_to_top={st.offset_to_top})",
    ]
    if slot.is_thunk:
        lines.append(
            f"Thunk: {slot.thunk_prefix}  underlying: {slot.underlying_symbol}"
        )
    if demangle:
        lines.append(f"Demangled: {pretty_demangle(slot.symbol)}")
        if slot.is_thunk:
            lines.append(
                f"Underlying demangled: {pretty_demangle(slot.underlying_symbol)}"
            )
    return "\n".join(lines)


# ---------- commands ----------

def _select_subtable(vt: VTable, sub_object_offset: int) -> SubTable:
    for st in vt.sub_tables:
        if st.sub_object_offset == sub_object_offset:
            return st
    avail = ", ".join(
        f"{st.sub_object_offset} ({'primary' if st.is_primary else 'secondary'})"
        for st in vt.sub_tables
    )
    raise VTError(
        f"no sub-object at offset {sub_object_offset} for {vt.class_name}; "
        f"available offsets: [{avail}]",
        exit_code=2,
    )


def cmd_dump(args: argparse.Namespace) -> int:
    obj_path = find_obj_for_class(args.class_name, obj=args.obj, unit=args.unit)
    vt = parse_vtable(obj_path, args.class_name)

    if args.offset is not None:
        offset = parse_int(args.offset)
        sub_offset = parse_int(args.sub_offset) if args.sub_offset is not None else 0
        st = _select_subtable(vt, sub_offset)
        # Match the resolve subcommand convention: <100 = slot index,
        # >=100 = live-vptr byte offset (divide by 4).
        if offset >= 100:
            if offset % 4 != 0:
                raise VTError(
                    f"--offset {offset} (0x{offset:x}) is not 4-aligned",
                    exit_code=2,
                )
            slot_idx = offset // 4
        else:
            slot_idx = offset
        matching = [s for s in st.slots if s.slot == slot_idx]
        if not matching:
            raise VTError(
                f"slot {offset} not found in sub-table at sub-object offset "
                f"+{sub_offset}",
                exit_code=2,
            )
        if args.json:
            print(json.dumps(matching[0].as_dict(args.demangle), indent=2))
        else:
            print(fmt_slot_text(vt, st, matching[0], demangle=args.demangle))
        return 0

    if args.json:
        print(json.dumps(vt.as_dict(args.demangle), indent=2))
    else:
        print(fmt_vtable_text(vt, demangle=args.demangle))
    return 0


def cmd_resolve(args: argparse.Namespace) -> int:
    obj_path = find_obj_for_class(args.class_name, obj=args.obj, unit=args.unit)
    vt = parse_vtable(obj_path, args.class_name)

    sub_offset = parse_int(args.sub_object_offset)
    raw_slot = parse_int(args.slot)
    if raw_slot < 0:
        raise VTError(f"slot must be non-negative, got {raw_slot}", exit_code=2)

    st = _select_subtable(vt, sub_offset)

    # >= 100 → treat as a live-vptr byte offset (the form an agent reads
    # straight off `lwz rN, OFF(rM)` in asm). MWCC vptrs point past the
    # 8-byte header, so live-vptr offset OFF == slot OFF/4.
    if raw_slot >= 100:
        if raw_slot % 4 != 0:
            raise VTError(
                f"slot byte-offset {raw_slot} (0x{raw_slot:x}) is not 4-aligned",
                exit_code=2,
            )
        slot_idx = raw_slot // 4
    else:
        slot_idx = raw_slot

    if slot_idx < 0 or slot_idx >= len(st.slots):
        raise VTError(
            f"slot {raw_slot} out of range; {'primary' if st.is_primary else 'secondary'} "
            f"sub-table has {len(st.slots)} function slots (slot 0 starts at "
            f"offset +0x08 within the sub-table)",
            exit_code=2,
        )

    slot = st.slots[slot_idx]
    if args.json:
        out = {
            "vtable": vt.as_dict(args.demangle),
            "sub_table_index": st.index,
            "slot": slot.as_dict(args.demangle),
        }
        # Trim slots list from vtable dump to keep JSON compact for resolve.
        for s in out["vtable"]["sub_tables"]:
            s.pop("slots", None)
        print(json.dumps(out, indent=2))
    else:
        print(fmt_slot_text(vt, st, slot, demangle=args.demangle))
    return 0


# ---------- argparse plumbing ----------

def parse_int(s: str) -> int:
    """Accept decimal or 0x-prefixed hex."""
    s = s.strip()
    if s.startswith(("0x", "0X", "-0x", "-0X")):
        return int(s, 16)
    return int(s, 10)


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="dump_vtable.py",
        description="Dump MWCC vtables from RB3 .o files.",
    )
    sub = p.add_subparsers(dest="cmd")

    pd = sub.add_parser("dump", help="Dump a class vtable (default).")
    pd.add_argument("class_name", help="Class name or mangled __vt__... symbol")
    pd.add_argument("--obj", help="Explicit .o file path")
    pd.add_argument("--unit", help="objdiff-style unit, e.g. system/char/Character")
    pd.add_argument("--offset", help="Slot index (<100) or byte offset (>=100); show only that slot")
    pd.add_argument("--sub-offset", dest="sub_offset", default="0",
                    help="Sub-object offset for --offset lookup (default 0)")
    pd.add_argument("--json", action="store_true", help="Emit JSON")
    pd.add_argument("--demangle", action="store_true",
                    help="Pretty-print Class::method (length-prefix unwrap, not a full demangler)")
    pd.set_defaults(func=cmd_dump)

    pr = sub.add_parser("resolve", help="Resolve a single vcall slot.")
    pr.add_argument("class_name")
    pr.add_argument("sub_object_offset",
                    help="Byte offset from `this` to the vtable load (e.g. 0, 0x20)")
    pr.add_argument("slot",
                    help="Slot index, OR byte offset >=100 (auto-divided by 4)")
    pr.add_argument("--obj")
    pr.add_argument("--unit")
    pr.add_argument("--json", action="store_true")
    pr.add_argument("--demangle", action="store_true")
    pr.set_defaults(func=cmd_resolve)

    return p


def main(argv: list[str]) -> int:
    # Peek argv[1] so `dump_vtable.py Character` (no subcommand) routes to dump.
    if len(argv) >= 1 and argv[0] not in ("dump", "resolve", "-h", "--help"):
        argv = ["dump"] + argv

    parser = build_parser()
    args = parser.parse_args(argv)
    if not hasattr(args, "func"):
        parser.print_help()
        return 2

    try:
        return args.func(args)
    except VTError as e:
        sys.stderr.write(f"error: {e}\n")
        return e.exit_code


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
