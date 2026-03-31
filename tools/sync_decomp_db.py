#!/usr/bin/env python3
"""
Sync decomp.db with the current build report (report.json).

Updates match percentages, sizes, units, and best-ever scores.
Creates the DB and schema if it doesn't exist.

Usage:
    python3 tools/sync_decomp_db.py          # sync from report.json
    python3 tools/sync_decomp_db.py --stats   # sync + print summary stats
"""

import argparse
import json
import sqlite3
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DB_PATH = ROOT / "decomp.db"
REPORT_PATH = ROOT / "build" / "SZBE69_B8" / "report.json"

SCHEMA_VERSION = 2

SCHEMA_SQL = """
CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY
);

CREATE TABLE IF NOT EXISTS functions (
    id INTEGER PRIMARY KEY,
    symbol TEXT NOT NULL UNIQUE,
    demangled TEXT,
    unit TEXT,
    size INTEGER,

    current_percent REAL,
    best_percent REAL,
    verdict TEXT,

    locked_by TEXT,
    locked_at TIMESTAMP,

    attempt_count INTEGER DEFAULT 0,
    last_model TEXT,
    next_model TEXT,

    source_patch TEXT,

    ghidra_decompiled TEXT,
    ghidra_decompiled_at TIMESTAMP,
    m2c_decompiled TEXT,
    m2c_decompiled_at TIMESTAMP,

    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_functions_verdict ON functions(verdict);
CREATE INDEX IF NOT EXISTS idx_functions_locked ON functions(locked_by);
CREATE INDEX IF NOT EXISTS idx_functions_unit ON functions(unit);
CREATE INDEX IF NOT EXISTS idx_functions_percent ON functions(current_percent);

CREATE TABLE IF NOT EXISTS attempts (
    id INTEGER PRIMARY KEY,
    function_id INTEGER REFERENCES functions(id),
    session_id TEXT,
    model TEXT,

    started_at TIMESTAMP,
    finished_at TIMESTAMP,

    exit_status TEXT,
    start_percent REAL,
    end_percent REAL,
    verdict TEXT,

    error_log TEXT,
    diff_summary TEXT,
    source_patch TEXT
);

CREATE TABLE IF NOT EXISTS worktrees (
    id INTEGER PRIMARY KEY,
    name TEXT UNIQUE,
    path TEXT,
    branch TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    status TEXT DEFAULT 'active'
);
"""


def ensure_schema(db: sqlite3.Connection):
    """Create tables if they don't exist, run migrations."""
    db.executescript(SCHEMA_SQL)

    row = db.execute("SELECT MAX(version) FROM schema_version").fetchone()
    current = row[0] if row[0] is not None else 0

    if current < 2:
        # Add decompiled code cache columns if missing
        cols = {r[1] for r in db.execute("PRAGMA table_info(functions)").fetchall()}
        for col, typ in [
            ("ghidra_decompiled", "TEXT"),
            ("ghidra_decompiled_at", "TIMESTAMP"),
            ("m2c_decompiled", "TEXT"),
            ("m2c_decompiled_at", "TIMESTAMP"),
        ]:
            if col not in cols:
                db.execute(f"ALTER TABLE functions ADD COLUMN {col} {typ}")

        db.execute("INSERT OR REPLACE INTO schema_version (version) VALUES (?)", (SCHEMA_VERSION,))
        db.commit()


def sync(db: sqlite3.Connection, report: dict) -> dict:
    """Sync functions from report into DB. Returns stats dict."""
    now = datetime.utcnow().isoformat()
    inserted = 0
    updated = 0
    unchanged = 0

    for unit in report["units"]:
        unit_name = unit.get("name", "")
        for func in unit.get("functions", []):
            symbol = func.get("name", "")
            if not symbol:
                continue

            size = func.get("size", 0)
            match_pct = func.get("fuzzy_match_percent", 0.0)
            demangled = func.get("demangled_name", "")

            row = db.execute(
                "SELECT id, current_percent, best_percent FROM functions WHERE symbol = ?",
                (symbol,),
            ).fetchone()

            if row:
                fid, cur, best = row
                new_best = max(best or 0, match_pct)
                if cur != match_pct or best != new_best or not best:
                    db.execute(
                        "UPDATE functions SET current_percent=?, best_percent=?, size=?, unit=?, demangled=?, updated_at=? WHERE id=?",
                        (match_pct, new_best, size, unit_name, demangled or None, now, fid),
                    )
                    updated += 1
                else:
                    unchanged += 1
            else:
                db.execute(
                    "INSERT INTO functions (symbol, demangled, unit, size, current_percent, best_percent, created_at, updated_at) VALUES (?,?,?,?,?,?,?,?)",
                    (symbol, demangled or None, unit_name, size, match_pct, match_pct, now, now),
                )
                inserted += 1

    db.commit()
    return {"inserted": inserted, "updated": updated, "unchanged": unchanged}


def print_stats(db: sqlite3.Connection):
    """Print summary statistics."""
    total = db.execute("SELECT COUNT(*) FROM functions").fetchone()[0]
    matched = db.execute("SELECT COUNT(*) FROM functions WHERE current_percent >= 100").fetchone()[0]
    at_limit = db.execute("SELECT COUNT(*) FROM functions WHERE verdict = 'AT_LIMIT'").fetchone()[0]
    cached = db.execute("SELECT COUNT(*) FROM functions WHERE ghidra_decompiled IS NOT NULL").fetchone()[0]
    attempts = db.execute("SELECT COUNT(*) FROM attempts").fetchone()[0]

    avg = db.execute("SELECT AVG(current_percent) FROM functions WHERE current_percent > 0").fetchone()[0] or 0

    print(f"\n{'='*40}")
    print(f"  decomp.db Summary")
    print(f"{'='*40}")
    print(f"  Total functions:  {total:,}")
    print(f"  100% matched:     {matched:,}")
    print(f"  At limit:         {at_limit:,}")
    print(f"  Avg match (>0%):  {avg:.1f}%")
    print(f"  Ghidra cached:    {cached:,}")
    print(f"  Attempt records:  {attempts:,}")
    print()


def main():
    parser = argparse.ArgumentParser(description="Sync decomp.db with report.json")
    parser.add_argument("--stats", action="store_true", help="Print summary stats after sync")
    parser.add_argument("--report", type=Path, default=REPORT_PATH, help="Path to report.json")
    parser.add_argument("--db", type=Path, default=DB_PATH, help="Path to decomp.db")
    args = parser.parse_args()

    if not args.report.exists():
        print(f"Report not found: {args.report}", file=sys.stderr)
        print("Run: ninja build/SZBE69_B8/report.json", file=sys.stderr)
        sys.exit(1)

    with open(args.report) as f:
        report = json.load(f)

    db = sqlite3.connect(str(args.db))
    ensure_schema(db)

    result = sync(db, report)
    print(f"Synced: {result['inserted']} inserted, {result['updated']} updated, {result['unchanged']} unchanged")

    if args.stats:
        print_stats(db)

    db.close()


if __name__ == "__main__":
    main()
