#!/usr/bin/env python3
"""Grow the Wii<->Xenon eval holdout from round-2 human-judged identities.

Round-1 left a 146-entry holdout (Xenon addr + TU stem only) at
build/SZBE69_B8/ghidra/xenon-seeds/holdout.json. Round 2 hand-judged a
stratified 30-pair band3 ACCEPT sample (docs/decomp/xenon-hardening/round2/
forensics/structured_pairs.json + the injected judge verdicts). The
judged-CORRECT pairs are human-grade ground truth; the judged-WRONG pairs are
the first known-negatives.

This script is the SINGLE source of truth for the round-2 holdout growth. It is
fully deterministic and idempotent (re-running produces byte-identical output —
no duplicate entries, no drift):

  (1) SPLIT (XOR rule — a pair is holdout XOR future-seed, never both):
      take all judged-CORRECT pairs, sort by xenon_addr, Random(42).shuffle,
      first ceil(n/2) -> holdout, the rest -> reserved-for-seeds.
  (2) Append the holdout half to xenon-seeds/holdout.json, preserving every
      original entry byte-equivalently. New entries carry, in addition to the
      legacy {addr, stem}:
        wii_addr_bank8 — the Bank-8 Wii virtual address (so eval can score the
                         recovery by EXACT p1 agreement, stronger than the TU
                         stem heuristic),
        wii_symbol     — the CW-mangled Wii symbol,
        source         — 'judged-round2-correct'.
  (3) Write the reserved half to xenon-seeds/reserved_seed_candidates_round2.json
      (NOT ingested into any seeds file this round; future-seed pool) and the
      judged-WRONG pairs to xenon-seeds/known_negatives.json.

Anti-leak invariants asserted in-script (and surfaced to the caller):
  - holdout-new  ∩ reserved = ∅  (the XOR split)
  - new-holdout addrs ∩ original-146 addrs = ∅ (the sample is band3 ACCEPT
    non-seed; the original holdout is the crossval set — disjoint by construction,
    asserted)
  - reserved ∩ holdout = ∅

Pair metadata (TU, category, Bank-8 wii addr, match_types) comes from the
vetted_identities.json ACCEPT tier joined by Xenon address; the verdicts come
from forensics/judge_verdicts.json (written here from the injected judge output
the first time, then reused). Addresses are joined bare-hex (strip 0x) and
emitted as '0x'+UPPERCASE to match the legacy holdout.json addr format.

Usage:
    python3 tools/ghidra/grow_xenon_holdout.py            # write the real files
    python3 tools/ghidra/grow_xenon_holdout.py --dry-run  # print, write nothing
    python3 tools/ghidra/grow_xenon_holdout.py --out-dir /tmp/claude/seeds-test
"""
from __future__ import annotations

import argparse
import json
import math
import random
import sys
from pathlib import Path
from typing import Dict, List, Optional

RB3 = Path(__file__).resolve().parents[2]
ROUND2 = RB3 / "docs" / "decomp" / "xenon-hardening" / "round2"
FORENSICS = ROUND2 / "forensics"

DEFAULT_STRUCTURED = FORENSICS / "structured_pairs.json"
DEFAULT_VERDICTS = FORENSICS / "judge_verdicts.json"
DEFAULT_VETTED = (RB3 / "build" / "SZBE69_B8" / "ghidra" / "ghidriff-xenon"
                  / "vetted_identities.json")
DEFAULT_OUT_DIR = RB3 / "build" / "SZBE69_B8" / "ghidra" / "xenon-seeds"

SPLIT_SEED = 42  # the XOR-split RNG seed (locked; do not change)

# The 30-pair judge verdicts injected by the round-2 runner. Persisted to
# forensics/judge_verdicts.json on first run; the file (if present) wins so the
# verdicts are auditable and re-derivable without the runner. Keyed by pair_id.
INJECTED_VERDICTS: Dict[str, str] = {
    "01": "correct", "02": "correct", "03": "correct", "04": "correct",
    "05": "correct", "06": "correct", "07": "correct", "08": "correct",
    "09": "correct", "10": "correct", "11": "correct", "12": "correct",
    "13": "wrong",   "14": "correct", "15": "correct", "16": "wrong",
    "17": "correct", "18": "correct", "19": "correct", "20": "correct",
    "21": "correct", "22": "correct", "23": "correct", "24": "correct",
    "25": "correct", "26": "correct", "27": "correct", "28": "correct",
    "29": "wrong",   "30": "correct",
}


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------
def norm_hex(a: str) -> str:
    """'0x8252a9f0' / '8252a9f0' / '0x8252A9F0' -> '0x8252A9F0' (legacy fmt)."""
    s = a.strip()
    if s.lower().startswith("0x"):
        s = s[2:]
    return "0x" + s.upper()


def bare_hex(a: str) -> str:
    """Lowercase bare hex for join keys."""
    s = a.strip()
    if s.lower().startswith("0x"):
        s = s[2:]
    return s.lower()


def tu_stem(tu: Optional[str]) -> Optional[str]:
    if not tu:
        return None
    return tu[:-2] if tu.endswith(".o") else tu


def load_verdicts(path: Path) -> Dict[str, str]:
    """Round-2 judge verdicts. On-disk file wins; otherwise persist the
    injected map so future runs are reproducible without the runner."""
    if path.is_file():
        data = json.loads(path.read_text())
        # accept either {pair_id: verdict} or {"per_pair":[{pair_id,verdict}]}
        if isinstance(data, dict) and "per_pair" in data:
            return {r["pair_id"]: r["verdict"] for r in data["per_pair"]}
        return {str(k): str(v) for k, v in data.items()}
    return dict(INJECTED_VERDICTS)


def vetted_index(vetted_path: Path) -> Dict[str, dict]:
    """Xenon bare-hex addr -> vetted entry (tu/category/wii_addr/match_types)."""
    vet = json.loads(vetted_path.read_text())
    entries = vet["entries"] if isinstance(vet, dict) else vet
    return {bare_hex(e["xenon_addr"]): e for e in entries}


# ---------------------------------------------------------------------------
# core
# ---------------------------------------------------------------------------
def build_rows(structured: List[dict], verdicts: Dict[str, str],
               vidx: Dict[str, dict],
               orig_holdout_addrs: Optional[set] = None) -> dict:
    """Returns {'holdout':[...], 'reserved':[...], 'known_negatives':[...],
    'already_in_holdout':[...]}.

    Deterministic: judged-correct sorted by numeric xenon_addr, Random(42)
    shuffle, first ceil(n/2) -> holdout. Each row is enriched from the vetted
    ACCEPT index (TU stem + Bank-8 wii addr + wii symbol + match_types).

    ANTI-LEAK: a judged-CORRECT pair whose Xenon addr is ALREADY in the round-1
    crossval holdout is dropped from the split entirely — it is already holdout,
    so it can be neither re-added (double-count) nor reserved-for-seeds (it is
    not a future-seed candidate). Such pairs are reported in 'already_in_holdout'.
    Known-negatives are unaffected by this filter (a wrong (p1,p2) pairing is a
    negative regardless of whether that p2 is a holdout entry, whose p1 differs)."""
    orig_holdout_addrs = orig_holdout_addrs or set()
    correct, wrong, already = [], [], []
    for p in structured:
        v = verdicts.get(p["pair_id"])
        if v == "correct":
            if norm_hex(p["xenon_addr"]) in orig_holdout_addrs:
                already.append(p)
            else:
                correct.append(p)
        elif v == "wrong":
            wrong.append(p)
        # 'uncertain'/missing: neither holdout nor known-negative (conservative)

    def enrich(p: dict) -> dict:
        xa = bare_hex(p["xenon_addr"])
        ve = vidx.get(xa, {})
        tu = ve.get("tu")
        return {
            "pair_id": p["pair_id"],
            "xenon_addr": norm_hex(p["xenon_addr"]),
            "wii_addr_bank8": norm_hex(p["wii_addr"]).lower(),  # 0x + lower (Bank8 vaddr)
            "wii_symbol": p["wii_symbol"],
            "tu": tu,
            "stem": tu_stem(tu),
            "category": ve.get("category"),
            "match_types": ve.get("match_types") or [p.get("match_type")],
        }

    correct_sorted = sorted(correct, key=lambda p: int(p["xenon_addr"], 16))
    shuffled = correct_sorted[:]
    random.Random(SPLIT_SEED).shuffle(shuffled)
    half = math.ceil(len(shuffled) / 2)
    holdout = [enrich(p) for p in shuffled[:half]]
    reserved = [enrich(p) for p in shuffled[half:]]
    known_neg = [enrich(p) for p in wrong]

    # anti-leak invariants
    h_addrs = {r["xenon_addr"] for r in holdout}
    r_addrs = {r["xenon_addr"] for r in reserved}
    assert h_addrs.isdisjoint(r_addrs), "holdout ∩ reserved != ∅ (XOR violated)"
    assert len(h_addrs) == len(holdout), "duplicate xenon addr in holdout half"
    assert len(r_addrs) == len(reserved), "duplicate xenon addr in reserved half"
    return {"holdout": holdout, "reserved": reserved,
            "known_negatives": known_neg,
            "already_in_holdout": [enrich(p) for p in
                                   sorted(already, key=lambda p: int(p["xenon_addr"], 16))]}


def to_holdout_entry(row: dict) -> dict:
    """A grown holdout.json entry (legacy {addr,stem} + round-2 fields)."""
    return {
        "addr": row["xenon_addr"],
        "stem": row["stem"],
        "wii_addr_bank8": row["wii_addr_bank8"],
        "wii_symbol": row["wii_symbol"],
        "source": "judged-round2-correct",
    }


def merge_holdout(existing: dict, new_rows: List[dict]) -> dict:
    """Union-merge by Xenon addr. Original entries preserved byte-equivalent;
    grown entries appended in deterministic (sorted-addr) order; idempotent
    (an addr already present as a round-2 entry is replaced in place, never
    duplicated). Returns the merged dict; does NOT mutate `existing`."""
    entries = list(existing.get("entries", []))
    by_addr = {norm_hex(e["addr"]): i for i, e in enumerate(entries)}
    for row in sorted(new_rows, key=lambda r: int(r["xenon_addr"], 16)):
        entry = to_holdout_entry(row)
        a = norm_hex(entry["addr"])
        if a in by_addr:
            existing_e = entries[by_addr[a]]
            # only overwrite OUR round-2 entries (never the crossval 146)
            if existing_e.get("source") == "judged-round2-correct":
                entries[by_addr[a]] = entry
            # else: addr collides with a crossval entry — leave the original,
            # the disjointness assert in build_rows/main guards against this.
        else:
            by_addr[a] = len(entries)
            entries.append(entry)

    note = existing.get("note", "")
    grown_note = (
        " | round 2 (2026-06-11): grown with human-judged band3 ACCEPT "
        "identities (source='judged-round2-correct'); those carry "
        "wii_addr_bank8 + wii_symbol so eval scores them by EXACT Wii "
        "address. The original crossval entries are unchanged."
    )
    if grown_note.strip() not in note:
        note = note + grown_note
    out = dict(existing)
    out["note"] = note
    out["entries"] = entries
    return out


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--structured", type=Path, default=DEFAULT_STRUCTURED)
    ap.add_argument("--verdicts", type=Path, default=DEFAULT_VERDICTS)
    ap.add_argument("--vetted", type=Path, default=DEFAULT_VETTED)
    ap.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    ap.add_argument("--dry-run", action="store_true",
                    help="print what would change, write nothing")
    args = ap.parse_args()

    structured = json.loads(args.structured.read_text())
    verdicts = load_verdicts(args.verdicts)
    vidx = vetted_index(args.vetted)

    out_dir = args.out_dir
    holdout_path = out_dir / "holdout.json"
    reserved_path = out_dir / "reserved_seed_candidates_round2.json"
    kn_path = out_dir / "known_negatives.json"

    existing = (json.loads(holdout_path.read_text())
                if holdout_path.is_file() else {"entries": []})
    orig_entries = existing.get("entries", [])
    # the crossval-146 (everything not already a round-2 entry) defines what is
    # "already holdout" — those judged-correct pairs are dropped from the split.
    orig_addrs = {norm_hex(e["addr"]) for e in orig_entries
                  if e.get("source") != "judged-round2-correct"}

    rows = build_rows(structured, verdicts, vidx, orig_holdout_addrs=orig_addrs)
    holdout_rows = rows["holdout"]
    reserved_rows = rows["reserved"]
    kn_rows = rows["known_negatives"]
    already_rows = rows["already_in_holdout"]

    new_addrs = {r["xenon_addr"] for r in holdout_rows}
    # defensive backstop: the split already drops crossval-overlapping pairs, so
    # this can never fire — but if it ever did it would mean a double-count.
    overlap = new_addrs & orig_addrs
    assert not overlap, f"new holdout collides with crossval addrs: {sorted(overlap)}"
    # reserved must also be disjoint from the crossval holdout (future-seed pool).
    res_overlap = {r["xenon_addr"] for r in reserved_rows} & orig_addrs
    assert not res_overlap, f"reserved collides with crossval addrs: {sorted(res_overlap)}"

    merged_holdout = merge_holdout(existing, holdout_rows)

    reserved_doc = {
        "note": "Round-2 RESERVED-FOR-SEEDS pool (judged-CORRECT, the half NOT "
                "promoted to holdout via the Random(42) XOR split). NOT ingested "
                "into any seeds file this round; a FUTURE seed build may consume "
                "these. Asserted disjoint from holdout.json.",
        "split_seed": SPLIT_SEED,
        "source": "judged-round2-correct",
        "entries": [
            {"xenon_addr": r["xenon_addr"], "wii_addr_bank8": r["wii_addr_bank8"],
             "wii_symbol": r["wii_symbol"], "tu": r["tu"], "stem": r["stem"],
             "category": r["category"], "match_types": r["match_types"]}
            for r in sorted(reserved_rows, key=lambda r: int(r["xenon_addr"], 16))
        ],
    }

    kn_doc = {
        "note": "Round-2 KNOWN-NEGATIVES: human-judged-WRONG (p1,p2) identity "
                "pairs. A match recurring this EXACT (xenon_addr -> wii_addr) "
                "pair is wrong; the same xenon_addr matched to a DIFFERENT Wii "
                "addr may still be the true identity and is NOT penalized.",
        "source": "judged-round2-wrong",
        "entries": [
            {"xenon_addr": r["xenon_addr"], "wii_addr_bank8": r["wii_addr_bank8"],
             "wii_symbol": r["wii_symbol"], "match_types": r["match_types"],
             "source": "judged-round2-wrong"}
            for r in sorted(kn_rows, key=lambda r: int(r["xenon_addr"], 16))
        ],
    }

    n_orig = len(orig_addrs)
    summary = {
        "judged_correct": len(holdout_rows) + len(reserved_rows) + len(already_rows),
        "judged_wrong": len(kn_rows),
        "split_seed": SPLIT_SEED,
        "holdout_new": len(holdout_rows),
        "reserved": len(reserved_rows),
        "known_negatives": len(kn_rows),
        "already_in_original_holdout": len(already_rows),
        "holdout_original": n_orig,
        "holdout_total_after": len(merged_holdout["entries"]),
        "holdout_new_pair_ids": sorted(r["pair_id"] for r in holdout_rows),
        "reserved_pair_ids": sorted(r["pair_id"] for r in reserved_rows),
        "known_negative_pair_ids": sorted(r["pair_id"] for r in kn_rows),
        "already_in_holdout_pair_ids": sorted(r["pair_id"] for r in already_rows),
    }
    print(json.dumps(summary, indent=1))

    if args.dry_run:
        print("[dry-run] no files written", file=sys.stderr)
        return 0

    out_dir.mkdir(parents=True, exist_ok=True)
    holdout_path.write_text(json.dumps(merged_holdout, indent=1) + "\n")
    reserved_path.write_text(json.dumps(reserved_doc, indent=1) + "\n")
    kn_path.write_text(json.dumps(kn_doc, indent=1) + "\n")
    print(f"[grow] holdout: {holdout_path} ({len(merged_holdout['entries'])} entries, "
          f"+{len(holdout_rows)} round-2)", file=sys.stderr)
    print(f"[grow] reserved: {reserved_path} ({len(reserved_rows)})", file=sys.stderr)
    print(f"[grow] known-negatives: {kn_path} ({len(kn_rows)})", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
