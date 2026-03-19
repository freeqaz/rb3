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
    get_attempts_for_function,
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

    def __init__(self, db_path: str, record_attempts: bool = True):
        self.db_path = db_path
        self.record_attempts = record_attempts
        # Determine project root from script location
        self.project_root = Path(__file__).resolve().parent.parent.parent
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
                                "description": "Filter by function status: 'workable' (default), 'all', 'complete', 'at_limit'",
                                "enum": ["workable", "all", "complete", "at_limit"],
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
                    description="Deep analysis of WHY a function doesn't match. Provides root cause diagnosis, cluster analysis, register swap detection, offset analysis. Use after run_objdiff when you need deeper insight.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "symbol": {
                                "type": "string",
                                "description": "Function symbol (mangled or demangled name)",
                            },
                            "mode": {
                                "type": "string",
                                "enum": ["diagnose", "clusters", "regswaps", "offsets", "replaces", "compare", "save_baseline", "mismatches"],
                                "description": "Analysis mode: diagnose (root cause), clusters (contiguous groups), regswaps (register swap pairs), offsets (offset shift histogram), replaces (categorize noise vs real), compare (delta vs baseline), save_baseline, mismatches (instruction table)",
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
            ]

        @self.server.call_tool()
        async def call_tool(name: str, arguments: dict) -> list[TextContent]:
            if name == "report_result":
                return await self._report_result(arguments)
            elif name == "query_functions":
                return await self._query_functions(arguments)
            elif name == "get_attempts":
                return await self._get_attempts(arguments)
            elif name == "run_objdiff":
                return await self._run_objdiff(arguments)
            elif name == "run_diff_inspect":
                return await self._run_diff_inspect(arguments)
            elif name == "mark_patch_result":
                return await self._mark_patch_result(arguments)
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

        valid_modes = {"diagnose", "clusters", "regswaps", "offsets", "replaces", "compare", "save_baseline", "mismatches"}
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
    args = parser.parse_args()

    server = DecompMCPServer(
        db_path=args.db,
        record_attempts=not args.no_record_attempts,
    )
    asyncio.run(server.run())


if __name__ == "__main__":
    main()
