#!/usr/bin/env python3
"""
Import CodeWarrior/Metrowerks linker map symbols into Ghidra via MCP.

Parses the .text section of a CodeWarrior linker map file and applies
function symbols to the Ghidra project via the pyghidra-mcp API.

This is needed because pyghidra-mcp's built-in --map-file only supports
MSVC-format maps. CodeWarrior maps have a different format:

    .text section layout
      Starting        Virtual  File
      address  Size   address  offset
      ---------------------------------
      00000000 000368 8000de20 0000a000 16 AppDebugModal__FRbPcb  App.o

The virtual address column (3rd) is the actual runtime address.
Lines with alignment "1" are section/object headers, not individual symbols.

When using the debug ELF (which already has DWARF symbols), this script
is typically NOT needed. It's mainly useful for the retail DOL.

Usage:
    # Parse-only (test mode, no Ghidra connection):
    python3 import_cw_map.py --parse-only /path/to/band_r_wii.map

    # Import via MCP:
    python3 import_cw_map.py /path/to/band_r_wii.map

    # Import specific sections:
    python3 import_cw_map.py --sections .text,.rodata,.data /path/to/band_r_wii.map
"""

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


@dataclass
class MapSymbol:
    """A symbol parsed from the CodeWarrior map file."""
    name: str
    virtual_address: int
    size: int
    section: str
    object_file: str


def parse_cw_map(map_path: str, sections: Optional[list[str]] = None) -> list[MapSymbol]:
    """Parse a CodeWarrior linker map file and extract symbols.

    Args:
        map_path: Path to the .map file
        sections: List of section names to parse (e.g. ['.text', '.data']).
                  If None, parses .text only.

    Returns:
        List of MapSymbol entries
    """
    if sections is None:
        sections = [".text"]

    symbols: list[MapSymbol] = []
    current_section: Optional[str] = None
    in_section = False
    past_header = False

    # Pattern for section headers like ".text section layout"
    section_header_re = re.compile(r'^(\.\w+) section layout')

    # Pattern for symbol lines:
    # StartAddr Size    VirtAddr FileOff  Align SymbolName  ObjectFile
    # 00000000  000368  8000de20 0000a000 16    __dt__8DataNodeFv  App.o
    #
    # Alignment 1 = section/object header (skip)
    # Alignment 16/4/8 = actual symbol entry
    # Lines with *fill* are padding (skip)
    symbol_re = re.compile(
        r'^\s*([0-9a-fA-F]{8})\s+'   # starting address
        r'([0-9a-fA-F]{6,8})\s+'     # size
        r'([0-9a-fA-F]{8})\s+'       # virtual address
        r'([0-9a-fA-F]{6,8})\s+'     # file offset
        r'(\d+)\s+'                   # alignment
        r'(\S+)'                      # symbol name
    )

    with open(map_path, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            # Check for section header
            m = section_header_re.match(line)
            if m:
                section_name = m.group(1)
                if section_name in sections:
                    current_section = section_name
                    in_section = True
                    past_header = False
                else:
                    in_section = False
                    current_section = None
                continue

            if not in_section:
                continue

            # Skip the column header lines
            if '---' in line:
                past_header = True
                continue

            if not past_header:
                continue

            # Empty line ends section (two consecutive newlines)
            stripped = line.strip()
            if not stripped:
                # Section might have blank lines between objects; keep going
                continue

            # If we hit another section header pattern, stop
            if section_header_re.match(line):
                break

            # Parse symbol line
            m = symbol_re.match(stripped)
            if not m:
                continue

            start_addr = m.group(1)
            size_hex = m.group(2)
            virt_addr = m.group(3)
            file_off = m.group(4)
            alignment = int(m.group(5))
            name = m.group(6)

            size = int(size_hex, 16)

            # Skip entries with alignment 1 (section/object headers, not symbols)
            if alignment == 1:
                continue

            # Skip fill entries
            if '*fill*' in name:
                continue

            # Skip zero-size entries (usually end-of-section markers like .text.67493)
            if size == 0:
                continue

            vaddr = int(virt_addr, 16)

            # Extract object file from the rest of the line
            # The object file comes after the symbol name, separated by whitespace/tab
            rest = stripped[m.end():]
            obj_file = rest.strip().split('\t')[-1].strip() if rest.strip() else ""

            symbols.append(MapSymbol(
                name=name,
                virtual_address=vaddr,
                size=size,
                section=current_section,
                object_file=obj_file,
            ))

    return symbols


def import_via_mcp(symbols: list[MapSymbol], url: str = "http://127.0.0.1:8001/mcp") -> dict:
    """Import parsed symbols into Ghidra via pyghidra-mcp bulk_create_functions + search.

    This creates function entries at each address and labels them.

    Args:
        symbols: List of parsed MapSymbol entries
        url: MCP server URL

    Returns:
        Summary dict with counts
    """
    # Import here to avoid dependency when just parsing
    from mcp_client import MCPClient, MCPError

    client = MCPClient(url=url)
    client.initialize()

    # Step 1: Create functions at all addresses
    text_symbols = [s for s in symbols if s.section == ".text"]
    addresses = [f"{s.virtual_address:08x}" for s in text_symbols]

    print(f"Creating {len(addresses)} functions...")
    try:
        result = client.call_tool("bulk_create_functions", {
            "binary_name": client.binary,
            "addresses": addresses,
        }, timeout=600)
        print(f"  Created: {result.get('created', 0)}, "
              f"Already exist: {result.get('already_exist', 0)}, "
              f"Failed: {result.get('failed', 0)}")
    except MCPError as e:
        print(f"  Warning: bulk_create_functions failed: {e}")
        result = {}

    # Step 2: Apply symbol names
    # Build list in the format expected by apply_demangled_signatures
    # For CW mangled names, we just apply as labels
    print(f"Applying {len(text_symbols)} symbol names...")
    applied = 0
    failed = 0
    batch_size = 500

    for i in range(0, len(text_symbols), batch_size):
        batch = text_symbols[i:i + batch_size]
        sym_list = [
            {"mangled": s.name, "address": f"{s.virtual_address:08x}"}
            for s in batch
        ]
        try:
            r = client.call_tool("apply_demangled_signatures", {
                "binary_name": client.binary,
                "symbols": sym_list,
            }, timeout=300)
            applied += r.get("applied", 0) + r.get("partial", 0)
            failed += r.get("demangle_failed", 0) + r.get("no_function", 0)
        except MCPError as e:
            print(f"  Warning: batch {i//batch_size} failed: {e}")
            failed += len(batch)

        if (i + batch_size) % 2000 == 0:
            print(f"  Progress: {i + batch_size}/{len(text_symbols)}")

    return {
        "total_symbols": len(symbols),
        "text_symbols": len(text_symbols),
        "applied": applied,
        "failed": failed,
        **result,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Import CodeWarrior map symbols into Ghidra via MCP"
    )
    parser.add_argument("map_file", help="Path to CodeWarrior .map file")
    parser.add_argument(
        "--sections", default=".text",
        help="Comma-separated list of sections to parse (default: .text)"
    )
    parser.add_argument(
        "--parse-only", action="store_true",
        help="Only parse the map file, don't connect to Ghidra"
    )
    parser.add_argument(
        "--url", default="http://127.0.0.1:8001/mcp",
        help="MCP server URL (default: http://127.0.0.1:8001/mcp)"
    )
    parser.add_argument(
        "--stats", action="store_true",
        help="Print statistics about parsed symbols"
    )
    args = parser.parse_args()

    sections = [s.strip() for s in args.sections.split(",")]

    print(f"Parsing {args.map_file}...")
    print(f"Sections: {sections}")

    symbols = parse_cw_map(args.map_file, sections=sections)
    print(f"Parsed {len(symbols)} symbols")

    if args.stats or args.parse_only:
        # Group by section
        by_section: dict[str, list[MapSymbol]] = {}
        for s in symbols:
            by_section.setdefault(s.section, []).append(s)

        for section, syms in sorted(by_section.items()):
            print(f"\n{section}: {len(syms)} symbols")
            # Top object files
            obj_counts: dict[str, int] = {}
            for s in syms:
                obj_counts[s.object_file] = obj_counts.get(s.object_file, 0) + 1
            top_objs = sorted(obj_counts.items(), key=lambda x: -x[1])[:10]
            for obj, count in top_objs:
                print(f"  {obj}: {count} symbols")

        # Show a few example symbols
        print(f"\nFirst 10 symbols:")
        for s in symbols[:10]:
            print(f"  0x{s.virtual_address:08x}  {s.size:6d}  {s.name}  ({s.object_file})")

        if len(symbols) > 10:
            print(f"\nLast 5 symbols:")
            for s in symbols[-5:]:
                print(f"  0x{s.virtual_address:08x}  {s.size:6d}  {s.name}  ({s.object_file})")

    if args.parse_only:
        return

    # Import via MCP
    print(f"\nImporting to Ghidra via {args.url}...")
    result = import_via_mcp(symbols, url=args.url)
    print(f"\nImport complete:")
    for k, v in result.items():
        print(f"  {k}: {v}")


if __name__ == "__main__":
    main()
