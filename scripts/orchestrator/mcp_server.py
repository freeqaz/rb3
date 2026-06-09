#!/usr/bin/env python3
"""
MCP Server for RB3 Decomp Orchestrator.

Provides tools for sub-agents to:
- Report task completion results
- Query function database for work targets
- Get previous attempt history
- Run objdiff with smart output handling
- Deep diff analysis via diff_inspect

Run as: python3 -m scripts.orchestrator.mcp_server --db decomp.db
"""

import argparse
import asyncio
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

# Maximum lines to return inline (larger outputs go to file)
MAX_INLINE_LINES = 500

# MCP protocol imports
try:
    from mcp.server import Server
    from mcp.server.stdio import stdio_server
    from mcp.types import Tool, TextContent
except ImportError:
    print("MCP package not installed. Install with: pip install mcp", file=sys.stderr)
    sys.exit(1)

# Add scripts and project root to path
sys.path.insert(0, str(Path(__file__).parent.parent))
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from orchestrator.database import (
    get_connection,
    get_function_by_symbol,
    query_functions as db_query_functions,
    query_next_targets as db_query_next_targets,
    get_attempts_for_function,
    get_last_attempt,
    record_attempt,
    update_function_status,
    search_functions_by_name,
)


# Patterns for filtering noisy build output
_NINJA_PROGRESS = re.compile(r'^\s*\[\d+/\d+\]\s')
_NOISY_PREFIXES = (' INFO ', ' WARN ', 'INFO ')
_NOISY_SUBSTRINGS = (
    'Skipping tail block merge',
    'Known functions complete',
    'Detected tail block',
    'Not a function @',
    'Found ',
)


def _stack_signal_summary(instrs: list) -> str | None:
    """Compute a one-line stack-layout signal from already-parsed objdiff instructions.

    Returns None when there's no actionable signal (frame matches AND no user-slot
    mismatches). Otherwise returns "Stack: ..." suitable for one-line display in
    run_objdiff output. Uses only the JSON we already have — no DWARF recompile.
    """
    try:
        from analysis.stack_layout import (
            build_fingerprints, parse_prologue,
            classify_slots, dominant_delta_from_rows,
        )
    except ImportError:
        return None

    if not instrs:
        return None

    try:
        tgt_slots = build_fingerprints("target", instrs)
        base_slots = build_fingerprints("base", instrs)
        if not tgt_slots and not base_slots:
            return None
        tgt_prol = parse_prologue(instrs, "target")
        base_prol = parse_prologue(instrs, "base")
        dom = dominant_delta_from_rows(tgt_slots, base_slots)
        rows = classify_slots(
            tgt_slots, base_slots, dom,
            tgt_prol.callee_save_slots, base_prol.callee_save_slots,
        )
    except Exception:
        return None

    from collections import Counter
    user_rows = [r for r in rows if not r.callee_save]
    counts = Counter(r.verdict for r in user_rows)
    swapped = counts.get("SWAPPED", 0)
    shifted = counts.get("SHIFTED", 0)
    differ = counts.get("DIFFER", 0)
    tgt_only = counts.get("TGT_ONLY", 0)
    base_only = counts.get("BASE_ONLY", 0)
    actionable = swapped + shifted + differ + tgt_only + base_only
    frame_delta = base_prol.frame_size - tgt_prol.frame_size

    if actionable == 0 and frame_delta == 0:
        return None  # no signal worth showing

    parts: list[str] = []
    if frame_delta != 0:
        callee_bytes = (
            (base_prol.saved_gpr_count - tgt_prol.saved_gpr_count) * 4
            + (base_prol.saved_fpr_count - tgt_prol.saved_fpr_count) * 8
        )
        if callee_bytes == frame_delta:
            parts.append(f"frame Δ {frame_delta:+#x} (callee-save AT_LIMIT)")
        else:
            parts.append(f"frame Δ {frame_delta:+#x} (structural)")

    verdict_pieces = []
    if swapped:
        verdict_pieces.append(f"{swapped} SWAPPED")
    if shifted:
        verdict_pieces.append(f"{shifted} SHIFTED")
    if differ:
        verdict_pieces.append(f"{differ} DIFFER")
    if tgt_only or base_only:
        verdict_pieces.append(f"{tgt_only}/{base_only} TGT/BASE-only")
    if verdict_pieces:
        parts.append(", ".join(verdict_pieces))

    if not parts:
        return None

    hint = ""
    if swapped > 0:
        hint = " — reorder paired declarations"
    elif shifted > 0:
        hint = " — likely extra local on one side"
    elif differ > 0 and frame_delta == 0:
        hint = " — different variables in same slots"

    return (f"**Stack:** {' | '.join(parts)}{hint}. "
            f"Run `run_diff_inspect mode=stack-layout` for the full table.")


def _filter_build_output(text: str) -> str:
    """Filter noisy build/split output, keeping only meaningful lines."""
    if not text:
        return ""
    lines = text.strip().splitlines()
    filtered = []
    for line in lines:
        if _NINJA_PROGRESS.match(line):
            continue
        if any(line.startswith(p) for p in _NOISY_PREFIXES):
            continue
        if any(s in line for s in _NOISY_SUBSTRINGS):
            continue
        filtered.append(line)
    return "\n".join(filtered)


# MetroWorks/Itanium ABI mangled name pattern: MethodName__<N><ClassName><params>
_ITANIUM_PATTERN = re.compile(r'^(.+?)__(\d+)(\w+)')


def _demangle_itanium_to_qualified(symbol: str) -> str | None:
    """Demangle an Itanium-style mangled name to ClassName::MethodName.

    Returns None if the symbol is not Itanium-mangled.

    Examples:
        PokeStart__12GlitchFinderFPCcUi... -> GlitchFinder::PokeStart
        __ct__12GlitchFinderFv             -> GlitchFinder::GlitchFinder
        __dt__12GlitchFinderFv             -> GlitchFinder::~GlitchFinder
        SomeFunc__Fv (free function)       -> None
    """
    if "::" in symbol:
        return None

    m = _ITANIUM_PATTERN.match(symbol)
    if not m:
        return None

    method, class_len_str, rest = m.group(1), m.group(2), m.group(3)
    class_len = int(class_len_str)

    if class_len > len(rest) or class_len == 0:
        return None

    class_name = rest[:class_len]

    # Handle ctor/dtor special names
    if method == "__ct":
        method = class_name
    elif method == "__dt":
        method = f"~{class_name}"

    return f"{class_name}::{method}"


class DecompMCPServer:
    """MCP Server providing decomp orchestration tools for RB3."""

    def __init__(self, db_path: str, record_attempts: bool = True, dc3_path: str | None = None):
        self.db_path = db_path
        self.record_attempts = record_attempts
        # Determine project root from script location
        self.project_root = Path(__file__).resolve().parent.parent.parent
        # DC3 source path for lookup_dc3 (shared Milo engine reference)
        self.dc3_path = dc3_path or os.path.expanduser("~/code/milohax/dc3-decomp/src")
        # DC3 build report path (per-unit match% used to rank lookup_dc3 results)
        self.dc3_report_path = os.path.expanduser(
            "~/code/milohax/dc3-decomp/build/373307D9/report.json"
        )
        # Cache for the parsed DC3 report: {unit_name: matched_functions_percent}.
        # Keyed by the report file's mtime; reloaded only when mtime changes.
        self._dc3_report_cache: dict[str, float] | None = None
        self._dc3_report_mtime: float | None = None
        self.server = Server("rb3-decomp")
        self._setup_tools()

    def _setup_tools(self):
        """Register all MCP tools."""

        @self.server.list_tools()
        async def list_tools() -> list[Tool]:
            return [
                Tool(
                    name="report_result",
                    description="Report task completion. Call when done working on a function.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "symbol": {
                                "type": "string",
                                "description": "Function symbol (mangled name) being reported on",
                            },
                            "status": {
                                "type": "string",
                                "enum": ["complete", "at_limit", "stuck", "error"],
                                "description": "Exit status: complete (100%), at_limit (unfixable), stuck (need help), error",
                            },
                            "percent": {
                                "type": "number",
                                "description": "Final match percentage (0-100)",
                            },
                            "notes": {
                                "type": "string",
                                "description": "Summary of what was tried",
                            },
                            "model": {
                                "type": "string",
                                "description": "Model that worked on this (e.g., 'sonnet', 'haiku', 'opus')",
                            },
                        },
                        "required": ["symbol", "status", "percent", "notes"],
                    },
                ),
                Tool(
                    name="query_functions",
                    description="Query the function database for potential work targets.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "min_percent": {
                                "type": "number",
                                "description": "Minimum match percentage (default: 0)",
                            },
                            "max_percent": {
                                "type": "number",
                                "description": "Maximum match percentage (default: 100)",
                            },
                            "unit_pattern": {
                                "type": "string",
                                "description": "Glob pattern for unit path (e.g., 'main/*', 'engine/rndobj/*')",
                            },
                            "limit": {
                                "type": "integer",
                                "description": "Max results to return (default: 20)",
                            },
                            "status": {
                                "type": "string",
                                "description": "Filter by function status: 'workable' (default), 'all', 'complete', 'at_limit', 'stub' (no real body in our build — empty/placeholder or never implemented)",
                                "enum": ["workable", "all", "complete", "at_limit", "stub"],
                            },
                        },
                    },
                ),
                Tool(
                    name="next_target",
                    description="Return workable functions ranked for ROI (score = current_percent / (attempt_count + 1)). Use to get a ready list of functions to work on.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "unit_pattern": {
                                "type": "string",
                                "description": "Glob pattern for unit path (e.g., 'main/*', 'main/system/char/*'). Default: '*'.",
                            },
                            "min_percent": {
                                "type": "number",
                                "description": "Minimum current match percentage (default: 0)",
                            },
                            "max_percent": {
                                "type": "number",
                                "description": "Maximum current match percentage (default: 99.5; caps out stuck residuals)",
                            },
                            "min_size": {
                                "type": "integer",
                                "description": "Minimum function size in bytes (default: 16)",
                            },
                            "limit": {
                                "type": "integer",
                                "description": "Max results to return (default: 5)",
                            },
                        },
                    },
                ),
                Tool(
                    name="get_attempts",
                    description="Get previous attempt history for a function to learn from.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "symbol": {
                                "type": "string",
                                "description": "Function symbol (mangled name)",
                            },
                        },
                        "required": ["symbol"],
                    },
                ),
                Tool(
                    name="run_objdiff",
                    description="Build and diff a function, returning match% and verdict. Handles large output automatically.\n\nPass project_dir parameter when in a worktree so your edits are tested.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "symbol": {
                                "type": "string",
                                "description": "Function symbol (mangled name)",
                            },
                            "full_build": {
                                "type": "boolean",
                                "description": "Force full rebuild (slower but more accurate). Default: false",
                            },
                            "project_dir": {
                                "type": "string",
                                "description": "Project directory to build from. Pass your worktree directory here.",
                            },
                            "context": {
                                "type": "integer",
                                "description": "Show N instructions of context before/after each mismatch. Default: 3.",
                            },
                            "concise": {
                                "type": "boolean",
                                "description": "Concise output: match%, compact summary, patterns, verdict headline. Default: true.",
                            },
                            "full_listing": {
                                "type": "boolean",
                                "description": "Show ALL instructions instead of only mismatches. Default: false.",
                            },
                            "unit": {
                                "type": "string",
                                "description": "Unit name to disambiguate when a symbol exists in multiple units.",
                            },
                        },
                        "required": ["symbol"],
                    },
                ),
                Tool(
                    name="run_diff_inspect",
                    description="Deep analysis of WHY a function doesn't match. Provides root cause diagnosis, cluster analysis, register swap detection, offset analysis, and stack-layout slot diff (with base-side variable names from a MWCC DWARF recompile). Use after run_objdiff when you need deeper insight. For functions with stack signal flagged in run_objdiff output, use mode=stack-layout.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "symbol": {
                                "type": "string",
                                "description": "Function symbol (mangled or demangled name)",
                            },
                            "mode": {
                                "type": "string",
                                "enum": ["diagnose", "clusters", "regswaps", "offsets", "replaces", "compare", "save_baseline", "mismatches", "stack-layout"],
                                "description": "Analysis mode: diagnose (root cause), clusters (contiguous groups), regswaps (register swap pairs), offsets (offset shift histogram), replaces (categorize noise vs real), compare (delta vs baseline), save_baseline, mismatches (instruction table), stack-layout (per-slot diff with base-side variable names from DWARF)",
                            },
                            "project_dir": {
                                "type": "string",
                                "description": "Project directory to build from.",
                            },
                            "baseline_json": {
                                "type": "string",
                                "description": "Optional: path to baseline JSON file for compare mode.",
                            },
                            "unit": {
                                "type": "string",
                                "description": "Unit name to disambiguate.",
                            },
                        },
                        "required": ["symbol", "mode"],
                    },
                ),
                Tool(
                    name="batch_objdiff",
                    description=(
                        "Diff many symbols against the target binary in one shot. "
                        "Runs ninja-locked ONCE (via the first symbol's --build), "
                        "then issues per-symbol diff queries against the already-built "
                        "object files. Each entry is deterministically classified as "
                        "COSMETIC | REAL_DIFF | BORDERLINE so the caller can promote "
                        "COSMETIC verdicts without an LLM judgment.\n\n"
                        "Use this for verification-sweep workflows where you have a "
                        "list of candidate symbols at >=95% fuzzy match. Far cheaper "
                        "than N×run_objdiff because the build cost is amortized."
                    ),
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "symbols": {
                                "type": "array",
                                "items": {"type": "string"},
                                "description": "Mangled function symbols (1..N).",
                            },
                            "units": {
                                "type": "array",
                                "items": {"type": "string"},
                                "description": "Optional per-symbol unit names (aligned with symbols[]).",
                            },
                            "project_dir": {
                                "type": "string",
                                "description": "Project directory (defaults to repo root).",
                            },
                        },
                        "required": ["symbols"],
                    },
                ),
                Tool(
                    name="mark_patch_result",
                    description="Mark a queued patch as applied, failed, or skipped.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "patch_queue_id": {
                                "type": "integer",
                                "description": "Patch queue ID from the manifest",
                            },
                            "status": {
                                "type": "string",
                                "enum": ["applied", "failed", "skipped"],
                                "description": "Result of applying the patch",
                            },
                            "reason": {
                                "type": "string",
                                "description": "Explanation if failed or skipped",
                            },
                        },
                        "required": ["patch_queue_id", "status"],
                    },
                ),
                Tool(
                    name="lookup_dc3",
                    description="Search DC3 decomp source for a method or class name (shared Milo engine reference). Results are deduped per file and ranked by the DC3 unit's matched_functions_percent (how complete that unit's decomp is). Accepts a method name, qualified name, or RB3 MWCC mangled symbol — extraction is automatic.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "symbol": {
                                "type": "string",
                                "description": "Method name (e.g. 'Poll'), qualified name (e.g. 'CharServoBone::Poll'), or RB3 mangled symbol (e.g. 'Poll__13CharServoBoneFv'). Mangled MWCC symbols are auto-extracted to their method name.",
                            },
                            "min_match": {
                                "type": "number",
                                "description": "Only return files whose DC3 unit is at least this matched_functions_percent (0-100). Default: 0 (no filter).",
                            },
                        },
                        "required": ["symbol"],
                    },
                ),
            ]

        @self.server.call_tool()
        async def call_tool(name: str, arguments: dict) -> list[TextContent]:
            if name == "report_result":
                return await self._report_result(arguments)
            elif name == "query_functions":
                return await self._query_functions(arguments)
            elif name == "next_target":
                return await self._next_target(arguments)
            elif name == "get_attempts":
                return await self._get_attempts(arguments)
            elif name == "run_objdiff":
                return await self._run_objdiff(arguments)
            elif name == "batch_objdiff":
                return await self._batch_objdiff(arguments)
            elif name == "run_diff_inspect":
                return await self._run_diff_inspect(arguments)
            elif name == "mark_patch_result":
                return await self._mark_patch_result(arguments)
            elif name == "lookup_dc3":
                return await self._lookup_dc3(arguments)
            else:
                return [TextContent(type="text", text=f"Unknown tool: {name}")]

    async def _report_result(self, args: dict) -> list[TextContent]:
        """Handle report_result tool call."""
        symbol = args.get("symbol", "")
        status = args.get("status", "unknown")
        percent = args.get("percent", 0)
        notes = args.get("notes", "")
        model = args.get("model", "unknown")

        db_stored = False
        if symbol and self.record_attempts:
            func = get_function_by_symbol(symbol, db_path=self.db_path)
            if func:
                start_percent = func.get("current_percent") or 0

                verdict = None
                if status == "at_limit":
                    verdict = "AT_LIMIT"
                elif status == "complete":
                    verdict = "COMPLETE"

                record_attempt(
                    function_id=func["id"],
                    session_id="mcp_direct",
                    model=model,
                    start_percent=start_percent,
                    end_percent=percent,
                    exit_status=status,
                    verdict=verdict,
                    notes=notes,
                    db_path=self.db_path,
                )

                update_function_status(
                    function_id=func["id"],
                    current_percent=percent,
                    verdict=verdict,
                    db_path=self.db_path,
                )
                db_stored = True

        result = {
            "_decomp_exit": True,
            "status": status,
            "percent": percent,
            "notes": notes,
        }

        status_msg = f"Result recorded: {status} at {percent}%"
        if db_stored:
            status_msg += " (stored to database)"
        elif symbol:
            status_msg += f" (function not found in database: {symbol})"

        return [
            TextContent(
                type="text",
                text=f"{status_msg}\n\n```json\n{json.dumps(result, indent=2)}\n```",
            )
        ]

    async def _query_functions(self, args: dict) -> list[TextContent]:
        """Handle query_functions tool call."""
        min_percent = args.get("min_percent", 0)
        max_percent = args.get("max_percent", 100)
        pattern = args.get("unit_pattern", "*")
        limit = args.get("limit", 20)
        status = args.get("status", "workable")

        stubs_only = False
        if status == "all":
            exclude_complete = False
            exclude_at_limit = False
            verdict_filter = None
        elif status == "complete":
            exclude_complete = False
            exclude_at_limit = True
            verdict_filter = "COMPLETE"
        elif status == "at_limit":
            exclude_complete = True
            exclude_at_limit = False
            verdict_filter = "AT_LIMIT"
        elif status == "stub":
            # Functions with no real body in our build (is_stub=1) — empty/
            # placeholder stubs that link, plus never-implemented ones. Set by
            # scripts/analysis/find_stubs.py --update-db.
            exclude_complete = True
            exclude_at_limit = False
            verdict_filter = None
            stubs_only = True
        else:  # "workable"
            exclude_complete = True
            exclude_at_limit = True
            verdict_filter = None

        results = db_query_functions(
            pattern=pattern,
            min_percent=min_percent,
            max_percent=max_percent,
            exclude_complete=exclude_complete,
            exclude_at_limit=exclude_at_limit,
            verdict_filter=verdict_filter,
            limit=limit,
            db_path=self.db_path,
            stubs_only=stubs_only,
        )

        # Check for hidden functions when filtering by unit
        hidden_note = ""
        if status != "all" and pattern != "*":
            all_results = db_query_functions(
                pattern=pattern,
                min_percent=0,
                max_percent=100,
                exclude_complete=False,
                exclude_at_limit=False,
                verdict_filter=None,
                limit=9999,
                max_attempts=None,
                db_path=self.db_path,
            )
            total = len(all_results)
            if total > len(results):
                hidden_note = (
                    f"\n---\n"
                    f"Note: Showing {len(results)} of {total} functions "
                    f"(filtered by status='{status}'). "
                    f"Use status='all' to see all functions in this unit."
                )

        if not results:
            msg = "No functions found matching criteria."
            if hidden_note:
                msg += hidden_note
            return [TextContent(type="text", text=msg)]

        max_display = 30
        output = f"Found {len(results)} functions"
        if len(results) > max_display:
            output += f" (showing first {max_display})"
        output += ":\n\n"
        for func in results[:max_display]:
            pct = func.get("current_percent")
            pct_str = f"{pct:.1f}%" if pct is not None else "unimplemented"
            verdict = func.get("verdict")
            verdict_str = f" | Verdict: {verdict}" if verdict else ""
            output += f"- `{func['symbol']}` ({func.get('demangled', 'N/A')})\n"
            output += f"  Unit: {func.get('unit', 'unknown')} | Match: {pct_str}{verdict_str}\n"

        if len(results) > max_display:
            output += f"\n... and {len(results) - max_display} more\n"

        if hidden_note:
            output += hidden_note

        return [TextContent(type="text", text=output)]

    async def _next_target(self, args: dict) -> list[TextContent]:
        """Handle next_target tool call.

        Returns workable functions ranked by ROI score
        (current_percent / (attempt_count + 1)), descending.
        """
        pattern = args.get("unit_pattern", "*")
        min_percent = args.get("min_percent", 0)
        max_percent = args.get("max_percent", 99.5)
        min_size = args.get("min_size", 16)
        limit = args.get("limit", 5)

        results = db_query_next_targets(
            pattern=pattern,
            min_percent=min_percent,
            max_percent=max_percent,
            min_size=min_size,
            limit=limit,
            db_path=self.db_path,
        )

        if not results:
            return [TextContent(
                type="text",
                text="No workable targets found matching criteria.",
            )]

        output = f"Top {len(results)} ROI targets (score = current_percent / (attempt_count + 1)):\n\n"
        for func in results:
            pct = func.get("current_percent")
            pct_str = f"{pct:.1f}%" if pct is not None else "unimplemented"
            attempts = func.get("attempt_count") or 0
            output += f"- `{func['symbol']}` ({func.get('demangled', 'N/A')})\n"
            output += (
                f"  Unit: {func.get('unit', 'unknown')} | Match: {pct_str} | "
                f"Size: {func.get('size', '?')} | Attempts: {attempts}\n"
            )

            last = get_last_attempt(func["id"], db_path=self.db_path)
            if last:
                exit_status = last.get("exit_status", "unknown")
                end_pct = last.get("end_percent")
                end_str = f"{end_pct:.1f}%" if end_pct is not None else "?"
                created = last.get("created_at", "?")
                output += (
                    f"  Last attempt: {exit_status} -> {end_str} ({created})\n"
                )
            else:
                output += "  Last attempt: never attempted\n"

        return [TextContent(type="text", text=output)]

    async def _get_attempts(self, args: dict) -> list[TextContent]:
        """Handle get_attempts tool call."""
        symbol = args.get("symbol", "")

        func = get_function_by_symbol(symbol, db_path=self.db_path)
        if not func:
            return [TextContent(type="text", text=f"Function not found: {symbol}")]

        attempts = get_attempts_for_function(func["id"], limit=10, db_path=self.db_path)

        if not attempts:
            return [TextContent(type="text", text="No previous attempts for this function.")]

        output = f"## Previous Attempts for {symbol}\n\n"
        output += f"**Current Status:** {func.get('current_percent', 'unknown')}% match, Verdict: {func.get('verdict', 'unknown')}\n\n"

        for i, attempt in enumerate(attempts, 1):
            start_pct = attempt.get('start_percent') or 0
            end_pct = attempt.get('end_percent') or 0
            change = end_pct - start_pct
            change_str = f"+{change:.1f}%" if change >= 0 else f"{change:.1f}%"

            status = attempt.get('exit_status', 'unknown')

            output += f"### Attempt {i}: {status.upper()}\n"
            output += f"- **Model:** {attempt.get('model', 'unknown')}\n"
            output += f"- **Match:** {start_pct:.1f}% -> {end_pct:.1f}% ({change_str})\n"
            if attempt.get("verdict"):
                output += f"- **Verdict:** {attempt['verdict']}\n"
            if attempt.get("notes"):
                notes = attempt['notes']
                if len(notes) > 200:
                    notes = notes[:200] + "..."
                output += f"- **Notes:** {notes}\n"
            output += "\n"

        return [TextContent(type="text", text=output)]

    def _suggest_similar_symbols(self, symbol: str) -> list[str]:
        """When a symbol is not found, suggest similar symbols from the database."""
        search_term = symbol
        demangled = _demangle_itanium_to_qualified(symbol)
        if demangled:
            search_term = demangled
        elif "::" in symbol:
            search_term = symbol

        try:
            results = search_functions_by_name(search_term, limit=5, db_path=self.db_path)
            if not results:
                method_only = search_term.split("::")[-1] if "::" in search_term else search_term
                results = search_functions_by_name(method_only, limit=5, db_path=self.db_path)

            suggestions = []
            for r in results:
                pct = r.get("current_percent")
                pct_str = f" ({pct:.1f}%)" if pct is not None else ""
                suggestions.append(f"`{r['symbol']}`{pct_str}")
            return suggestions
        except Exception:
            return []

    async def _run_objdiff(self, args: dict) -> list[TextContent]:
        """Handle run_objdiff tool call."""
        symbol = args.get("symbol", "")
        full_build = args.get("full_build", False)
        project_dir_arg = args.get("project_dir", None)
        context = args.get("context", 3)
        concise = args.get("concise", True)
        full_listing = args.get("full_listing", False)
        unit = args.get("unit", None)

        if not symbol:
            return [TextContent(type="text", text="Error: No symbol provided.")]

        # Determine project directory
        if project_dir_arg:
            project_dir = Path(project_dir_arg)
            if not project_dir.exists():
                return [TextContent(
                    type="text",
                    text=f"Error: project_dir does not exist: {project_dir}"
                )]
        elif os.environ.get("REPO_ROOT"):
            project_dir = Path(os.environ["REPO_ROOT"])
        else:
            project_dir = self.project_root

        # Find objdiff-cli -- try project bin/ first, then PATH
        objdiff_cli = project_dir / "bin" / "objdiff-cli"
        if not objdiff_cli.exists():
            # Try system PATH
            which_result = subprocess.run(
                ["which", "objdiff-cli"], capture_output=True, text=True
            )
            if which_result.returncode == 0:
                objdiff_cli = Path(which_result.stdout.strip())
            else:
                return [TextContent(
                    type="text",
                    text="Error: objdiff-cli not found in project bin/ or system PATH"
                )]

        # Common args
        base_args = [
            str(objdiff_cli),
            "diff",
            "-p", str(project_dir),
            symbol,
            "--verdict",
        ]
        if unit:
            base_args.extend(["-u", unit])

        build_flag = ["--build"]
        if full_build:
            build_flag.append("--full-build")

        json_extra = ["--include-instructions"]
        if full_listing:
            json_extra.append("--full-listing")
        elif context:
            json_extra.extend(["-C", str(context)])

        try:
            # 1) JSON run (with build) - for enrichment data
            json_cmd = base_args + json_extra + build_flag + ["-f", "json"]
            json_result = subprocess.run(
                json_cmd,
                capture_output=True,
                text=True,
                timeout=300,
                cwd=str(project_dir),
            )

            json_output = json_result.stdout
            stderr_text = _filter_build_output(json_result.stderr)

            has_json = "{" in json_output
            stdout_has_error = "Symbol not found" in json_output or (
                "Failed" in json_output and not has_json
            )
            stderr_has_error = "Failed" in (json_result.stderr or "")

            if stdout_has_error or (stderr_has_error and not has_json):
                suggestions = self._suggest_similar_symbols(symbol)
                error_msg = _filter_build_output(json_output)
                if stderr_text:
                    error_msg += f"\n\n[stderr]\n{stderr_text}"
                if suggestions:
                    error_msg += "\n\nDid you mean:\n" + "\n".join(
                        f"  - {s}" for s in suggestions
                    )
                return [TextContent(type="text", text=error_msg.strip())]

            # Strip ninja build preamble
            _json_start = json_output.find("{")
            if _json_start > 0:
                json_output = json_output[_json_start:]

            # 2) Markdown run (no build, already built)
            md_cmd = list(base_args) + ["-f", "markdown"]
            if full_listing:
                md_cmd.append("--full-listing")
            elif concise:
                md_cmd.append("--concise")
            md_result = subprocess.run(
                md_cmd,
                capture_output=True,
                text=True,
                timeout=60,
                cwd=str(project_dir),
            )
            output = md_result.stdout

            # 3) Try to fix match% from JSON
            try:
                data = json.loads(json_output)
                fuzzy_pct = data.get("fuzzy_match_percent")
                raw_pct = data.get("raw_match_percent")
                if fuzzy_pct is not None and raw_pct is not None:
                    output = re.sub(
                        r"Match: [\d.]+% normalized \([\d.]+% raw\)",
                        f"Match: {fuzzy_pct:.1f}% normalized ({raw_pct:.1f}% raw)",
                        output,
                        count=1,
                    )
            except (json.JSONDecodeError, KeyError):
                pass

            # 4) Append mismatch preview
            try:
                data = json.loads(json_output)
                instrs = data.get("instructions", [])
                if instrs:
                    mismatches = [ins for ins in instrs if ins.get("match_type") != "equal"]
                    if mismatches:
                        match_pct = data.get("fuzzy_match_percent", 0)
                        if match_pct >= 98:
                            show_limit = len(mismatches)
                        elif match_pct >= 90:
                            show_limit = 15
                        else:
                            show_limit = 8

                        shown = mismatches[:show_limit]
                        lines = ["\n## Key Mismatches\n"]
                        for ins in shown:
                            idx = ins.get("index", "?")
                            mt = ins.get("match_type", "?")
                            t = ins.get("target", {})
                            b = ins.get("base", {})
                            t_str = f"{t.get('opcode', '---')} {t.get('args', '')}" if t else "---"
                            b_str = f"{b.get('opcode', '---')} {b.get('args', '')}" if b else "---"
                            lines.append(f"- [{idx}] {mt}: `{t_str}` vs `{b_str}`")

                        if len(mismatches) > show_limit:
                            lines.append(f"\n*({len(mismatches) - show_limit} more mismatches not shown)*")

                        output += "\n".join(lines)
            except (json.JSONDecodeError, KeyError):
                pass

            # 4b) Stack-layout one-liner (only when actionable)
            try:
                data = json.loads(json_output)
                stack_line = _stack_signal_summary(data.get("instructions", []))
                if stack_line:
                    output += f"\n\n{stack_line}"
            except (json.JSONDecodeError, KeyError):
                pass

            # 5) Auto-diagnose for non-concise mode
            if not concise:
                try:
                    parsed = json.loads(json_output)
                    match_pct = parsed.get("fuzzy_match_percent", 100)
                    if match_pct < 95:
                        diff_inspect_script = self.project_root / "scripts" / "analysis" / "diff_inspect.py"
                        if diff_inspect_script.exists():
                            tmp_json = Path(tempfile.mktemp(suffix=".json", dir="/tmp/claude"))
                            tmp_json.parent.mkdir(parents=True, exist_ok=True)
                            with open(tmp_json, "w") as f:
                                f.write(json_output)

                            diag_result = subprocess.run(
                                [sys.executable, str(diff_inspect_script), str(tmp_json), "--diagnose"],
                                capture_output=True, text=True, timeout=30,
                            )
                            if diag_result.returncode == 0 and diag_result.stdout.strip():
                                output += "\n\n## Auto-Diagnosis\n\n" + diag_result.stdout.strip()

                            try:
                                tmp_json.unlink()
                            except OSError:
                                pass
                except Exception:
                    pass

            if stderr_text:
                output += f"\n\n[stderr]\n{stderr_text}"

            lines = output.split("\n")
            line_count = len(lines)

            if line_count < MAX_INLINE_LINES:
                return [TextContent(type="text", text=output)]
            else:
                analysis_dir = project_dir / "function_analysis"
                analysis_dir.mkdir(exist_ok=True, parents=True)
                safe_symbol = re.sub(r'[<>?@*]', '_', symbol)
                output_file = analysis_dir / f"objdiff_{safe_symbol}.md"
                with open(output_file, "w") as f:
                    f.write(output)

                summary = ""
                try:
                    data = json.loads(json_output)
                    match_pct = data.get("fuzzy_match_percent", "?")
                    verdict = data.get("verdict", {}).get("classification", "UNKNOWN")
                    summary = f"**Match: {match_pct}% | Verdict: {verdict}**\n\n"
                except (json.JSONDecodeError, KeyError):
                    pass

                return [TextContent(
                    type="text",
                    text=f"""{summary}Output is large ({line_count} lines). Written to file.

**File:** `{output_file.relative_to(project_dir)}`

Read in chunks of 200 lines.
"""
                )]

        except subprocess.TimeoutExpired:
            return [TextContent(type="text", text="Error: objdiff timed out after 5 minutes.")]
        except Exception as e:
            return [TextContent(type="text", text=f"Error running objdiff: {e}")]

    async def _batch_objdiff(self, args: dict) -> list[TextContent]:
        """Build once, diff many. Classify each as COSMETIC|REAL_DIFF|BORDERLINE."""
        symbols = args.get("symbols", [])
        units = args.get("units") or []
        project_dir_arg = args.get("project_dir")

        if not symbols or not isinstance(symbols, list):
            return [TextContent(type="text", text="Error: 'symbols' must be a non-empty array.")]

        if project_dir_arg:
            project_dir = Path(project_dir_arg)
            if not project_dir.exists():
                return [TextContent(
                    type="text",
                    text=f"Error: project_dir does not exist: {project_dir}",
                )]
        elif os.environ.get("REPO_ROOT"):
            project_dir = Path(os.environ["REPO_ROOT"])
        else:
            project_dir = self.project_root

        objdiff_cli = project_dir / "bin" / "objdiff-cli"
        if not objdiff_cli.exists():
            which_result = subprocess.run(
                ["which", "objdiff-cli"], capture_output=True, text=True
            )
            if which_result.returncode == 0:
                objdiff_cli = Path(which_result.stdout.strip())
            else:
                return [TextContent(
                    type="text",
                    text="Error: objdiff-cli not found in project bin/ or system PATH",
                )]

        # Import the classifier lazily so the server can still start if the
        # diff_inspect dependency is broken.
        try:
            from orchestrator.classify_equivalent import classify
        except ImportError as e:
            return [TextContent(
                type="text",
                text=f"Error: cannot import classifier: {e}",
            )]

        def _diff_one(symbol: str, unit: str | None, do_build: bool) -> dict:
            cmd = [
                str(objdiff_cli),
                "diff",
                "-p", str(project_dir),
                symbol,
                "--verdict",
                "--include-instructions",
            ]
            if unit:
                cmd.extend(["-u", unit])
            if do_build:
                cmd.append("--build")
            cmd.extend(["-f", "json"])
            r = subprocess.run(
                cmd, capture_output=True, text=True, timeout=300,
                cwd=str(project_dir),
            )
            out = r.stdout
            start = out.find("{")
            if start < 0:
                return {"error": "no JSON output", "stderr": _filter_build_output(r.stderr)}
            try:
                return json.loads(out[start:])
            except json.JSONDecodeError as e:
                return {"error": f"json decode: {e}"}

        results = []
        # First symbol: do the build. Subsequent: skip the build flag — the
        # ninja-locked acquisition has already happened and the .o files
        # are current.
        for i, sym in enumerate(symbols):
            unit = units[i] if i < len(units) else None
            data = _diff_one(sym, unit, do_build=(i == 0))
            entry = {
                "symbol": sym,
                "unit": unit,
            }
            if "error" in data:
                entry.update({"error": data["error"]})
                if data.get("stderr"):
                    entry["stderr"] = data["stderr"][:400]
                entry["classification"] = "ERROR"
                results.append(entry)
                continue
            entry["fuzzy_pct"] = round(data.get("fuzzy_match_percent", 0.0), 2)
            entry["raw_pct"] = round(data.get("raw_match_percent", 0.0), 2)
            entry["verdict"] = (data.get("verdict") or {}).get("classification", "")
            cls = classify(data)
            entry["classification"] = cls.label
            entry["rationale"] = cls.rationale
            results.append(entry)

        # Summary header + table.
        from collections import Counter
        labels = Counter(r["classification"] for r in results)
        lines = [
            f"Batch objdiff: {len(results)} symbol(s) — "
            + ", ".join(f"{k}={v}" for k, v in labels.most_common())
        ]
        lines.append("")
        lines.append(
            f"{'verdict':<14}  {'fuzzy':>7}  {'raw':>7}  classification  symbol"
        )
        lines.append("-" * 90)
        for r in results:
            fmt_fuzzy = f"{r.get('fuzzy_pct', 0):.1f}%" if "fuzzy_pct" in r else "    -"
            fmt_raw = f"{r.get('raw_pct', 0):.1f}%" if "raw_pct" in r else "    -"
            lines.append(
                f"{r.get('verdict', '?'):<14}  {fmt_fuzzy:>7}  {fmt_raw:>7}  "
                f"{r['classification']:<13}   {r['symbol']}"
            )
        lines.append("")
        lines.append("# JSON")
        lines.append(json.dumps(results, indent=2))
        return [TextContent(type="text", text="\n".join(lines))]

    async def _run_diff_inspect(self, args: dict) -> list[TextContent]:
        """Handle run_diff_inspect tool call."""
        symbol = args.get("symbol", "")
        mode = args.get("mode", "")
        project_dir_arg = args.get("project_dir", None)
        baseline_json = args.get("baseline_json", None)
        unit = args.get("unit", None)

        if not symbol:
            return [TextContent(type="text", text="Error: No symbol provided.")]
        if not mode:
            return [TextContent(type="text", text="Error: No mode provided.")]

        valid_modes = {"diagnose", "clusters", "regswaps", "offsets", "replaces", "compare", "save_baseline", "mismatches", "stack-layout"}
        if mode not in valid_modes:
            return [TextContent(type="text", text=f"Error: Invalid mode '{mode}'. Valid: {', '.join(sorted(valid_modes))}")]

        # Determine project directory
        if project_dir_arg:
            project_dir = Path(project_dir_arg)
            if not project_dir.exists():
                return [TextContent(type="text", text=f"Error: project_dir does not exist: {project_dir}")]
        elif os.environ.get("REPO_ROOT"):
            project_dir = Path(os.environ["REPO_ROOT"])
        else:
            project_dir = self.project_root

        safe_symbol = re.sub(r'[<>?@*]', '_', symbol)

        diff_inspect_script = self.project_root / "scripts" / "analysis" / "diff_inspect.py"

        # Find objdiff-cli
        objdiff_cli = project_dir / "bin" / "objdiff-cli"
        if not objdiff_cli.exists():
            which_result = subprocess.run(
                ["which", "objdiff-cli"], capture_output=True, text=True
            )
            if which_result.returncode == 0:
                objdiff_cli = Path(which_result.stdout.strip())
            else:
                return [TextContent(type="text", text="Error: objdiff-cli not found")]

        try:
            # -- save_baseline mode --
            if mode == "save_baseline":
                cmd = [
                    str(objdiff_cli), "diff",
                    "-p", str(project_dir),
                    symbol,
                    "--include-instructions", "--build", "--incremental",
                    "-f", "json",
                ]
                if unit:
                    cmd.extend(["-u", unit])
                result = subprocess.run(
                    cmd, capture_output=True, text=True,
                    timeout=300, cwd=str(project_dir),
                )
                if result.returncode != 0:
                    return [TextContent(type="text", text=f"Error running objdiff: {result.stderr or result.stdout}")]

                analysis_dir = project_dir / "function_analysis"
                analysis_dir.mkdir(exist_ok=True, parents=True)
                baseline_file = analysis_dir / f"baseline_{safe_symbol}.json"
                with open(baseline_file, "w") as f:
                    f.write(result.stdout)

                return [TextContent(type="text", text=f"Baseline saved: `{baseline_file}`")]

            # -- compare mode --
            elif mode == "compare":
                if baseline_json:
                    baseline_path = Path(baseline_json)
                else:
                    baseline_path = project_dir / "function_analysis" / f"baseline_{safe_symbol}.json"

                if not baseline_path.exists():
                    return [TextContent(type="text", text=f"Error: No baseline found at `{baseline_path}`.\n"
                                        "Use `save_baseline` mode first.")]

                cmd = [
                    str(objdiff_cli), "diff",
                    "-p", str(project_dir),
                    symbol,
                    "--include-instructions", "--build", "--incremental",
                    "-f", "json",
                ]
                if unit:
                    cmd.extend(["-u", unit])
                result = subprocess.run(
                    cmd, capture_output=True, text=True,
                    timeout=300, cwd=str(project_dir),
                )
                if result.returncode != 0:
                    return [TextContent(type="text", text=f"Error running objdiff: {result.stderr or result.stdout}")]

                current_file = Path(tempfile.mktemp(suffix=".json", dir="/tmp/claude"))
                current_file.parent.mkdir(parents=True, exist_ok=True)
                with open(current_file, "w") as f:
                    f.write(result.stdout)

                if diff_inspect_script.exists():
                    compare_cmd = [
                        sys.executable, str(diff_inspect_script),
                        "--compare", str(baseline_path), str(current_file),
                    ]
                    compare_result = subprocess.run(
                        compare_cmd, capture_output=True, text=True, timeout=60,
                    )
                    try:
                        current_file.unlink()
                    except OSError:
                        pass

                    output = compare_result.stdout
                    if compare_result.stderr:
                        output += f"\n[stderr] {compare_result.stderr.strip()}"
                    return [TextContent(type="text", text=output)]
                else:
                    try:
                        current_file.unlink()
                    except OSError:
                        pass
                    return [TextContent(type="text", text="Error: diff_inspect.py not found. Compare mode requires it.")]

            # -- mismatches mode --
            elif mode == "mismatches":
                cmd = [
                    str(objdiff_cli), "diff",
                    "-p", str(project_dir),
                    symbol,
                    "--include-instructions", "--build", "--incremental",
                    "-f", "json",
                ]
                if unit:
                    cmd.extend(["-u", unit])
                result = subprocess.run(
                    cmd, capture_output=True, text=True,
                    timeout=300, cwd=str(project_dir),
                )

                stderr_text = result.stderr.strip() if result.stderr else ""
                if result.returncode != 0:
                    return [TextContent(type="text", text=f"Error running objdiff (exit {result.returncode}):\n{result.stdout}\n{stderr_text}")]

                stdout_text = result.stdout
                json_start = stdout_text.find("{")
                if json_start < 0:
                    return [TextContent(type="text", text=f"No JSON in objdiff output.")]

                try:
                    data = json.loads(stdout_text[json_start:])
                except json.JSONDecodeError as e:
                    return [TextContent(type="text", text=f"Error parsing objdiff JSON: {e}")]

                instrs = data.get("instructions", [])
                if not instrs:
                    return [TextContent(type="text", text="No instructions found in objdiff output.")]

                mismatches = [ins for ins in instrs if ins.get("match_type") != "equal"]
                total = len(instrs)

                if not mismatches:
                    match_pct = data.get("fuzzy_match_percent", 100)
                    return [TextContent(type="text", text=f"No mismatches -- all {total} instructions match ({match_pct}%).")]

                MAX_MISMATCHES = 30
                truncated = len(mismatches) > MAX_MISMATCHES
                shown = mismatches[:MAX_MISMATCHES]

                header = f"## Mismatched Instructions ({len(mismatches)} of {total} total)\n"
                if truncated:
                    header += f"*Showing {MAX_MISMATCHES} of {len(mismatches)} mismatches*\n"

                lines = [header]
                lines.append("| Idx | Type | Target | Base |")
                lines.append("|-----|------|--------|------|")

                for ins in shown:
                    idx = ins.get("index", "?")
                    mt = ins.get("match_type", "?")
                    t = ins.get("target", {})
                    b = ins.get("base", {})
                    t_str = f"{t.get('opcode', '---')} {t.get('args', '')}" if t else "---"
                    b_str = f"{b.get('opcode', '---')} {b.get('args', '')}" if b else "---"
                    lines.append(f"| {idx} | {mt} | `{t_str}` | `{b_str}` |")

                if truncated:
                    lines.append(f"\n*{len(mismatches) - MAX_MISMATCHES} more mismatches not shown.*")

                output = "\n".join(lines)
                return [TextContent(type="text", text=output)]

            # -- stack-layout mode (uses a different script) --
            elif mode == "stack-layout":
                stack_script = self.project_root / "scripts" / "analysis" / "stack_layout.py"
                if not stack_script.exists():
                    return [TextContent(type="text", text=f"Error: stack_layout.py not found at {stack_script}")]

                cmd = [
                    sys.executable, str(stack_script),
                    "--symbol", symbol,
                    "--project-dir", str(project_dir),
                ]
                if unit:
                    cmd.extend(["--unit", unit])
                result = subprocess.run(
                    cmd, capture_output=True, text=True,
                    timeout=300,
                )

                output = result.stdout
                if result.stderr:
                    filtered_stderr = _filter_build_output(result.stderr)
                    if filtered_stderr:
                        output += f"\n\n[stderr]\n{filtered_stderr}"

                if result.returncode != 0:
                    return [TextContent(type="text", text=f"Error (exit {result.returncode}):\n{output}")]

                lines_count = len(output.split("\n"))
                if lines_count < MAX_INLINE_LINES:
                    return [TextContent(type="text", text=output)]
                else:
                    analysis_dir = project_dir / "function_analysis"
                    analysis_dir.mkdir(exist_ok=True, parents=True)
                    output_file = analysis_dir / f"stack_layout_{safe_symbol}.txt"
                    with open(output_file, "w") as f:
                        f.write(output)
                    return [TextContent(type="text", text=f"Output is large ({lines_count} lines). Written to file.\n\n"
                                        f"**File:** `{output_file.relative_to(project_dir)}`")]

            # -- analysis modes (diagnose/clusters/regswaps/offsets/replaces) --
            else:
                if not diff_inspect_script.exists():
                    return [TextContent(type="text", text=f"Error: diff_inspect.py not found at {diff_inspect_script}")]

                cmd = [
                    sys.executable, str(diff_inspect_script),
                    "--symbol", symbol,
                    f"--{mode}",
                    "--project-dir", str(project_dir),
                ]
                if unit:
                    cmd.extend(["--unit", unit])
                result = subprocess.run(
                    cmd, capture_output=True, text=True,
                    timeout=300,
                )

                output = result.stdout
                if result.stderr:
                    filtered_stderr = _filter_build_output(result.stderr)
                    if filtered_stderr:
                        output += f"\n\n[stderr]\n{filtered_stderr}"

                if result.returncode != 0:
                    return [TextContent(type="text", text=f"Error (exit {result.returncode}):\n{output}")]

                lines = output.split("\n")
                if len(lines) < MAX_INLINE_LINES:
                    return [TextContent(type="text", text=output)]
                else:
                    analysis_dir = project_dir / "function_analysis"
                    analysis_dir.mkdir(exist_ok=True, parents=True)
                    output_file = analysis_dir / f"diff_inspect_{mode}_{safe_symbol}.txt"
                    with open(output_file, "w") as f:
                        f.write(output)
                    return [TextContent(type="text", text=f"Output is large ({len(lines)} lines). Written to file.\n\n"
                                        f"**File:** `{output_file.relative_to(project_dir)}`")]

        except subprocess.TimeoutExpired:
            return [TextContent(type="text", text=f"Error: diff_inspect timed out (mode={mode}).")]
        except Exception as e:
            return [TextContent(type="text", text=f"Error running diff_inspect: {e}")]

    async def _mark_patch_result(self, args: dict) -> list[TextContent]:
        """Handle mark_patch_result tool call (stub for now)."""
        queue_id = args.get("patch_queue_id")
        status = args.get("status", "")
        reason = args.get("reason", "")

        if queue_id is None:
            return [TextContent(type="text", text="Error: patch_queue_id is required.")]
        if not status:
            return [TextContent(type="text", text="Error: status is required.")]

        return [TextContent(type="text", text=f"Patch {queue_id} marked as {status}. Reason: {reason or 'N/A'}")]

    def _load_dc3_report(self) -> dict[str, float]:
        """Parse DC3's report.json into {unit_name: matched_functions_percent}.

        Cached on the instance and keyed by the report file's mtime; the JSON is
        re-parsed only when the file changes. Returns an empty dict if the
        report does not exist or cannot be parsed.
        """
        report_path = Path(self.dc3_report_path)
        try:
            mtime = report_path.stat().st_mtime
        except OSError:
            return {}

        if self._dc3_report_cache is not None and self._dc3_report_mtime == mtime:
            return self._dc3_report_cache

        try:
            with open(report_path) as f:
                report = json.load(f)
        except (OSError, json.JSONDecodeError):
            return {}

        cache: dict[str, float] = {}
        for unit in report.get("units", []):
            name = unit.get("name")
            if not name:
                continue
            measures = unit.get("measures", {}) or {}
            pct = measures.get("matched_functions_percent")
            if pct is not None:
                cache[name] = pct

        self._dc3_report_cache = cache
        self._dc3_report_mtime = mtime
        return cache

    def _dc3_file_to_unit(self, file_path: str) -> str | None:
        """Map an absolute DC3 source path to its DC3 unit name.

        Strips the DC3 src-root prefix and the file extension, prepends
        'default/'. For a '.h' file, maps to the unit of the sibling '.cpp'
        when that file exists on disk. Returns None when no unit can be
        derived (header with no sibling .cpp).
        """
        p = Path(file_path)
        if p.suffix == ".h":
            sibling = p.with_suffix(".cpp")
            if not sibling.exists():
                return None
            p = sibling

        try:
            rel = p.resolve().relative_to(Path(self.dc3_path).resolve())
        except ValueError:
            return None

        rel_no_ext = rel.with_suffix("")
        return f"default/{rel_no_ext.as_posix()}"

    async def _lookup_dc3(self, args: dict) -> list[TextContent]:
        """Search DC3 source for a method/class name (shared Milo engine reference)."""
        symbol = args.get("symbol", "")
        if not symbol:
            return [TextContent(type="text", text="No symbol provided.")]
        min_match = args.get("min_match", 0)

        # Extract a searchable name from MWCC mangling or qualified C++ name.
        # MWCC examples (RB3): 'Poll__13CharServoBoneFv', '__dt__6MyObjFv',
        # '__ct__6MyObjFv'. Qualified: 'CharServoBone::Poll'. Plain: 'Poll'.
        search_term = symbol
        if "::" in symbol:
            search_term = symbol.split("::")[-1]
        elif "__" in symbol:
            before, _, rest = symbol.partition("__")
            if before:
                # Regular MWCC: 'Method__<class><args>' -> 'Method'
                search_term = before
            else:
                # Leading-underscore form: '__dt__6MyObjFv' / '__ct__6MyObjFv'.
                # Pull the class name out of the <length><name> encoding so
                # the grep finds the class definition.
                m = re.match(r"^[a-zA-Z]+__(\d+)([A-Za-z_][\w]*)", rest)
                if m:
                    n = int(m.group(1))
                    search_term = m.group(2)[:n] or symbol

        dc3_path = Path(self.dc3_path)
        if not dc3_path.exists():
            return [TextContent(
                type="text",
                text=f"DC3 source path not found: {dc3_path}\n"
                     f"Pass --dc3 <path> to mcp_server, or set the source dir under "
                     f"~/code/milohax/dc3-decomp/src.",
            )]

        try:
            result = subprocess.run(
                ["grep", "-rn", "--include=*.cpp", "--include=*.h",
                 search_term, str(dc3_path)],
                capture_output=True, text=True, timeout=30,
            )
        except subprocess.TimeoutExpired:
            return [TextContent(type="text", text="DC3 search timed out.")]
        except Exception as e:
            return [TextContent(type="text", text=f"DC3 search error: {e}")]

        stdout = result.stdout.strip()
        if not stdout:
            return [TextContent(type="text", text=f"No matches found in DC3 for: {search_term}")]

        # Dedup grep line-hits to one row per source file, counting hits.
        file_hits: dict[str, int] = {}
        for line in stdout.split("\n"):
            # grep -rn output: "<path>:<lineno>:<text>"
            file_part = line.split(":", 1)[0]
            if file_part:
                file_hits[file_part] = file_hits.get(file_part, 0) + 1

        if not file_hits:
            return [TextContent(type="text", text=f"No matches found in DC3 for: {search_term}")]

        # Score each file by its DC3 unit's matched_functions_percent.
        dc3_report = self._load_dc3_report()
        rows = []
        for file_path, hits in file_hits.items():
            unit = self._dc3_file_to_unit(file_path)
            if unit is None:
                # Header with no sibling .cpp: no unit score, sort last.
                rows.append({
                    "file": file_path, "unit": None,
                    "percent": None, "hits": hits,
                })
            else:
                pct = dc3_report.get(unit)
                rows.append({
                    "file": file_path, "unit": unit,
                    "percent": pct, "hits": hits,
                })

        # Apply min_match filter (rows with no unit score are excluded by a
        # positive threshold; never silently — count and report them).
        filtered_out = 0
        if min_match and min_match > 0:
            kept = []
            for r in rows:
                pct = r["percent"]
                if pct is not None and pct >= min_match:
                    kept.append(r)
                else:
                    filtered_out += 1
            rows = kept

        # Sort by unit matched_functions_percent descending; unscored last.
        rows.sort(key=lambda r: (r["percent"] is None, -(r["percent"] or 0.0)))

        total_files = len(file_hits)
        shown = rows[:20]

        if not shown:
            msg = f"No DC3 files for '{search_term}' meet the criteria."
            if filtered_out:
                msg += f"\n{filtered_out} hits filtered out below {min_match}%."
            return [TextContent(type="text", text=msg)]

        output = (
            f"DC3 matches for '{search_term}' "
            f"({len(shown)} of {total_files} files, ranked by DC3 unit match%):\n\n"
        )
        for r in shown:
            if r["unit"] is None:
                unit_str = "(header, no unit)"
                pct_str = "n/a"
            else:
                unit_str = r["unit"]
                pct_str = f"{r['percent']:.1f}%" if r["percent"] is not None else "unknown"
            output += f"- {r['file']}\n"
            output += f"  Unit: {unit_str} | Match: {pct_str} | Hits: {r['hits']}\n"

        if filtered_out:
            output += f"\n{filtered_out} hits filtered out below {min_match}%."
        if total_files > 20:
            output += f"\n\n... and {total_files - 20} more files"

        return [TextContent(type="text", text=output)]

    async def run(self):
        """Run the MCP server."""
        async with stdio_server() as (read_stream, write_stream):
            await self.server.run(read_stream, write_stream, self.server.create_initialization_options())


def main():
    parser = argparse.ArgumentParser(description="RB3 Decomp MCP Server")
    parser.add_argument("--db", default="decomp.db", help="Database path")
    parser.add_argument(
        "--no-record-attempts",
        action="store_true",
        help="Don't record attempts in report_result",
    )
    parser.add_argument(
        "--dc3",
        default=None,
        help="DC3 source path for lookup_dc3 (default: ~/code/milohax/dc3-decomp/src)",
    )
    args = parser.parse_args()

    server = DecompMCPServer(
        db_path=args.db,
        record_attempts=not args.no_record_attempts,
        dc3_path=args.dc3,
    )
    asyncio.run(server.run())


if __name__ == "__main__":
    main()
