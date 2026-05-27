#!/usr/bin/env python3
"""Extract base-side local-variable → stack-offset mappings from MWCC DWARF.

Recompiles a source file with `-sym dwarf-2,full`, parses the resulting
.debug_info + .debug_loc to produce a `{r1_offset: LocalInfo}` table for a
named function. Used by `stack_layout.py --with-names` to label slot rows
with their source variable names.

Standalone usage:
    python3 scripts/analysis/dwarf_locals.py <mangled_symbol>
"""

from __future__ import annotations

import argparse
import io
import os
import re
import shlex
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


# ── Configuration ────────────────────────────────────────────────────────────

CACHE_DIR = Path("/tmp/claude/stack_dwarf")
DEBUG_FLAGS = ["-sym", "dwarf-2,full"]

# PPC32 relocation types we care about
R_PPC_ADDR32 = 1
R_PPC_UADDR32 = 24


# ── Symbol → source resolution ──────────────────────────────────────────────

def _project_root() -> Path:
    return Path(__file__).resolve().parent.parent.parent


def _find_unit_for_symbol(symbol: str, project_dir: Optional[str] = None) -> Optional[tuple[str, Path]]:
    """Look up (unit_name, source_path) for a mangled symbol via report.json."""
    root = Path(project_dir) if project_dir else _project_root()
    report = root / "build" / "SZBE69_B8" / "report.json"
    if not report.exists():
        return None
    import json
    with report.open() as f:
        data = json.load(f)
    for unit in data.get("units", []):
        for fn in unit.get("functions", []):
            if fn.get("name") == symbol:
                unit_name = unit.get("name", "")
                # Strip module-prefix conventions used in this project
                for prefix in ("default/", "main/"):
                    if unit_name.startswith(prefix):
                        unit_name = unit_name[len(prefix):]
                # Search for the .cpp / .c source
                for ext in (".cpp", ".c"):
                    src = root / "src" / (unit_name + ext)
                    if src.exists():
                        return unit_name, src
                return unit_name, root / "src" / (unit_name + ".cpp")
    return None


# ── build.ninja parsing ──────────────────────────────────────────────────────

def cflags_for(src_path: Path, project_dir: Optional[str] = None) -> Optional[list[str]]:
    """Read build.ninja and extract the cflags list for the given source file."""
    root = Path(project_dir) if project_dir else _project_root()
    ninja = root / "build.ninja"
    if not ninja.exists():
        return None

    # Match what ninja calls this source — relative path from project root
    rel = src_path.resolve().relative_to(root.resolve())
    rel_str = str(rel)

    # Each rule block looks like:
    #   build <out>: mwcc <src> | <deps>
    #     mw_version = Wii/1.3
    #     cflags = ... $
    #         ... $
    #         ...
    # We find the build line, then read subsequent indented lines.
    text = ninja.read_text()
    # Search for "build ... : mwcc <rel_str>" optionally with $ continuation
    # The src is the FIRST input after "mwcc". Use a regex that allows line continuations.
    flat = re.sub(r"\$\n\s*", " ", text)
    pat = re.compile(rf"^build\s+(\S+)\s*:\s*mwcc\s+{re.escape(rel_str)}\b", re.MULTILINE)
    m = pat.search(flat)
    if not m:
        return None
    # From the match, scan forward for indented variable assignments.
    start = m.end()
    # Find the next un-indented line (end of rule block)
    block_end = re.search(r"\nbuild\s|\nrule\s|\n[a-zA-Z]", flat[start:])
    block = flat[start:start + (block_end.start() if block_end else len(flat) - start)]
    cflags_match = re.search(r"^\s+cflags\s*=\s*(.+)$", block, re.MULTILINE)
    if not cflags_match:
        return None
    return shlex.split(cflags_match.group(1))


def _mw_version_for(src_path: Path, project_dir: Optional[str] = None) -> str:
    """Extract mw_version for the file (default Wii/1.3)."""
    root = Path(project_dir) if project_dir else _project_root()
    ninja = root / "build.ninja"
    if not ninja.exists():
        return "Wii/1.3"
    rel = str(src_path.resolve().relative_to(root.resolve()))
    flat = re.sub(r"\$\n\s*", " ", ninja.read_text())
    m = re.search(
        rf"^build\s+\S+\s*:\s*mwcc\s+{re.escape(rel)}\b[^\n]*\n((?:\s+\w+\s*=[^\n]*\n)+)",
        flat, re.MULTILINE
    )
    if not m:
        return "Wii/1.3"
    mv = re.search(r"mw_version\s*=\s*(\S+)", m.group(1))
    return mv.group(1) if mv else "Wii/1.3"


# ── Recompile with debug info ───────────────────────────────────────────────

def compile_with_debug(src_path: Path, project_dir: Optional[str] = None) -> Path:
    """Recompile a single source file with DWARF debug info. Cached by mtime.

    Returns the path to the resulting .o.
    """
    root = Path(project_dir) if project_dir else _project_root()
    cflags = cflags_for(src_path, project_dir)
    if cflags is None:
        raise RuntimeError(f"could not find cflags in build.ninja for {src_path}")

    mw_version = _mw_version_for(src_path, project_dir)
    compiler = root / "build" / "compilers" / mw_version / "mwcceppc.exe"
    wibo = root / "build" / "tools" / "wibo"
    if not compiler.exists():
        raise RuntimeError(f"compiler not found: {compiler}")
    if not wibo.exists():
        raise RuntimeError(f"wibo not found: {wibo}")

    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    out_path = CACHE_DIR / (src_path.stem + ".dwarf.o")

    src_mtime = src_path.stat().st_mtime
    if out_path.exists() and out_path.stat().st_mtime >= src_mtime:
        return out_path

    cmd = [str(wibo), str(compiler)] + cflags + DEBUG_FLAGS + [
        "-c", str(src_path), "-o", str(out_path)
    ]
    result = subprocess.run(cmd, cwd=str(root), capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"mwcc compile failed for {src_path}:\n{result.stderr[:1000]}"
        )
    return out_path


# ── PPC32 relocation handling ───────────────────────────────────────────────

def _apply_ppc_relocs_inplace(data: bytearray) -> None:
    """Apply PPC32 R_PPC_ADDR32/UADDR32 relocations to .debug_* sections,
    then neutralize the rela.debug_* section headers so pyelftools accepts
    the file without re-applying them."""
    from elftools.elf.elffile import ELFFile

    elf = ELFFile(io.BytesIO(bytes(data)))
    shoff = elf["e_shoff"]
    shentsize = elf["e_shentsize"]
    symtab = elf.get_section_by_name(".symtab")
    sym_values = [s.entry["st_value"] for s in symtab.iter_symbols()] if symtab else []

    for sec_idx, sec in enumerate(elf.iter_sections()):
        if not sec.name.startswith(".rela.debug_"):
            continue
        target_name = sec.name[len(".rela"):]  # ".rela.debug_info" -> ".debug_info"
        target = elf.get_section_by_name(target_name)
        if target is None or target["sh_type"] == "SHT_NOBITS":
            continue
        target_off = target["sh_offset"]
        for reloc in sec.iter_relocations():
            r_type = reloc["r_info_type"]
            if r_type in (R_PPC_ADDR32, R_PPC_UADDR32):
                r_offset = reloc["r_offset"]
                r_addend = reloc["r_addend"]
                r_sym = reloc["r_info_sym"]
                sym_val = sym_values[r_sym] if r_sym < len(sym_values) else 0
                val = (sym_val + r_addend) & 0xFFFFFFFF
                struct.pack_into(">I", data, target_off + r_offset, val)
        # Neutralize the rela section so pyelftools won't re-apply
        # (sh_type = SHT_NULL at offset +4 in section header entry)
        struct.pack_into(">I", data, shoff + sec_idx * shentsize + 4, 0)


# ── DWARF parsing ───────────────────────────────────────────────────────────

@dataclass
class LocalInfo:
    name: str
    offset: int
    depth: int
    is_param: bool = False


def _read_sleb128(buf, pos):
    val, shift = 0, 0
    while True:
        b = buf[pos]
        pos += 1
        val |= (b & 0x7F) << shift
        shift += 7
        if not (b & 0x80):
            if b & 0x40:
                val |= -(1 << shift)
            return val, pos


def _first_fbreg_offset(raw, off):
    """Parse a DWARF location list, return the first DW_OP_fbreg offset found."""
    pos = off
    while pos < len(raw):
        begin = struct.unpack_from(">I", raw, pos)[0]
        pos += 4
        end = struct.unpack_from(">I", raw, pos)[0]
        pos += 4
        if begin == 0 and end == 0:
            return None
        if begin == 0xFFFFFFFF:
            continue  # base address selection
        ln = struct.unpack_from(">H", raw, pos)[0]
        pos += 2
        if ln >= 1 and raw[pos] == 0x91:  # DW_OP_fbreg
            v, _ = _read_sleb128(raw, pos + 1)
            return v
        pos += ln
    return None


# ── Mangled-symbol → class+method extraction ────────────────────────────────

# Match MWCC mangling: name__<class-len><class>F<args>
_MANGLE_RE = re.compile(r"^(?P<method>.+?)__(?P<rest>\d.+)$")


def parse_mangled_symbol(symbol: str) -> Optional[tuple[str, str]]:
    """Return (class_name, method_name) or None for free functions.

    Handles forms like:
      Relativize__16CharBonesSamplesFP8CharClip → (CharBonesSamples, Relativize)
      __ct__5HelloFv → (Hello, Hello)   (constructor)
      __dt__5HelloFv → (Hello, ~Hello)  (destructor)
    """
    # Constructor / destructor specials
    if symbol.startswith("__ct__") or symbol.startswith("__dt__"):
        is_dtor = symbol.startswith("__dt__")
        rest = symbol[6:]
        m = re.match(r"^(\d+)(\w+)", rest)
        if m:
            length = int(m.group(1))
            name = rest[len(m.group(1)):len(m.group(1)) + length]
            return (name, ("~" + name) if is_dtor else name)
        return None

    m = _MANGLE_RE.match(symbol)
    if not m:
        return None
    method = m.group("method")
    rest = m.group("rest")
    # Parse leading length-prefixed class name
    lm = re.match(r"^(\d+)", rest)
    if not lm:
        return None
    length = int(lm.group(1))
    pos = len(lm.group(1))
    class_name = rest[pos:pos + length]
    return (class_name, method)


# ── Public API ──────────────────────────────────────────────────────────────

def extract_locals(symbol: str, project_dir: Optional[str] = None) -> dict[int, LocalInfo]:
    """Recompile + parse DWARF; return {r1_offset: LocalInfo} for the function."""
    found = _find_unit_for_symbol(symbol, project_dir)
    if not found:
        return {}
    unit_name, src_path = found
    if not src_path.exists():
        return {}

    try:
        debug_o = compile_with_debug(src_path, project_dir)
    except RuntimeError as exc:
        print(f"  [dwarf_locals] {exc}", file=sys.stderr)
        return {}

    # Read + patch in-memory
    data = bytearray(debug_o.read_bytes())
    _apply_ppc_relocs_inplace(data)

    # Now parse DWARF
    from elftools.elf.elffile import ELFFile
    stream = io.BytesIO(bytes(data))
    elf = ELFFile(stream)
    dl = elf.get_section_by_name(".debug_loc")
    if dl is None:
        return {}
    loc_raw = dl.data()
    dwarf = elf.get_dwarf_info()

    parsed = parse_mangled_symbol(symbol)
    if parsed is None:
        return {}
    class_name, method_name = parsed

    # Find subprogram DIE with matching name
    # DWARF name pattern: "ClassName::Method(args)" or just "Method(args)"
    name_pat = f"{class_name}::{method_name}("
    free_pat = f"{method_name}("

    target_die = None
    for cu in dwarf.iter_CUs():
        for die in cu.iter_DIEs():
            if die.tag != "DW_TAG_subprogram":
                continue
            n = die.attributes.get("DW_AT_name")
            if not n:
                continue
            v = n.value.decode(errors="replace")
            if name_pat in v or (class_name == "" and v.startswith(free_pat)):
                target_die = die
                break
        if target_die:
            break

    if target_die is None:
        return {}

    # Walk children + lexical_blocks; collect (offset, name, depth, is_param)
    results: dict[int, LocalInfo] = {}

    def walk(node, depth=0):
        for c in node.iter_children():
            if c.tag == "DW_TAG_lexical_block":
                walk(c, depth + 1)
            elif c.tag in ("DW_TAG_variable", "DW_TAG_formal_parameter"):
                nm = c.attributes.get("DW_AT_name")
                loc = c.attributes.get("DW_AT_location")
                if not nm or not loc or loc.form != "DW_FORM_data4":
                    continue
                fb = _first_fbreg_offset(loc_raw, loc.value)
                if fb is None or fb < 0:
                    continue
                name = nm.value.decode(errors="replace")
                is_param = c.tag == "DW_TAG_formal_parameter"
                # If the same offset already mapped, keep the shallowest depth
                # (outermost decl wins for readability).
                existing = results.get(fb)
                if existing is None or depth < existing.depth:
                    results[fb] = LocalInfo(name, fb, depth, is_param)

    walk(target_die)
    return results


# ── CLI ─────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("symbol", help="Mangled symbol name")
    parser.add_argument("--project-dir", default=None)
    args = parser.parse_args()

    locals_map = extract_locals(args.symbol, args.project_dir)
    if not locals_map:
        print("No locals extracted.", file=sys.stderr)
        sys.exit(1)
    print(f"Found {len(locals_map)} local(s) on the stack:")
    print(f"  {'offset':>8s}  {'name':24s}  depth  kind")
    for off, info in sorted(locals_map.items()):
        kind = "param" if info.is_param else "local"
        print(f"  {off:>5d}  0x{off:03x}  {info.name:24s}  {info.depth:5d}  {kind}")


if __name__ == "__main__":
    main()
