#!/usr/bin/env bash
# Compare RB3 and DC3 decomp databases to find functions we can port.
# Ranks functions by portability: high RB3 match + DC3 source available + small size.
#
# Usage:
#   ./scripts/compare-dc3.sh                  # Top 50 candidates
#   ./scripts/compare-dc3.sh --all            # All candidates
#   ./scripts/compare-dc3.sh --unit char      # Filter to system/char/
#   ./scripts/compare-dc3.sh --min-rb3 90     # Only show RB3 >= 90%
#   ./scripts/compare-dc3.sh --max-rb3 50     # Only show RB3 <= 50% (big gaps)
#   ./scripts/compare-dc3.sh --json           # JSON output for scripting
#   ./scripts/compare-dc3.sh --summary        # Unit-level summary

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RB3_DB="$PROJECT_ROOT/decomp.db"
DC3_ROOT="$PROJECT_ROOT/../dc3-decomp"
DC3_DB="$DC3_ROOT/decomp.db"

# Defaults
LIMIT=50
UNIT_FILTER=""
MIN_RB3=0
MAX_RB3=99.99
JSON=0
SUMMARY=0
SORT="score"  # score, gap, size, pct

usage() {
    cat <<'EOF'
compare-dc3.sh - Find RB3 functions to port from DC3

Options:
  --all             Show all candidates (no limit)
  -n, --limit N     Show top N candidates (default: 50)
  --unit SUBDIR     Filter to system/SUBDIR/ (e.g., char, rndobj, math)
  --min-rb3 PCT     Minimum RB3 match% (default: 0)
  --max-rb3 PCT     Maximum RB3 match% (default: 99.99)
  --sort FIELD      Sort by: score (default), gap, size, pct
  --json            Output JSON (for scripting/agents)
  --summary         Unit-level summary instead of per-function
  -h, --help        This help

Scoring:
  score = (rb3_pct * 2) + (100 / log2(size + 4)) - (size / 100)
  Higher score = easier to port (close to matching + small size)
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --all)       LIMIT=0; shift ;;
        -n|--limit)  LIMIT="$2"; shift 2 ;;
        --unit)      UNIT_FILTER="$2"; shift 2 ;;
        --min-rb3)   MIN_RB3="$2"; shift 2 ;;
        --max-rb3)   MAX_RB3="$2"; shift 2 ;;
        --sort)      SORT="$2"; shift 2 ;;
        --json)      JSON=1; shift ;;
        --summary)   SUMMARY=1; shift ;;
        -h|--help)   usage ;;
        *)           echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

if [[ ! -f "$RB3_DB" ]]; then
    echo "ERROR: RB3 database not found: $RB3_DB" >&2
    exit 1
fi
if [[ ! -f "$DC3_DB" ]]; then
    echo "ERROR: DC3 database not found: $DC3_DB" >&2
    exit 1
fi

# Auto-ingest: rebuild report and update DB before comparing
echo "Rebuilding report and updating DB..." >&2
(cd "$PROJECT_ROOT" && ninja build/SZBE69_B8/report.json 2>&1 | grep -E "^(ninja|ERROR|REPORT)" >&2 || true)
(cd "$PROJECT_ROOT" && bin/orchestrate ingest 2>&1 | grep -v "^$" >&2)

python3 - "$RB3_DB" "$DC3_DB" "$DC3_ROOT" "$LIMIT" "$UNIT_FILTER" "$MIN_RB3" "$MAX_RB3" "$JSON" "$SUMMARY" "$SORT" <<'PYTHON'
import sqlite3
import sys
import os
import json
import math
import re

rb3_db_path, dc3_db_path, dc3_root, limit_str, unit_filter, min_rb3_str, max_rb3_str, json_mode, summary_mode, sort_mode = sys.argv[1:]
limit = int(limit_str)
min_rb3 = float(min_rb3_str)
max_rb3 = float(max_rb3_str)
json_mode = json_mode == "1"
summary_mode = summary_mode == "1"

rb3 = sqlite3.connect(rb3_db_path)
rb3.row_factory = sqlite3.Row

# Shared system subdirectories
SHARED_DIRS = [
    "beatmatch", "char", "math", "meta", "midi", "movie",
    "obj", "oggvorbis", "os", "rndobj", "synth", "ui", "utl", "world", "zlib"
]

if unit_filter:
    dirs_to_check = [d for d in SHARED_DIRS if unit_filter.lower() in d.lower()]
    if not dirs_to_check:
        dirs_to_check = [unit_filter]
else:
    dirs_to_check = SHARED_DIRS

# Collect all incomplete RB3 system functions
candidates = []
for subdir in dirs_to_check:
    rows = rb3.execute("""
        SELECT symbol, demangled, unit, size, current_percent, verdict
        FROM functions
        WHERE unit LIKE ? AND current_percent < 100
        ORDER BY current_percent DESC
    """, (f"main/system/{subdir}/%",)).fetchall()

    for row in rows:
        rb3_pct = row["current_percent"] or 0.0
        if rb3_pct < min_rb3 or rb3_pct > max_rb3:
            continue

        # Map unit to DC3 source file
        unit_rel = row["unit"].replace("main/", "src/")
        dc3_cpp = os.path.join(dc3_root, unit_rel + ".cpp")

        if not os.path.exists(dc3_cpp):
            continue

        # Extract a search-friendly name from the symbol
        symbol = row["symbol"]
        demangled = row["demangled"] or symbol

        # Try to find the function in DC3 source
        # Extract the bare function name for searching
        # For "Bar__6MyObjFv" -> search for "Bar"
        # For "FileRelativePath" -> search for "FileRelativePath"
        bare_name = symbol.split("__")[0] if "__" in symbol else symbol

        # Search DC3 source file for this function name
        found_in_dc3 = False
        try:
            with open(dc3_cpp, "r") as f:
                content = f.read()
                # Look for function definition (name followed by opening paren)
                # This catches "void Foo(" or "Type Foo(" or "Class::Foo("
                pattern = r'(?:^|\s|:)' + re.escape(bare_name) + r'\s*\('
                if re.search(pattern, content):
                    found_in_dc3 = True
        except (IOError, OSError):
            continue

        if not found_in_dc3:
            continue

        size = row["size"] or 0
        verdict = row["verdict"] or ""

        # Scoring: favor high RB3 % (close to done) and small size (easy to fix)
        # score = rb3_pct * 2 + compactness_bonus - size_penalty
        compactness = 100.0 / math.log2(max(size, 4) + 4) if size > 0 else 50.0
        score = rb3_pct * 2.0 + compactness - size / 100.0

        candidates.append({
            "symbol": symbol,
            "demangled": demangled,
            "rb3_unit": row["unit"].replace("main/", ""),
            "dc3_source": dc3_cpp,
            "rb3_pct": round(rb3_pct, 1),
            "size": size,
            "score": round(score, 1),
            "verdict": verdict,
        })

# Sort
sort_keys = {
    "score": lambda r: (-r["score"], r["symbol"]),
    "gap":   lambda r: (r["rb3_pct"], r["symbol"]),  # lowest RB3% first = biggest gap
    "size":  lambda r: (-r["size"], r["symbol"]),     # largest first
    "pct":   lambda r: (-r["rb3_pct"], r["symbol"]),  # highest RB3% first
}
candidates.sort(key=sort_keys.get(sort_mode, sort_keys["score"]))

# Summary mode
if summary_mode:
    from collections import defaultdict
    by_unit = defaultdict(lambda: {"count": 0, "total_size": 0, "avg_pct": 0.0, "pcts": []})
    for c in candidates:
        unit = c["rb3_unit"]
        # Group by parent dir (e.g., system/char)
        parts = unit.split("/")
        group = "/".join(parts[:2]) if len(parts) >= 2 else unit
        by_unit[group]["count"] += 1
        by_unit[group]["total_size"] += c["size"]
        by_unit[group]["pcts"].append(c["rb3_pct"])

    summary = []
    for group, data in by_unit.items():
        avg = sum(data["pcts"]) / len(data["pcts"]) if data["pcts"] else 0
        summary.append({
            "group": group,
            "count": data["count"],
            "total_bytes": data["total_size"],
            "avg_rb3_pct": round(avg, 1),
        })
    summary.sort(key=lambda x: -x["count"])

    if json_mode:
        print(json.dumps(summary, indent=2))
    else:
        print(f"{'System Subdir':<25} {'Count':>7} {'Bytes':>10} {'Avg RB3%':>9}")
        print("-" * 55)
        for s in summary:
            print(f"{s['group']:<25} {s['count']:>7} {s['total_bytes']:>10} {s['avg_rb3_pct']:>8.1f}%")
        print("-" * 55)
        print(f"{'TOTAL':<25} {len(candidates):>7} {sum(c['size'] for c in candidates):>10}")
    sys.exit(0)

# Apply limit
if limit > 0:
    display = candidates[:limit]
else:
    display = candidates

# Output
if json_mode:
    print(json.dumps(display, indent=2))
else:
    print(f"Found {len(candidates)} portable functions (showing {'all' if limit == 0 else f'top {limit}'})")
    print(f"Criteria: RB3 {min_rb3}-{max_rb3}%, DC3 source exists & has implementation")
    print()
    print(f"{'Score':>6} {'RB3%':>6} {'Size':>6} {'Unit':<40} Demangled")
    print("-" * 120)
    for c in display:
        dem = c["demangled"]
        if len(dem) > 50:
            dem = dem[:47] + "..."
        verdict_tag = f" [{c['verdict']}]" if c['verdict'] else ""
        print(f"{c['score']:>6.1f} {c['rb3_pct']:>5.1f}% {c['size']:>6} {c['rb3_unit']:<40} {dem}{verdict_tag}")
    if limit > 0 and len(candidates) > limit:
        print(f"\n... {len(candidates) - limit} more candidates. Use --all to see all.")
PYTHON
