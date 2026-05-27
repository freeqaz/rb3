#!/usr/bin/env python3
"""
Bulk-decompile in-scope RB3 functions via pyghidra-mcp.

Goals:
  1. Pre-populate cache.db under the new PowerPC:BE:32:Gekko_Broadway variant.
  2. Surface gaps in gekko.sinc — any function whose Ghidra output contains
     halt_baddata/Bad instruction is an instruction encoding the new SLEIGH
     does not yet handle.
  3. Surface MCP/server errors.

Scope: src/band3/ and src/system/ excluding system/rndwii/ + system/os/ (per the
"port-surface" filter in CLAUDE.md). 30k+ functions.
"""

from __future__ import annotations
import argparse
import json
import sys
import time
import threading
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "ghidra"))

from mcp_client import MCPClient, MCPError  # noqa: E402

ELF_PATH = (
    "/home/free/code/milohax/milo-executable-library/"
    "rb3/Wii Proto (Bank 5) (Debug)/band_r_wii.elf"
)


def load_elf_symbols() -> dict[str, str]:
    """mangled-symbol -> ELF VA (hex string). Single readelf call."""
    import subprocess
    out = subprocess.run(
        ["readelf", "-sW", ELF_PATH], check=True, capture_output=True, text=True
    ).stdout
    mapping: dict[str, str] = {}
    for line in out.splitlines():
        # Format: "  Num: Value     Size Type    Bind ... Ndx Name"
        parts = line.split()
        if len(parts) < 8:
            continue
        if parts[3] != "FUNC":
            continue
        va = parts[1]
        name = parts[-1]
        # Earlier-defined wins (matches the linker order)
        mapping.setdefault(name, f"0x{va}")
    return mapping


def in_scope(unit_name: str) -> bool:
    n = unit_name.removeprefix("main/")
    if n.startswith("band3/"):
        return True
    if n.startswith("system/"):
        if n.startswith("system/rndwii/") or n.startswith("system/os/"):
            return False
        return True
    return False


def load_targets(report_path: Path, limit: int | None = None) -> list[tuple[str, str, str]]:
    """Returns [(virtual_address_hex, symbol, unit_name), ...] for in-scope functions."""
    r = json.loads(report_path.read_text())
    out = []
    for u in r["units"]:
        if not in_scope(u["name"]):
            continue
        for f in u.get("functions", []):
            va = f["metadata"].get("virtual_address")
            if not va or va == "0":
                continue
            out.append((f"0x{int(va):08x}", f["name"], u["name"]))
    if limit is not None:
        out = out[:limit]
    return out


def categorize(code: str) -> str:
    """Bucket the Ghidra output to detect SLEIGH gaps."""
    if not code:
        return "empty"
    if "halt_baddata" in code:
        return "halt_baddata"
    if "Bad instruction - Truncating control flow" in code:
        return "bad_instruction"
    if "Truncating control flow" in code:
        return "truncated"
    return "ok"


def worker(client: MCPClient, ref: str) -> tuple[str, str, str]:
    """Returns (ref, category, detail) for the given function name or address."""
    try:
        result = client.decompile_function(ref)
    except MCPError as e:
        return (ref, "mcp_error", str(e)[:200])
    except Exception as e:
        return (ref, "exception", f"{type(e).__name__}: {e}"[:200])

    if isinstance(result, dict):
        code = result.get("code") or result.get("decompiled_code") or result.get("decompilation") or ""
    else:
        code = str(result)
    return (ref, categorize(code), "")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--workers", type=int, default=8, help="parallel decompile workers (default 8)")
    ap.add_argument("--limit", type=int, default=None, help="limit number of functions (smoke test)")
    ap.add_argument(
        "--out",
        type=Path,
        default=ROOT / "logs" / "ghidra-sweep.jsonl",
        help="per-function results jsonl",
    )
    ap.add_argument(
        "--errors-out",
        type=Path,
        default=ROOT / "logs" / "ghidra-sweep-errors.jsonl",
        help="errors-only jsonl (smaller, easier to grep)",
    )
    args = ap.parse_args()

    targets = load_targets(ROOT / "build/SZBE69_B8/report.json", limit=args.limit)
    print(f"in-scope targets (Bank 8): {len(targets)}", flush=True)

    # report.json has Bank-8 VAs; the open Ghidra program is the Bank-5 ELF
    # with full DWARF. Map mangled-symbol -> Bank-5 VA from the ELF itself.
    elf_syms = load_elf_symbols()
    print(f"ELF symbols (Bank 5): {len(elf_syms)}", flush=True)
    mapped, missing = [], []
    for addr_b8, sym, unit in targets:
        va_b5 = elf_syms.get(sym)
        if va_b5:
            mapped.append((va_b5, sym, unit))
        else:
            missing.append((sym, unit))
    print(f"resolved against ELF: {len(mapped)} ({len(missing)} unmatched)", flush=True)
    targets = mapped

    # One shared client. session_id is set once and reused for all threads;
    # requests.post is safe to call from multiple threads (no shared Session).
    client = MCPClient()
    client.initialize()
    _ = client.binary  # force binary-name resolution before the thread pool
    print(f"binary: {client.binary}", flush=True)
    print(f"session: {client.session_id[:8]}...", flush=True)

    args.out.parent.mkdir(parents=True, exist_ok=True)

    counts = {"ok": 0, "halt_baddata": 0, "bad_instruction": 0, "truncated": 0,
              "mcp_error": 0, "exception": 0, "empty": 0}
    lock = threading.Lock()
    started_at = time.time()
    n_total = len(targets)

    with args.out.open("w") as fout, args.errors_out.open("w") as ferr:
        with ThreadPoolExecutor(max_workers=args.workers) as pool:
            # Targets are (bank5_va, sym, unit). Pass the Bank-5 ELF address
            # which is what Ghidra's program has loaded.
            futures = {
                pool.submit(worker, client, va_b5): (va_b5, sym, unit)
                for va_b5, sym, unit in targets
            }
            n_done = 0
            for fut in as_completed(futures):
                addr, sym, unit = futures[fut]
                _, category, detail = fut.result()
                row = {"addr": addr, "sym": sym, "unit": unit, "category": category}
                if detail:
                    row["detail"] = detail
                fout.write(json.dumps(row) + "\n")
                if category != "ok":
                    ferr.write(json.dumps(row) + "\n")
                    ferr.flush()
                with lock:
                    counts[category] = counts.get(category, 0) + 1
                n_done += 1
                if n_done % 100 == 0 or n_done == n_total:
                    elapsed = time.time() - started_at
                    rate = n_done / elapsed if elapsed > 0 else 0
                    eta = (n_total - n_done) / rate if rate > 0 else 0
                    print(
                        f"  {n_done}/{n_total}  ok={counts['ok']:>5}  "
                        f"halt={counts['halt_baddata']:>3}  bad={counts['bad_instruction']:>3}  "
                        f"err={counts['mcp_error']:>3}  exc={counts['exception']:>3}  "
                        f"rate={rate:.1f}/s  eta={eta/60:.0f}m",
                        flush=True,
                    )

    print()
    print("Summary:")
    for k, v in sorted(counts.items(), key=lambda kv: -kv[1]):
        if v:
            print(f"  {k:16s} {v}")
    print(f"Per-function log: {args.out}")
    print(f"Errors-only log:  {args.errors_out}")


if __name__ == "__main__":
    main()
