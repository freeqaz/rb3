#!/usr/bin/env python3
"""
Compare C++ header struct layouts against Ghidra's inferred layouts.

Adapted from DC3 decomp for RB3 Wii (MetroWerks CW, PowerPC Gekko).

Uses the local struct_db.sqlite (built from annotated headers) as our source
of truth and queries pyghidra-mcp's list_structures for Ghidra's view.

Usage:
    python3 tools/ghidra/struct_check.py Character
    python3 tools/ghidra/struct_check.py --unit system/char/Character
    python3 tools/ghidra/struct_check.py --all --pattern 'Rnd*'
"""

import argparse
import json
import re
import sqlite3
import sys
from pathlib import Path

# Add project root to path for imports
PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

from tools.ghidra.mcp_client import MCPClient, MCPError
from tools.struct_db import StructDB


STRUCT_DB_PATH = str(PROJECT_ROOT / "struct_db.sqlite")
DECOMP_DB_PATH = str(PROJECT_ROOT / "decomp.db")


def get_our_layout(class_name: str, db_path: str = STRUCT_DB_PATH) -> dict | None:
    """Get our struct layout from struct_db.sqlite."""
    with StructDB(db_path) as db:
        return db.get_class_info(class_name)


def get_ghidra_layout(client: MCPClient, class_name: str) -> list | None:
    """Get Ghidra's struct layout via MCP list_structures.

    Returns list of {name, offset, size, type_str} or None if not found.
    """
    # Try exact match first, then broader search
    for query in [f"^{class_name}(\\.conflict.*)?$", class_name]:
        try:
            result = client.list_structures(query=query, limit=50)
        except MCPError as e:
            print(f"Warning: Ghidra query failed: {e}", file=sys.stderr)
            return None

        structures = _extract_structures(result, class_name)
        if structures is not None:
            return structures

    return None


def _extract_structures(result, class_name: str) -> list | None:
    """Extract member list from Ghidra's list_structures response."""
    if isinstance(result, str):
        try:
            result = json.loads(result)
        except json.JSONDecodeError:
            return _parse_text_structures(result, class_name)

    if isinstance(result, dict):
        structs = result.get("structures", [result])
    elif isinstance(result, list):
        structs = result
    else:
        return None

    # Find our class in the results
    candidates = []
    for struct in structs:
        if not isinstance(struct, dict):
            continue
        name = struct.get("name", "")
        # Match class name, including ".conflict" variants from DTM collisions
        base_name = name.split(".conflict")[0]
        if base_name == class_name or base_name.endswith(f"::{class_name}") or base_name.endswith(f"/{class_name}"):
            members = struct.get("members", struct.get("fields", []))
            if isinstance(members, list):
                normalized = [_normalize_member(m) for m in members if isinstance(m, dict)]
                candidates.append(normalized)

    if not candidates:
        return None

    # Return the candidate with the most named (non-padding) members
    return max(candidates, key=lambda ms: sum(1 for m in ms if m["name"]))


def _normalize_member(m: dict) -> dict:
    """Normalize a Ghidra member dict to {name, offset, size, type_str}."""
    return {
        "name": m.get("name", m.get("fieldName", "?")),
        "offset": m.get("offset", m.get("ordinal", -1)),
        "size": m.get("size", m.get("length", 0)),
        "type_str": m.get("type_str", m.get("type", m.get("dataType", "?"))),
    }


def _parse_text_structures(text: str, class_name: str) -> list | None:
    """Try to parse plain-text structure output from Ghidra."""
    members = []
    in_target = False
    for line in text.split("\n"):
        if class_name in line and ("struct" in line.lower() or "class" in line.lower()):
            in_target = True
            continue
        if in_target:
            m = re.match(r'\s*(?:0x)?([0-9a-fA-F]+)\s*:\s*(\S+)\s+(\S+)(?:\s*\((\d+)\s*bytes?\))?', line)
            if m:
                members.append({
                    "name": m.group(3),
                    "offset": int(m.group(1), 16),
                    "size": int(m.group(4)) if m.group(4) else 0,
                    "type_str": m.group(2),
                })
            elif line.strip() == "" and members:
                break
    return members if members else None


def diff_layouts(class_name: str, ours: dict, ghidra: list) -> list:
    """Compare our layout vs Ghidra's. Returns list of diff rows."""
    rows = []

    # Index Ghidra members by offset, skip unnamed padding bytes
    ghidra_by_offset = {}
    for m in ghidra:
        off = m["offset"]
        if off >= 0 and m["name"] is not None:
            ghidra_by_offset[off] = m

    # Index our members by offset
    our_by_offset = {}
    for m in ours.get("members", []):
        our_by_offset[m["offset"]] = m

    # All offsets
    all_offsets = sorted(set(our_by_offset.keys()) | set(ghidra_by_offset.keys()))

    for off in all_offsets:
        our_m = our_by_offset.get(off)
        ghidra_m = ghidra_by_offset.get(off)

        if our_m and ghidra_m:
            status = "OK"
            our_name = our_m["name"]
            gh_name = ghidra_m["name"]
            if our_name != gh_name and not gh_name.startswith("field_"):
                status = "NAME_DIFF"
            rows.append({
                "offset": off,
                "our_name": our_name,
                "our_type": our_m.get("type_str", "?"),
                "ghidra_name": gh_name,
                "ghidra_type": ghidra_m.get("type_str", "?"),
                "status": status,
            })
        elif our_m and not ghidra_m:
            rows.append({
                "offset": off,
                "our_name": our_m["name"],
                "our_type": our_m.get("type_str", "?"),
                "ghidra_name": "-",
                "ghidra_type": "-",
                "status": "GHIDRA_MISSING",
            })
        else:
            rows.append({
                "offset": off,
                "our_name": "-",
                "our_type": "-",
                "ghidra_name": ghidra_m["name"],
                "ghidra_type": ghidra_m.get("type_str", "?"),
                "status": "OURS_MISSING",
            })

    return rows


def print_diff_table(class_name: str, rows: list) -> int:
    """Print a comparison table. Returns number of mismatches."""
    if not rows:
        print(f"{class_name}: no fields to compare")
        return 0

    mismatches = sum(1 for r in rows if r["status"] not in ("OK", "NAME_DIFF"))
    name_diffs = sum(1 for r in rows if r["status"] == "NAME_DIFF")

    print(f"\n{'='*70}")
    print(f"  {class_name}")
    print(f"{'='*70}")
    print(f"  {'Offset':<10} {'Our Field':<25} {'Ghidra Field':<25} {'Status'}")
    print(f"  {'-'*10} {'-'*25} {'-'*25} {'-'*10}")

    for r in rows:
        status_str = r["status"]
        if status_str == "OK":
            marker = ""
        elif status_str == "NAME_DIFF":
            marker = " ~"
        else:
            marker = " ***"

        our_field = f"{r['our_name']}"
        gh_field = f"{r['ghidra_name']}"

        print(f"  0x{r['offset']:04x}     {our_field:<25} {gh_field:<25} {status_str}{marker}")

    total = len(rows)
    ok = total - mismatches - name_diffs
    print(f"\n  {ok}/{total} OK", end="")
    if name_diffs:
        print(f", {name_diffs} name diffs", end="")
    if mismatches:
        print(f", {mismatches} MISMATCHES", end="")
    print()

    return mismatches


def get_classes_for_unit(unit: str) -> list:
    """Get class names referenced in a translation unit from decomp.db."""
    # RB3 decomp.db uses MetroWerks-style demangled names
    conn = sqlite3.connect(DECOMP_DB_PATH)
    try:
        # Get demangled function names for this unit and extract class names
        # RB3 unit format: "main/system/char/Character" or similar
        # Try several unit formats
        candidates = [unit]
        if not unit.startswith("main/"):
            candidates.append(f"main/{unit}")
        if not unit.startswith("main/src/"):
            candidates.append(f"main/src/{unit}")

        class_names = set()
        for unit_try in candidates:
            cursor = conn.execute(
                "SELECT DISTINCT demangled FROM functions WHERE unit LIKE ?",
                (f"%{unit_try}%",)
            )
            for (demangled,) in cursor:
                if not demangled or "::" not in demangled:
                    continue
                # Extract class name from CW demangled names
                # Format: "ClassName::Method(args)"
                clean = demangled.strip()
                if "::" in clean:
                    parts = clean.split("::")
                    for part in parts[:-1]:
                        part = part.strip()
                        # Remove return type prefix if present
                        if ' ' in part:
                            part = part.split()[-1]
                        if part and part[0].isupper() and part.isidentifier():
                            class_names.add(part)

            if class_names:
                break

        return sorted(class_names)
    finally:
        conn.close()


def main():
    parser = argparse.ArgumentParser(
        description="Compare C++ header struct layouts against Ghidra's inferred layouts (RB3)"
    )
    parser.add_argument(
        "class_names", nargs="*",
        help="Class names to check"
    )
    parser.add_argument(
        "--unit", "-u",
        help="Check all classes referenced in a translation unit (e.g., system/char/Character)"
    )
    parser.add_argument(
        "--all", action="store_true",
        help="Check all classes in struct_db"
    )
    parser.add_argument(
        "--pattern", "-p",
        help="Filter classes by glob pattern (with --all)"
    )
    parser.add_argument(
        "--db", default=STRUCT_DB_PATH,
        help=f"Path to struct_db.sqlite (default: {STRUCT_DB_PATH})"
    )
    parser.add_argument(
        "--json", action="store_true",
        help="Output as JSON"
    )

    args = parser.parse_args()

    # Determine which classes to check
    classes_to_check = list(args.class_names) if args.class_names else []

    if args.unit:
        unit_classes = get_classes_for_unit(args.unit)
        if not unit_classes:
            print(f"No classes found for unit: {args.unit}", file=sys.stderr)
            sys.exit(1)
        print(f"Found {len(unit_classes)} classes in unit {args.unit}: {', '.join(unit_classes)}")
        classes_to_check.extend(unit_classes)

    if args.all:
        # First check if struct_db exists, if not offer to build it
        db_path = Path(args.db)
        if not db_path.exists():
            print(f"struct_db.sqlite not found at {args.db}", file=sys.stderr)
            print(f"Build it with: python3 tools/struct_db.py build src/", file=sys.stderr)
            sys.exit(1)
        with StructDB(args.db) as db:
            all_classes = db.list_classes(args.pattern)
        classes_to_check.extend(c["name"] for c in all_classes)

    if not classes_to_check:
        parser.print_help()
        sys.exit(1)

    # Deduplicate while preserving order
    seen = set()
    unique_classes = []
    for c in classes_to_check:
        if c not in seen:
            seen.add(c)
            unique_classes.append(c)
    classes_to_check = unique_classes

    # Check if struct_db exists
    db_path = Path(args.db)
    if not db_path.exists():
        print(f"struct_db.sqlite not found at {args.db}", file=sys.stderr)
        print(f"Build it with: python3 tools/struct_db.py build src/", file=sys.stderr)
        sys.exit(1)

    # Connect to Ghidra MCP
    try:
        client = MCPClient()
        client.initialize()
    except MCPError as e:
        print(f"Error: Could not connect to pyghidra-mcp", file=sys.stderr)
        print(f"  {e}", file=sys.stderr)
        print(f"\nIs the service running? Check with: ./tools/ghidra/pyghidra-service.sh status", file=sys.stderr)
        sys.exit(1)

    # Process each class
    total_mismatches = 0
    results = {}

    for class_name in classes_to_check:
        ours = get_our_layout(class_name, args.db)
        if not ours:
            print(f"\n{class_name}: not found in struct_db (no offset annotations in headers?)",
                  file=sys.stderr)
            continue

        if not ours.get("members"):
            print(f"\n{class_name}: no annotated members in struct_db", file=sys.stderr)
            continue

        ghidra = get_ghidra_layout(client, class_name)
        if ghidra is None:
            print(f"\n{class_name}: not found in Ghidra's Data Type Manager", file=sys.stderr)
            continue

        if not ghidra:
            print(f"\n{class_name}: Ghidra struct has no members", file=sys.stderr)
            continue

        rows = diff_layouts(class_name, ours, ghidra)

        if args.json:
            results[class_name] = rows
        else:
            mismatches = print_diff_table(class_name, rows)
            total_mismatches += mismatches

    if args.json:
        print(json.dumps(results, indent=2))
    elif len(classes_to_check) > 1:
        print(f"\n{'='*70}")
        print(f"  Total: {total_mismatches} mismatches across {len(classes_to_check)} classes")
        print(f"{'='*70}")

    sys.exit(1 if total_mismatches > 0 else 0)


if __name__ == "__main__":
    main()
