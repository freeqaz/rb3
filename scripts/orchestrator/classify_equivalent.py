"""Deterministic classifier: COSMETIC | REAL_DIFF | BORDERLINE.

Given objdiff JSON output (the same shape `mcp_server._run_objdiff` produces
in its internal step 1), return a verdict label + rationale. The intent is
to bypass subagent judgment for the common cases:

  - COSMETIC: register swaps, offset shifts, commutative-arg flips, relocation
    noise. Promote to Equivalent without further review.
  - REAL_DIFF: opcode changes (cmpw<->cmplw), branch flips, inserts/deletes
    of different opcodes, constant differences. Worth a subagent investigation.
  - BORDERLINE: anything in between — defer to a subagent.

Implementation reuses extractors from scripts/analysis/diff_inspect.py.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "scripts"))

try:
    from analysis.diff_inspect import (
        parse_breakdowns,
        categorize_replaces,
    )
except ImportError as e:
    raise ImportError(
        f"classify_equivalent requires scripts/analysis/diff_inspect.py: {e}"
    ) from e


# Opcode classes whose substitution is *real* (semantic) rather than cosmetic.
_BRANCH_FLIPS = {
    ("beq", "bne"),
    ("bne", "beq"),
    ("ble", "bgt"),
    ("bgt", "ble"),
    ("blt", "bge"),
    ("bge", "blt"),
    ("ble", "beq"),
    ("beq", "ble"),
    ("bge", "beq"),
    ("beq", "bge"),
}
_REAL_CMP_FLIPS = {
    ("cmpw", "cmplw"),
    ("cmplw", "cmpw"),
    ("cmpwi", "cmplwi"),
    ("cmplwi", "cmpwi"),
}


@dataclass
class Classification:
    label: str         # COSMETIC | REAL_DIFF | BORDERLINE
    rationale: str
    metrics: dict[str, int]


def _normalize_opcode(s: dict | None) -> str:
    if not s:
        return ""
    return s.get("opcode", "") or ""


def _count_real_diff_op(instrs: list[dict]) -> tuple[int, list[str]]:
    real = 0
    samples: list[str] = []
    for ins in instrs:
        if ins.get("match_type") not in ("diff_op", "replace"):
            continue
        t = _normalize_opcode(ins.get("target"))
        b = _normalize_opcode(ins.get("base"))
        if not t or not b or t == b:
            continue
        pair = (t, b)
        if pair in _BRANCH_FLIPS or pair in _REAL_CMP_FLIPS:
            real += 1
            if len(samples) < 4:
                samples.append(f"[{ins.get('index', '?')}] {t}↔{b}")
    return real, samples


def _count_real_insert_delete(instrs: list[dict]) -> tuple[int, list[str]]:
    """An insert/delete pair where opcodes differ is structural.

    A pair where opcodes are equal (e.g. matching nop padding) can be
    cosmetic — but in practice nearly all insert/delete pairs are real.
    Returns count of insert/delete instructions plus a few samples.
    """
    real = 0
    samples: list[str] = []
    for ins in instrs:
        mt = ins.get("match_type")
        if mt not in ("insert", "delete"):
            continue
        real += 1
        if len(samples) < 4:
            side = ins.get("target") or ins.get("base") or {}
            samples.append(f"[{ins.get('index', '?')}] {mt} {_normalize_opcode(side)}")
    return real, samples


def classify(data: dict) -> Classification:
    """Classify an objdiff JSON output payload."""
    fuzzy = data.get("fuzzy_match_percent", 0.0) or 0.0
    raw = data.get("raw_match_percent", 0.0) or 0.0
    verdict = (data.get("verdict") or {}).get("classification", "")
    instrs = data.get("instructions", []) or []

    reg_swaps, offset_diffs, symbol_diffs, branch_diffs = parse_breakdowns(instrs)
    reloc_noise, real_replace, _ = categorize_replaces(instrs)
    real_op_diff, op_samples = _count_real_diff_op(instrs)
    insert_delete, id_samples = _count_real_insert_delete(instrs)

    metrics = {
        "fuzzy_pct_x100": int(fuzzy * 100),
        "raw_pct_x100": int(raw * 100),
        "reg_swaps": len(reg_swaps),
        "offset_diffs": len(offset_diffs),
        "symbol_diffs": len(symbol_diffs),
        "branch_diffs": len(branch_diffs),
        "reloc_noise_replaces": reloc_noise,
        "real_replaces": real_replace,
        "real_op_diffs": real_op_diff,
        "insert_delete": insert_delete,
    }

    # Hard COSMETIC cases.
    if verdict in ("MatchingFunctions", "100% normalized") and not instrs:
        return Classification(
            "COSMETIC",
            "verdict matches and no instruction diff",
            metrics,
        )

    if fuzzy >= 100.0 and raw >= 100.0:
        return Classification(
            "COSMETIC",
            "100% normalized + raw match",
            metrics,
        )

    # Hard REAL_DIFF: opcode-level semantic changes.
    if real_op_diff > 0:
        return Classification(
            "REAL_DIFF",
            f"{real_op_diff} branch/cmp opcode flip(s): {', '.join(op_samples)}",
            metrics,
        )

    if insert_delete > 0:
        return Classification(
            "REAL_DIFF",
            f"{insert_delete} insert/delete instruction(s): {', '.join(id_samples)}",
            metrics,
        )

    if real_replace > 0:
        return Classification(
            "REAL_DIFF",
            f"{real_replace} non-relocation replace(s)",
            metrics,
        )

    # COSMETIC when remaining diffs are purely register/offset/symbol noise
    # AND the function is close to a perfect match.
    cosmetic_only = (
        reg_swaps or offset_diffs or symbol_diffs or branch_diffs or reloc_noise
    )
    if cosmetic_only and fuzzy >= 95.0 and verdict in (
        "AtLimit", "MaybeFixable", "LikelyFixable", "100% normalized",
    ):
        bits = []
        if reg_swaps:
            bits.append(f"{len(reg_swaps)} reg-swap")
        if offset_diffs:
            bits.append(f"{len(offset_diffs)} offset-shift")
        if symbol_diffs:
            bits.append(f"{len(symbol_diffs)} reloc-symbol")
        if reloc_noise:
            bits.append(f"{reloc_noise} reloc-noise replace")
        return Classification(
            "COSMETIC",
            f"verdict={verdict}, cosmetic-only diffs: {', '.join(bits)}",
            metrics,
        )

    # Everything else: let a subagent look.
    return Classification(
        "BORDERLINE",
        f"verdict={verdict}, fuzzy={fuzzy:.1f}%",
        metrics,
    )


def classify_cli() -> int:
    import argparse
    import json

    ap = argparse.ArgumentParser(description="Classify an objdiff JSON payload.")
    ap.add_argument("path", help="Path to objdiff JSON output (or '-' for stdin)")
    args = ap.parse_args()

    if args.path == "-":
        data = json.load(sys.stdin)
    else:
        with open(args.path) as f:
            data = json.load(f)
    r = classify(data)
    print(f"{r.label}: {r.rationale}")
    for k, v in r.metrics.items():
        print(f"  {k}: {v}")
    return 0


if __name__ == "__main__":
    raise SystemExit(classify_cli())
