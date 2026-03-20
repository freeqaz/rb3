#!/usr/bin/env python3
"""
analyze-function: Combined objdiff + Ghidra + m2c analysis for RB3 decompilation.

Usage:
    tools/analyze_function.py RecordTrillStats__9GemPlayerFv
    tools/analyze_function.py -u game/GemPlayer RecordTrillStats__9GemPlayerFv
    tools/analyze_function.py --no-ghidra --no-m2c RecordTrillStats__9GemPlayerFv
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OBJDIFF_CLI = ROOT / "bin" / "objdiff-cli"
M2C = ROOT.parent / "m2c" / "m2c.py"
ASM_DIR = ROOT / "build/SZBE69_B8/asm"

sys.path.insert(0, str(ROOT / "tools" / "ghidra"))
try:
    from mcp_client import MCPClient, MCPError
    GHIDRA_AVAILABLE = True
except ImportError:
    GHIDRA_AVAILABLE = False


# =============================================================================
# objdiff
# =============================================================================

def normalize_unit(unit: str) -> str:
    """Normalize unit path to objdiff's 'main/...' format."""
    unit = unit.removeprefix("main/")
    # If the unit doesn't start with a known top-level dir, find it by searching asm files
    top = ("band3/", "system/", "network/", "lib/", "sdk/")
    if not any(unit.startswith(p) for p in top):
        # Try finding the asm file to deduce correct prefix
        for prefix in top:
            if (ASM_DIR / prefix / (unit + ".s")).exists():
                unit = prefix + unit
                break
    return f"main/{unit}"


def run_objdiff(function: str, unit: str | None = None) -> dict:
    cmd = [
        str(OBJDIFF_CLI), "diff",
        function,
        "--format", "json",
        "-o", "-",
        "--include-instructions",
        "--summary",
        "--analyze",
        "--verdict",
    ]
    if unit:
        cmd.extend(["-u", normalize_unit(unit)])

    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    if proc.returncode != 0:
        return {"error": proc.stderr.strip()}

    try:
        return json.loads(proc.stdout)
    except json.JSONDecodeError as e:
        return {"error": f"JSON parse failed: {e}"}


def format_objdiff(data: dict) -> str:
    if "error" in data:
        return f"objdiff error: {data['error']}"

    match_pct = data.get("fuzzy_match_percent", 0.0)
    symbol = data.get("symbol", "?")
    target_size = data.get("target_size", 0)

    lines = [f"Match: {match_pct:.1f}%  |  Symbol: {symbol}  |  Size: {target_size} bytes"]

    # Verdict / patterns
    verdict = data.get("verdict")
    if verdict:
        vtype = verdict.get("type", "")
        patterns = verdict.get("patterns", [])
        pat_str = ", ".join(patterns) if patterns else "none"
        lines.append(f"Verdict: {vtype}  |  Patterns: {pat_str}")

    # Instruction summary
    summary = data.get("instruction_summary", {})
    if summary:
        counts = {k: v for k, v in summary.items() if v and k != "total"}
        if counts:
            lines.append("Summary: " + "  ".join(f"{k}={v}" for k, v in counts.items()))

    # Mismatches from instruction list
    instructions = data.get("instructions", [])
    mismatches = [i for i in instructions if i.get("match_type", "equal") != "equal"]
    if mismatches:
        lines.append("")
        lines.append(f"{'Idx':<5} {'Kind':<12} {'Target':<36} {'Base'}")
        lines.append("-" * 80)
        for m in mismatches[:20]:
            idx = str(m.get("index", "?"))
            kind = m.get("match_type", "?")
            tgt = m.get("target") or {}
            base = m.get("base") or {}
            tgt_str = (tgt.get("opcode", "") + " " + tgt.get("args", "")).strip() or "—"
            base_str = (base.get("opcode", "") + " " + base.get("args", "")).strip() or "—"
            lines.append(f"{idx:<5} {kind:<12} {tgt_str:<36} {base_str}")
        if len(mismatches) > 20:
            lines.append(f"  ... +{len(mismatches) - 20} more mismatches")

    return "\n".join(lines)


# =============================================================================
# Ghidra
# =============================================================================

def run_ghidra(function: str) -> str:
    if not GHIDRA_AVAILABLE:
        return "Ghidra client not available (import failed)"

    try:
        client = MCPClient()
        client.initialize()

        # Resolve: search for the mangled symbol to get its address, then decompile
        addr = None
        try:
            sym_result = client.search_symbols(function, limit=5)
            syms = sym_result.get("symbols", []) if isinstance(sym_result, dict) else []
            # Prefer Function type over Label
            for sym in sorted(syms, key=lambda s: (s.get("type") != "Function")):
                if function in (sym.get("name", ""), sym.get("name", "").split("-")[0]):
                    addr = "0x" + sym["address"]
                    break
        except MCPError:
            pass

        target = addr or function
        result = client.decompile_function(target)

        if isinstance(result, str):
            code = result
            name = function
            resolved_addr = addr or ""
        else:
            code = result.get("decompiled_code") or result.get("code", "")
            name = result.get("name") or result.get("function_name", function)
            resolved_addr = addr or result.get("address", "")

        lines = []
        if resolved_addr:
            lines.append(f"// address: {resolved_addr}")
        if name and name != function:
            lines.append(f"// name: {name}")
        if lines:
            lines.append("")
        lines.append(code or "// (no decompilation)")
        return "\n".join(lines)

    except MCPError as e:
        return f"// Ghidra error: {e}"
    except Exception as e:
        return f"// Ghidra exception: {e}"


# =============================================================================
# m2c
# =============================================================================

def find_asm_file(function: str, unit: str | None) -> Path | None:
    if unit:
        unit = unit.removeprefix("main/")
        candidate = ASM_DIR / (unit + ".s")
        if candidate.exists():
            return candidate
        for prefix in ("band3/", "system/", "network/", "lib/", "sdk/"):
            candidate = ASM_DIR / prefix / (unit + ".s")
            if candidate.exists():
                return candidate
        return None

    needle = f".fn {function},"
    for asm_file in sorted(ASM_DIR.rglob("*.s")):
        if needle in asm_file.read_text():
            return asm_file
    return None


def run_m2c(function: str, unit: str | None = None, context: str | None = None) -> str:
    if not M2C.exists():
        return f"m2c not found at {M2C}"

    asm_file = find_asm_file(function, unit)
    if asm_file is None:
        return f"// m2c: asm file not found for '{function}'"

    cmd = [
        sys.executable, str(M2C),
        "--target", "ppc",
        "-f", function,
        "--passes", "4",
    ]
    if context:
        cmd += ["--context", context]
    cmd.append(str(asm_file))

    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    if proc.returncode != 0:
        return f"// m2c error: {proc.stderr.strip()}"
    return proc.stdout.strip()


# =============================================================================
# Output
# =============================================================================

def print_section(title: str, content: str):
    print(f"\n{'='*70}")
    print(f"  {title}")
    print(f"{'='*70}")
    print(content)


def main():
    parser = argparse.ArgumentParser(description="Analyze an RB3 function with objdiff, Ghidra, and m2c.")
    parser.add_argument("function", help="Mangled function symbol (e.g. RecordTrillStats__9GemPlayerFv)")
    parser.add_argument("-u", "--unit", help="Unit path (e.g. game/GemPlayer or main/band3/game/GemPlayer)")
    parser.add_argument("-c", "--context", help="m2c context C/C++ file for type info")
    parser.add_argument("--no-ghidra", action="store_true", help="Skip Ghidra decompilation")
    parser.add_argument("--no-m2c", action="store_true", help="Skip m2c decompilation")
    parser.add_argument("--no-objdiff", action="store_true", help="Skip objdiff analysis")
    args = parser.parse_args()

    print(f"\n# analyze-function: {args.function}")
    if args.unit:
        print(f"# unit: {args.unit}")

    if not args.no_objdiff:
        data = run_objdiff(args.function, args.unit)
        print_section("objdiff", format_objdiff(data))

    if not args.no_ghidra:
        print_section("Ghidra decompilation", run_ghidra(args.function))

    if not args.no_m2c:
        print_section("m2c decompilation", run_m2c(args.function, args.unit, args.context))


if __name__ == "__main__":
    main()
