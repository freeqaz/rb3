#!/usr/bin/env python3
"""
Cache Ghidra decompilation and m2c output into decomp.db.

Usage:
    # Cache a single function
    python3 tools/cache_decompiled.py Poll__9CharacterFv

    # Cache all functions in a unit
    python3 tools/cache_decompiled.py --unit char/Character

    # Cache all functions below a match threshold (refresh stale ones)
    python3 tools/cache_decompiled.py --below 80 --limit 100

    # Force refresh even if already cached
    python3 tools/cache_decompiled.py --unit rndobj/Mesh --force

    # Only cache m2c (skip Ghidra)
    python3 tools/cache_decompiled.py --m2c-only --unit math/Rot
"""

import argparse
import sqlite3
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DB_PATH = ROOT / "decomp.db"
M2C = ROOT.parent / "m2c" / "m2c.py"
ASM_DIR = ROOT / "build/SZBE69_B8/asm"

sys.path.insert(0, str(ROOT / "tools" / "ghidra"))
try:
    from mcp_client import MCPClient, MCPError
    GHIDRA_AVAILABLE = True
except ImportError:
    GHIDRA_AVAILABLE = False


def get_ghidra_decompiled(client, symbol):
    """Get Ghidra decompilation for a symbol."""
    try:
        # Search for the symbol to get its address
        addr = None
        sym_result = client.search_symbols(symbol, limit=5)
        syms = sym_result.get("symbols", []) if isinstance(sym_result, dict) else []
        for sym in sorted(syms, key=lambda s: (s.get("type") != "Function")):
            if symbol in (sym.get("name", ""), sym.get("name", "").split("-")[0]):
                addr = "0x" + sym["address"]
                break

        target = addr or symbol
        result = client.decompile_function(target)

        if isinstance(result, str):
            return result
        return result.get("decompiled_code") or result.get("code", "")
    except (MCPError, Exception) as e:
        return f"// Error: {e}"


def get_m2c_decompiled(symbol, unit=None):
    """Get m2c decompilation for a symbol."""
    if not M2C.exists():
        return None

    # Find asm file
    asm_file = None
    if unit:
        unit = unit.removeprefix("main/")
        for prefix in ("", "band3/", "system/", "network/", "lib/", "sdk/"):
            candidate = ASM_DIR / prefix / (unit + ".s") if prefix else ASM_DIR / (unit + ".s")
            if candidate.exists():
                asm_file = candidate
                break
    else:
        needle = f".fn {symbol},"
        for f in sorted(ASM_DIR.rglob("*.s")):
            if needle in f.read_text():
                asm_file = f
                break

    if not asm_file:
        return None

    proc = subprocess.run(
        [sys.executable, str(M2C), "--target", "ppc", "-f", symbol, "--passes", "4", str(asm_file)],
        capture_output=True, text=True, timeout=60
    )
    if proc.returncode != 0:
        return f"// m2c error: {proc.stderr.strip()}"
    return proc.stdout.strip()


def main():
    parser = argparse.ArgumentParser(description="Cache decompiled code into decomp.db")
    parser.add_argument("symbol", nargs="?", help="Specific mangled symbol to cache")
    parser.add_argument("--unit", help="Cache all functions in a unit (e.g. char/Character)")
    parser.add_argument("--below", type=float, help="Cache functions below this match %%")
    parser.add_argument("--limit", type=int, default=50, help="Max functions to process (default: 50)")
    parser.add_argument("--force", action="store_true", help="Re-cache even if already present")
    parser.add_argument("--m2c-only", action="store_true", help="Only cache m2c output (skip Ghidra)")
    parser.add_argument("--ghidra-only", action="store_true", help="Only cache Ghidra output (skip m2c)")
    args = parser.parse_args()

    db = sqlite3.connect(str(DB_PATH))
    db.row_factory = sqlite3.Row

    # Build query
    if args.symbol:
        rows = db.execute("SELECT id, symbol, unit FROM functions WHERE symbol = ?", (args.symbol,)).fetchall()
    elif args.unit:
        unit_pattern = f"%{args.unit}%"
        rows = db.execute(
            "SELECT id, symbol, unit FROM functions WHERE unit LIKE ? ORDER BY current_percent ASC LIMIT ?",
            (unit_pattern, args.limit)
        ).fetchall()
    elif args.below is not None:
        where = "WHERE current_percent < ? AND current_percent > 0"
        if not args.force:
            if not args.m2c_only:
                where += " AND ghidra_decompiled IS NULL"
            else:
                where += " AND m2c_decompiled IS NULL"
        rows = db.execute(
            f"SELECT id, symbol, unit FROM functions {where} ORDER BY current_percent DESC LIMIT ?",
            (args.below, args.limit)
        ).fetchall()
    else:
        parser.error("Provide a symbol, --unit, or --below threshold")
        return

    if not rows:
        print("No functions to process.")
        return

    print(f"Processing {len(rows)} functions...")

    # Initialize Ghidra client if needed
    client = None
    if not args.m2c_only and GHIDRA_AVAILABLE:
        try:
            client = MCPClient()
            client.initialize()
        except Exception as e:
            print(f"Warning: Ghidra not available ({e}), skipping Ghidra caching")
            client = None

    now = datetime.utcnow().isoformat()
    cached = 0
    for i, row in enumerate(rows):
        symbol = row["symbol"]
        unit = row["unit"]
        func_id = row["id"]

        # Check if already cached
        if not args.force:
            existing = db.execute(
                "SELECT ghidra_decompiled, m2c_decompiled FROM functions WHERE id = ?",
                (func_id,)
            ).fetchone()
            if existing["ghidra_decompiled"] and existing["m2c_decompiled"]:
                continue

        print(f"  [{i+1}/{len(rows)}] {symbol}...", end=" ", flush=True)

        updates = {}

        # Ghidra
        if client and not args.m2c_only:
            code = get_ghidra_decompiled(client, symbol)
            if code and not code.startswith("// Error"):
                updates["ghidra_decompiled"] = code
                updates["ghidra_decompiled_at"] = now

        # m2c
        if not args.ghidra_only:
            m2c_code = get_m2c_decompiled(symbol, unit.removeprefix("main/") if unit else None)
            if m2c_code:
                updates["m2c_decompiled"] = m2c_code
                updates["m2c_decompiled_at"] = now

        if updates:
            sets = ", ".join(f"{k} = ?" for k in updates)
            vals = list(updates.values()) + [func_id]
            db.execute(f"UPDATE functions SET {sets} WHERE id = ?", vals)
            db.commit()
            cached += 1
            print("cached")
        else:
            print("skipped")

    print(f"\nDone. Cached {cached}/{len(rows)} functions.")
    db.close()


if __name__ == "__main__":
    main()
