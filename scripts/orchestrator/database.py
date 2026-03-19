"""Database module for RB3 Decomp Orchestrator.

Handles SQLite database for persistent state tracking of functions,
attempts, and worktrees.

Adapted from DC3 orchestrator for Wii (MetroWorks) decompilation.
"""

import json
import sqlite3
from datetime import datetime
from pathlib import Path
from typing import Any

# Database path (relative to repo root)
DEFAULT_DB_PATH = "decomp.db"

# Schema version for migrations
SCHEMA_VERSION = 1

# Default maximum attempts before deprioritizing a function
DEFAULT_MAX_ATTEMPTS = 20

SCHEMA = """
-- Schema version tracking
CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY
);

-- Core function tracking
CREATE TABLE IF NOT EXISTS functions (
    id INTEGER PRIMARY KEY,
    symbol TEXT NOT NULL UNIQUE,        -- Mangled name (MetroWorks style)
    demangled TEXT,                     -- Human-readable
    unit TEXT,                          -- "main/App"
    size INTEGER,

    current_percent REAL,               -- Latest match %
    best_percent REAL,                  -- Best ever match %
    verdict TEXT,                       -- COMPLETE, AT_LIMIT, etc.

    locked_by TEXT,                     -- Session ID (prevents conflicts)
    locked_at TIMESTAMP,

    attempt_count INTEGER DEFAULT 0,
    last_model TEXT,                    -- haiku, sonnet, opus
    next_model TEXT,                    -- What to try next

    source_patch TEXT,                  -- Successful diff

    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Attempt history (learning + debugging)
CREATE TABLE IF NOT EXISTS attempts (
    id INTEGER PRIMARY KEY,
    function_id INTEGER REFERENCES functions(id),
    session_id TEXT,
    model TEXT,

    started_at TIMESTAMP,
    finished_at TIMESTAMP,

    exit_status TEXT,                   -- success, stuck, error
    start_percent REAL,
    end_percent REAL,
    verdict TEXT,

    patch TEXT,                         -- What was tried
    notes TEXT,                         -- Agent's summary
    iterations INTEGER,                 -- How many tool calls

    -- Token usage tracking
    input_tokens INTEGER,
    output_tokens INTEGER,
    cache_read_tokens INTEGER,
    cache_creation_tokens INTEGER,
    actual_cost_usd REAL,
    duration_ms INTEGER,

    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Worktree pool tracking
CREATE TABLE IF NOT EXISTS worktrees (
    id INTEGER PRIMARY KEY,
    path TEXT NOT NULL UNIQUE,
    session_id TEXT,
    status TEXT,                        -- available, in_use, dirty
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Indexes for common queries
CREATE INDEX IF NOT EXISTS idx_functions_verdict ON functions(verdict);
CREATE INDEX IF NOT EXISTS idx_functions_locked ON functions(locked_by);
CREATE INDEX IF NOT EXISTS idx_functions_unit ON functions(unit);
CREATE INDEX IF NOT EXISTS idx_functions_percent ON functions(current_percent);
CREATE INDEX IF NOT EXISTS idx_attempts_function ON attempts(function_id);
CREATE INDEX IF NOT EXISTS idx_attempts_session ON attempts(session_id);
CREATE INDEX IF NOT EXISTS idx_worktrees_status ON worktrees(status);
"""


_migrated_dbs: set[str] = set()  # Track which DB paths have been migration-checked


def get_connection(db_path: str | Path = DEFAULT_DB_PATH) -> sqlite3.Connection:
    """Get a database connection with row factory enabled.

    Automatically runs pending migrations on first access per DB path.
    """
    db_str = str(db_path)
    conn = sqlite3.connect(db_str)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode = WAL")
    conn.execute("PRAGMA foreign_keys = ON")

    if db_str not in _migrated_dbs:
        _migrated_dbs.add(db_str)
        cursor = conn.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name='schema_version'"
        )
        if cursor.fetchone() is not None:
            version = conn.execute("SELECT version FROM schema_version").fetchone()[0]
            if version < SCHEMA_VERSION:
                _run_migrations(conn, version, SCHEMA_VERSION)

    return conn


def init_database(db_path: str | Path = DEFAULT_DB_PATH) -> sqlite3.Connection:
    """Initialize database with schema. Safe to call multiple times."""
    conn = get_connection(db_path)

    # Check if already initialized
    cursor = conn.execute(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='schema_version'"
    )
    if cursor.fetchone() is None:
        # Fresh database - create schema
        conn.executescript(SCHEMA)
        conn.execute("INSERT INTO schema_version (version) VALUES (?)", (SCHEMA_VERSION,))
        conn.commit()
        print(f"Initialized database at {db_path}")
    else:
        # Check version for migrations
        version = conn.execute("SELECT version FROM schema_version").fetchone()[0]
        if version < SCHEMA_VERSION:
            _run_migrations(conn, version, SCHEMA_VERSION)

    return conn


def _run_migrations(conn: sqlite3.Connection, from_version: int, to_version: int) -> None:
    """Run database migrations from from_version to to_version."""
    print(f"Running database migrations: v{from_version} -> v{to_version}")
    # Future migrations go here
    conn.execute("UPDATE schema_version SET version = ?", (to_version,))
    conn.commit()
    print(f"  Migration complete. Database at v{to_version}")


def ingest_report(
    report_path: str | Path,
    db_path: str | Path = DEFAULT_DB_PATH,
    update_existing: bool = True,
) -> dict[str, int]:
    """
    Parse report.json and populate/update the functions table.

    Args:
        report_path: Path to build/SZBE69_B8/report.json
        db_path: Path to SQLite database
        update_existing: If True, update existing functions. If False, skip them.

    Returns:
        Dict with counts: inserted, updated, skipped
    """
    conn = init_database(db_path)

    with open(report_path) as f:
        report = json.load(f)

    inserted = 0
    updated = 0
    skipped = 0

    # report.json structure (dtk-generated):
    # { "units": [ { "name": "...", "functions": [ { ... } ] } ] }
    for unit in report.get("units", []):
        unit_name = unit.get("name", "")

        for func in unit.get("functions", []):
            symbol = func.get("name", "")
            if not symbol:
                continue

            demangled = func.get("metadata", {}).get("demangled_name", "")
            size = int(func.get("size", 0) or 0)

            # Get match percentage
            percent = func.get("fuzzy_match_percent")
            if percent is None:
                percent = func.get("match_percent")

            # Check if function exists
            existing = conn.execute(
                "SELECT id, current_percent, best_percent, verdict FROM functions WHERE symbol = ?",
                (symbol,),
            ).fetchone()

            if existing:
                if update_existing:
                    # Update metadata and match percentage
                    new_best = percent
                    old_best = existing["best_percent"]
                    if old_best is not None and new_best is not None:
                        new_best = max(old_best, new_best)

                    # Auto-set verdict for 100% matches
                    verdict = existing["verdict"]
                    if percent is not None and percent >= 100.0:
                        verdict = "COMPLETE"

                    conn.execute(
                        """
                        UPDATE functions SET
                            demangled = COALESCE(?, demangled),
                            unit = COALESCE(?, unit),
                            size = COALESCE(?, size),
                            current_percent = ?,
                            best_percent = ?,
                            verdict = COALESCE(?, verdict),
                            updated_at = CURRENT_TIMESTAMP
                        WHERE id = ?
                        """,
                        (demangled, unit_name, size, percent, new_best, verdict, existing["id"]),
                    )
                    updated += 1
                else:
                    skipped += 1
            else:
                # Insert new function
                verdict = None
                if percent is not None and percent >= 100.0:
                    verdict = "COMPLETE"

                conn.execute(
                    """
                    INSERT INTO functions
                        (symbol, demangled, unit, size, current_percent, best_percent, verdict)
                    VALUES (?, ?, ?, ?, ?, ?, ?)
                    """,
                    (symbol, demangled, unit_name, size, percent, percent, verdict),
                )
                inserted += 1

    conn.commit()
    return {"inserted": inserted, "updated": updated, "skipped": skipped}


def get_function_by_symbol(
    symbol: str, db_path: str | Path = DEFAULT_DB_PATH
) -> dict[str, Any] | None:
    """Get function by symbol name."""
    conn = get_connection(db_path)
    row = conn.execute(
        """
        SELECT id, symbol, demangled, unit, size, current_percent, best_percent,
               verdict, locked_by, locked_at, attempt_count, last_model, next_model
        FROM functions
        WHERE symbol = ?
        """,
        (symbol,),
    ).fetchone()

    if row:
        return dict(row)
    return None


def search_functions_by_name(
    search_term: str,
    limit: int = 10,
    db_path: str | Path = DEFAULT_DB_PATH,
) -> list[dict[str, Any]]:
    """Search functions by demangled name substring."""
    conn = get_connection(db_path)
    rows = conn.execute(
        """
        SELECT id, symbol, demangled, unit, size, current_percent, verdict
        FROM functions
        WHERE demangled LIKE ? OR symbol LIKE ?
        ORDER BY current_percent DESC NULLS LAST
        LIMIT ?
        """,
        (f"%{search_term}%", f"%{search_term}%", limit),
    ).fetchall()
    return [dict(row) for row in rows]


def get_next_function(
    pattern: str | list[str] = "*",
    min_percent: float = 0,
    max_percent: float = 100,
    exclude_locked: bool = True,
    exclude_complete: bool = True,
    exclude_at_limit: bool = False,
    db_path: str | Path = DEFAULT_DB_PATH,
    order_by: str = "percent",
    order_asc: bool = False,
    min_size: int = 0,
    exclude_patterns: list[str] | None = None,
    max_attempts: int | None = DEFAULT_MAX_ATTEMPTS,
) -> dict[str, Any] | None:
    """
    Get next function to work on based on criteria.

    Returns function dict or None if no matches.
    """
    conn = get_connection(db_path)

    if exclude_patterns is None:
        exclude_patterns = DEFAULT_EXCLUDE_PATTERNS

    glob_clause, glob_params = _build_unit_glob_clause(pattern, exclude_patterns)

    query = f"""
        SELECT id, symbol, demangled, unit, size, current_percent, best_percent,
               verdict, locked_by, attempt_count, last_model
        FROM functions
        WHERE {glob_clause}
          AND (current_percent IS NULL OR (current_percent >= ? AND current_percent <= ?))
    """
    params: list[Any] = glob_params + [min_percent, max_percent]

    if exclude_locked:
        query += " AND locked_by IS NULL"

    if min_size > 0:
        query += f" AND size >= {min_size}"

    if max_attempts is not None:
        query += f" AND (attempt_count IS NULL OR attempt_count < {max_attempts})"

    excluded_verdicts = []
    if exclude_complete:
        excluded_verdicts.append('COMPLETE')
    if exclude_at_limit:
        excluded_verdicts.append('AT_LIMIT')
    if excluded_verdicts:
        placeholders = ", ".join(f"'{v}'" for v in excluded_verdicts)
        query += f" AND (verdict IS NULL OR verdict NOT IN ({placeholders}))"

    direction = "ASC" if order_asc else "DESC"
    if order_by == "size":
        query += f"""
        ORDER BY
            CASE WHEN size IS NULL THEN 1 ELSE 0 END,
            size {direction}
        LIMIT 1
    """
    else:
        query += f"""
        ORDER BY
            CASE WHEN current_percent IS NULL THEN 1 ELSE 0 END,
            current_percent {direction}
        LIMIT 1
    """

    row = conn.execute(query, params).fetchone()
    if row:
        return dict(row)
    return None


# Default exclusion patterns for batch operations
DEFAULT_EXCLUDE_PATTERNS: list[str] = [
    # SDK/library code not decomp targets
    "sdk/*",
    "lib/*",
]


def _build_unit_glob_clause(
    patterns: str | list[str],
    exclude_patterns: list[str] | None = None,
) -> tuple[str, list[str]]:
    """Build a SQL WHERE clause fragment matching one or more unit GLOB patterns."""
    if isinstance(patterns, str):
        patterns = [patterns]

    if len(patterns) == 1:
        include_clause = "unit GLOB ?"
    else:
        clauses = " OR ".join("unit GLOB ?" for _ in patterns)
        include_clause = f"({clauses})"

    params = list(patterns)

    if exclude_patterns:
        exclude_clauses = " AND ".join("unit NOT GLOB ?" for _ in exclude_patterns)
        full_clause = f"{include_clause} AND {exclude_clauses}"
        params = params + list(exclude_patterns)
        return full_clause, params

    return include_clause, params


def query_functions(
    pattern: str | list[str] = "*",
    min_percent: float = 0,
    max_percent: float = 100,
    exclude_locked: bool = True,
    exclude_complete: bool = True,
    exclude_at_limit: bool = False,
    verdict_filter: str | None = None,
    limit: int = 20,
    db_path: str | Path = DEFAULT_DB_PATH,
    exclude_patterns: list[str] | None = None,
    max_attempts: int | None = DEFAULT_MAX_ATTEMPTS,
    skip_boilerplate: bool = False,
) -> list[dict[str, Any]]:
    """
    Query multiple functions matching criteria.

    Returns list of function dicts.
    """
    conn = get_connection(db_path)

    if exclude_patterns is None:
        exclude_patterns = DEFAULT_EXCLUDE_PATTERNS

    glob_clause, glob_params = _build_unit_glob_clause(pattern, exclude_patterns)

    query = f"""
        SELECT id, symbol, demangled, unit, size, current_percent, best_percent,
               verdict, locked_by, attempt_count
        FROM functions
        WHERE {glob_clause}
          AND (current_percent IS NULL OR (current_percent >= ? AND current_percent <= ?))
    """
    params: list[Any] = glob_params + [min_percent, max_percent]

    if exclude_locked:
        query += " AND locked_by IS NULL"

    if verdict_filter:
        query += f" AND verdict = '{verdict_filter}'"
    else:
        excluded_verdicts = []
        if exclude_complete:
            excluded_verdicts.append('COMPLETE')
        if exclude_at_limit:
            excluded_verdicts.append('AT_LIMIT')
        if excluded_verdicts:
            placeholders = ", ".join(f"'{v}'" for v in excluded_verdicts)
            query += f" AND (verdict IS NULL OR verdict NOT IN ({placeholders}))"

    if max_attempts is not None:
        query += f" AND (attempt_count IS NULL OR attempt_count < {max_attempts})"

    query += """
        ORDER BY
            CASE WHEN current_percent IS NULL THEN 1 ELSE 0 END,
            current_percent DESC
        LIMIT ?
    """
    params.append(limit)

    rows = conn.execute(query, params).fetchall()
    return [dict(row) for row in rows]


def lock_function(
    function_id: int, session_id: str, db_path: str | Path = DEFAULT_DB_PATH
) -> bool:
    """Lock a function for exclusive work by a session."""
    conn = get_connection(db_path)
    row = conn.execute(
        "SELECT locked_by FROM functions WHERE id = ?", (function_id,)
    ).fetchone()

    if row is None:
        return False
    if row["locked_by"] is not None and row["locked_by"] != session_id:
        return False

    conn.execute(
        """
        UPDATE functions
        SET locked_by = ?, locked_at = CURRENT_TIMESTAMP, updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
        """,
        (session_id, function_id),
    )
    conn.commit()
    return True


def unlock_function(
    function_id: int, db_path: str | Path = DEFAULT_DB_PATH
) -> None:
    """Release lock on a function."""
    conn = get_connection(db_path)
    conn.execute(
        """
        UPDATE functions
        SET locked_by = NULL, locked_at = NULL, updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
        """,
        (function_id,),
    )
    conn.commit()


def unlock_session(
    session_id: str, db_path: str | Path = DEFAULT_DB_PATH
) -> int:
    """Release all locks held by a session."""
    conn = get_connection(db_path)
    cursor = conn.execute(
        """
        UPDATE functions
        SET locked_by = NULL, locked_at = NULL, updated_at = CURRENT_TIMESTAMP
        WHERE locked_by = ?
        """,
        (session_id,),
    )
    conn.commit()
    return cursor.rowcount


def record_attempt(
    function_id: int,
    session_id: str,
    model: str = "unknown",
    start_percent: float = 0,
    end_percent: float = 0,
    exit_status: str = "unknown",
    verdict: str | None = None,
    notes: str = "",
    patch: str = "",
    iterations: int = 0,
    db_path: str | Path = DEFAULT_DB_PATH,
    input_tokens: int | None = None,
    output_tokens: int | None = None,
    cache_read_tokens: int | None = None,
    cache_creation_tokens: int | None = None,
    actual_cost_usd: float | None = None,
    duration_ms: int | None = None,
) -> int:
    """Record an attempt on a function. Returns attempt ID."""
    conn = get_connection(db_path)
    cursor = conn.execute(
        """
        INSERT INTO attempts
            (function_id, session_id, model, started_at, finished_at,
             exit_status, start_percent, end_percent, verdict,
             patch, notes, iterations,
             input_tokens, output_tokens, cache_read_tokens, cache_creation_tokens,
             actual_cost_usd, duration_ms)
        VALUES (?, ?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP,
                ?, ?, ?, ?,
                ?, ?, ?,
                ?, ?, ?, ?,
                ?, ?)
        """,
        (function_id, session_id, model,
         exit_status, start_percent, end_percent, verdict,
         patch, notes, iterations,
         input_tokens, output_tokens, cache_read_tokens, cache_creation_tokens,
         actual_cost_usd, duration_ms),
    )

    # Update attempt count
    conn.execute(
        """
        UPDATE functions
        SET attempt_count = COALESCE(attempt_count, 0) + 1,
            last_model = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
        """,
        (model, function_id),
    )
    conn.commit()
    return cursor.lastrowid


def update_function_status(
    function_id: int,
    current_percent: float | None = None,
    verdict: str | None = None,
    db_path: str | Path = DEFAULT_DB_PATH,
) -> None:
    """Update a function's match percentage and/or verdict."""
    conn = get_connection(db_path)

    updates = ["updated_at = CURRENT_TIMESTAMP"]
    params: list[Any] = []

    if current_percent is not None:
        updates.append("current_percent = ?")
        params.append(current_percent)
        updates.append("best_percent = MAX(COALESCE(best_percent, 0), ?)")
        params.append(current_percent)

    if verdict is not None:
        updates.append("verdict = ?")
        params.append(verdict)

    params.append(function_id)
    conn.execute(
        f"UPDATE functions SET {', '.join(updates)} WHERE id = ?",
        params,
    )
    conn.commit()


def get_last_attempt(
    function_id: int, db_path: str | Path = DEFAULT_DB_PATH
) -> dict[str, Any] | None:
    """Get the most recent attempt for a function."""
    conn = get_connection(db_path)
    row = conn.execute(
        """
        SELECT * FROM attempts
        WHERE function_id = ?
        ORDER BY created_at DESC
        LIMIT 1
        """,
        (function_id,),
    ).fetchone()
    return dict(row) if row else None


def get_attempts_for_function(
    function_id: int, limit: int = 10, db_path: str | Path = DEFAULT_DB_PATH
) -> list[dict[str, Any]]:
    """Get recent attempts for a function."""
    conn = get_connection(db_path)
    rows = conn.execute(
        """
        SELECT * FROM attempts
        WHERE function_id = ?
        ORDER BY created_at DESC
        LIMIT ?
        """,
        (function_id, limit),
    ).fetchall()
    return [dict(row) for row in rows]


def get_stats(db_path: str | Path = DEFAULT_DB_PATH) -> dict[str, Any]:
    """Get overall database statistics."""
    conn = get_connection(db_path)

    total = conn.execute("SELECT COUNT(*) FROM functions").fetchone()[0]
    complete = conn.execute(
        "SELECT COUNT(*) FROM functions WHERE verdict = 'COMPLETE'"
    ).fetchone()[0]
    at_limit = conn.execute(
        "SELECT COUNT(*) FROM functions WHERE verdict = 'AT_LIMIT'"
    ).fetchone()[0]
    avg_pct = conn.execute(
        "SELECT AVG(current_percent) FROM functions WHERE current_percent IS NOT NULL"
    ).fetchone()[0]
    total_attempts = conn.execute("SELECT COUNT(*) FROM attempts").fetchone()[0]

    return {
        "total_functions": total,
        "complete": complete,
        "at_limit": at_limit,
        "avg_percent": round(avg_pct, 2) if avg_pct else 0,
        "total_attempts": total_attempts,
    }
