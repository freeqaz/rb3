#!/usr/bin/env python3
"""Confirm or refute struct-layout-audit candidates against the Bank-8 target.

`struct_layout_audit.py` flags candidate struct-layout bugs by diffing our header
`// 0xHEX` member offsets against DWARF. But the ONLY DWARF available is the
Bank-5 *proto* debug ELF, and the Bank-5 proto layouts genuinely DIVERGE from the
Bank-8 target the project actually matches against (the canonical proof is
`BandUser`: 100% matched yet showing a uniform +4 shift vs the Bank-5 DWARF). So
`struct_layout_audit.py` is a candidate GENERATOR that cannot, on its own, prove a
Bank-8 bug — its DWARF reference is the wrong bank.

This tool is the Bank-8 CONFIRMATION layer. It promotes/refutes those candidates
using objdiff per-function INSTRUCTION diffs, which ARE Bank-8-accurate because
they diff our compiled `.o` against the real Bank-8 target objects in
`build/SZBE69_B8/obj/*.o`.

How the confirmation works
--------------------------
A real Bank-8 struct-layout bug manifests as a CONSISTENT memory-displacement
delta on a non-frame, non-SDA base register, recurring across the functions that
touch that struct, in the direction the candidate predicts (our build uses one
offset, the target uses `ours + (target_dwarf - ours_dwarf)`).

  1. Take each candidate class from struct_layout_audit's "internal" bucket
     (non-uniform offset shift in a sub-`--max-pct` TU). A uniform whole-class
     shift is the BandUser signature of Bank-5 base divergence and is excluded by
     the generator already; we re-derive the same bucket here so this tool is
     self-contained from the generator's full --json dump.
  2. Map the candidate's defining header TU -> its report.json unit, gather the
     sub-100% functions there.
  3. Diff each function read-only (NO --build; safe alongside the permuter fleet)
     and extract `member`-type displacement deltas: a load/store where target and
     base share an opcode and a non-frame/non-SDA base register but differ only in
     the displacement immediate. The signed delta is `target_disp - base_disp`.
     (This reuses audit_normalized_masking.py's LOADSTORE set, base_reg_after_imm,
     and FRAME_REGS/SDA_REGS filtering — the same "member" signal.)
  4. Verdict:
       CONFIRMED    — a candidate-predicted delta recurs across the TU's
                      functions on real (non-frame/non-SDA) base registers. Our
                      Bank-8 layout is genuinely wrong; fixing the header pays off.
       REFUTED      — the TU has diffable sub-100% functions but NO member-offset
                      delta matches any predicted delta. The DWARF delta was
                      Bank-5 proto divergence; our Bank-8 layout is already correct.
       INCONCLUSIVE — the TU has no diffable sub-100% functions (every function is
                      100%, or none could be diffed), so the instruction stream
                      offers no evidence either way.

Read-only: diffs already-built .o files (NO --build), so it is safe to run
alongside the permuter fleet and never touches the ninja lock.

Default scope is the native-port surface (same OUT_OF_SCOPE filter as the
generator); pass --all to include sdk/network/rndwii/os/lib candidates too.

Examples:
    scripts/analysis/struct_confirm.py                         # confirm port-surface candidates
    scripts/analysis/struct_confirm.py --candidates out.json   # use an existing generator dump
    scripts/analysis/struct_confirm.py --filter CharEyes       # one candidate class
    scripts/analysis/struct_confirm.py --json verdicts.json
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OBJDIFF = REPO / "bin" / "objdiff-cli"
REPORT = REPO / "build" / "SZBE69_B8" / "report.json"
TMPDIR = "/tmp/struct_confirm"

# Same scope filter as struct_layout_audit (native-port surface by default).
OUT_OF_SCOPE_PREFIXES = (
    "system/rndwii/",
    "system/os/",
    "sdk/",
    "network/",
    "lib/",
)

# --- displacement-extraction primitives, reused from audit_normalized_masking ---
# PPC D-form memory ops: [dataReg, dispImm, baseReg]
LOADSTORE = {
    "lwz", "lwzu", "lbz", "lbzu", "lhz", "lhzu", "lha", "lhau", "lwa",
    "lfs", "lfsu", "lfd", "lfdu", "lmw", "psq_l", "psq_lu",
    "stw", "stwu", "stb", "stbu", "sth", "sthu",
    "stfs", "stfsu", "stfd", "stfdu", "stmw", "psq_st", "psq_stu",
}
FRAME_REGS = {"r1", "sp"}
SDA_REGS = {"r2", "r13", "rtoc"}


def base_reg_after_imm(typed_args):
    """For a D-form op, the base register is the Register that follows the immediate."""
    seen_imm = False
    for a in typed_args:
        t = a.get("type")
        if t in ("Signed", "Unsigned"):
            seen_imm = True
        elif t == "Register" and seen_imm:
            return a.get("value")
    return None


def disp_imm(typed_args):
    """The displacement immediate of a D-form op (the first Signed/Unsigned arg)."""
    for a in typed_args:
        if a.get("type") in ("Signed", "Unsigned"):
            return int(a.get("value"))
    return None


# --- candidate selection (re-derive struct_layout_audit's "internal" bucket) -----


@dataclass
class Candidate:
    cls: str
    unit: str               # header path, e.g. "system/char/CharEyes.h"
    owner_pct: float
    deltas: list            # predicted member shifts (target_dwarf - ours_dwarf)
    members: list = field(default_factory=list)  # [(name, ours, target)]


def in_scope(unit: str, include_all: bool) -> bool:
    if include_all:
        return True
    return not any(unit.startswith(p) for p in OUT_OF_SCOPE_PREFIXES)


def load_candidates(path: str, max_pct: float, include_all: bool,
                    filt: str | None) -> list[Candidate]:
    """Read the generator's full --json findings dump and reproduce its
    "internal" (non-uniform shift in sub-max_pct TU) candidate bucket.

    We classify here rather than parse the generator's terminal output so this
    tool stays robust to formatting changes and works from a plain JSON dump.
    """
    findings = json.loads(Path(path).read_text())
    by_class: dict[tuple, list[dict]] = defaultdict(list)
    for f in findings:
        by_class[(f["cls"], f["unit"], f["owner_pct"])].append(f)

    cands: list[Candidate] = []
    for (cls, unit, pct), items in by_class.items():
        if pct is None or pct >= max_pct:
            continue
        if not in_scope(unit, include_all):
            continue
        if filt and filt not in cls:
            continue
        offset_items = [f for f in items if f["kind"] == "OFFSET_MISMATCH"]
        deltas = {f["target"] - f["ours"] for f in offset_items}
        # Uniform whole-class shift = base/proto divergence (BandUser signature):
        # the generator already excludes it, and it carries no per-member signal.
        if len(deltas) == 1 and all(f["kind"] == "OFFSET_MISMATCH" for f in items):
            continue
        if not offset_items:
            # size-only candidate: no per-member offset prediction to confirm at
            # the instruction level (a byte_size delta is not a load displacement).
            continue
        members = [(f["member"], f["ours"], f["target"]) for f in offset_items]
        cands.append(Candidate(cls, unit, float(pct), sorted(deltas), members))
    return cands


def generate_candidates(max_pct: float, include_all: bool, filt: str | None,
                        unit: str | None) -> str:
    """Invoke struct_layout_audit.py to produce a fresh full findings dump."""
    out = os.path.join(TMPDIR, "candidates.json")
    cmd = [sys.executable, str(REPO / "scripts" / "analysis" / "struct_layout_audit.py"),
           "--json", out, "--max-pct", str(max_pct)]
    if include_all:
        cmd.append("--all")
    if filt:
        cmd += ["--filter", filt]
    if unit:
        cmd += ["--unit", unit]
    print("[gen] running struct_layout_audit.py to refresh candidates...",
          file=sys.stderr)
    r = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr, file=sys.stderr)
        sys.exit(2)
    return out


# --- report.json TU lookup -------------------------------------------------------


def strip_unit_prefix(name: str) -> str:
    for prefix in ("main/", "default/"):
        if name.startswith(prefix):
            return name[len(prefix):]
    return name


def load_units() -> dict[str, dict]:
    """Map stripped-unit-name (e.g. 'system/char/CharEyes') -> report unit dict."""
    data = json.loads(REPORT.read_text())
    out: dict[str, dict] = {}
    for u in data.get("units", []):
        out[strip_unit_prefix(u.get("name", ""))] = u
    return out


def tu_functions(unit_rec: dict) -> list[tuple]:
    """Return [(symbol, fuzzy_pct, size)] for sub-100% functions, lowest first."""
    fns = []
    for f in (unit_rec.get("functions") or []):
        pct = f.get("fuzzy_match_percent")
        if pct is None or pct >= 100.0:
            continue
        fns.append((f["name"], float(pct), int(f.get("size", 0))))
    fns.sort(key=lambda t: t[1])
    return fns


# --- per-function member-displacement extraction ---------------------------------


def member_deltas_for_function(symbol: str, unit_name: str):
    """Run a read-only objdiff and extract member-offset deltas.

    Returns (deltas, error) where deltas is a list of dicts:
        {delta, base_reg, opcode, target_disp, base_disp}
    A member delta is a load/store where target & base share opcode and a
    non-frame/non-SDA base register but differ only in the displacement immediate.
    `delta = target_disp - base_disp` (i.e. how much our build would have to move
    the member to reach the target's offset).
    """
    os.makedirs(TMPDIR, exist_ok=True)
    fd, path = tempfile.mkstemp(suffix=".json", prefix="sc_", dir=TMPDIR)
    os.close(fd)
    try:
        cmd = [str(OBJDIFF), "diff", "-p", str(REPO), symbol, "-u", unit_name,
               "--include-instructions", "-f", "json", "-o", path]
        r = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, timeout=120)
        if r.returncode != 0:
            return [], (r.stderr or "")[-200:]
        d = json.loads(Path(path).read_text())
    except Exception as e:  # noqa: BLE001
        return [], str(e)[:200]
    finally:
        try:
            os.unlink(path)
        except OSError:
            pass

    deltas = []
    for ins in d.get("instructions", []):
        if ins.get("match_type") != "diff_arg":
            continue
        tgt = ins.get("target") or {}
        base = ins.get("base") or {}
        op = tgt.get("opcode")
        if op != base.get("opcode") or op not in LOADSTORE:
            continue
        tt = tgt.get("typed_args") or []
        bt = base.get("typed_args") or []
        tb = base_reg_after_imm(tt)
        bb = base_reg_after_imm(bt)
        # The base register must be the same on both sides — a differing base
        # register is a register-allocation difference, not a member-offset one.
        if tb is None or tb != bb:
            continue
        if tb in FRAME_REGS or tb in SDA_REGS:
            continue  # stack-frame / small-data displacement — allowed to differ
        ti = disp_imm(tt)
        bi = disp_imm(bt)
        if ti is None or bi is None or ti == bi:
            continue
        # If the displacement carries a relocation (Symbol arg), it's a pooled
        # data/vtable offset, not a raw struct member — skip it.
        if any(a.get("type") == "Symbol" for a in tt) or \
           any(a.get("type") == "Symbol" for a in bt):
            continue
        deltas.append({
            "delta": ti - bi,
            "base_reg": tb,
            "opcode": op,
            "target_disp": ti,
            "base_disp": bi,
        })
    return deltas, None


# --- verdict --------------------------------------------------------------------


@dataclass
class Verdict:
    cls: str
    unit: str
    owner_pct: float
    predicted: list
    verdict: str                  # CONFIRMED | REFUTED | INCONCLUSIVE
    fns_total: int
    fns_diffed: int
    member_diff_total: int        # raw member-offset diffs observed (any delta)
    matched_delta_hits: dict      # predicted_delta -> count of OFFSET-ANCHORED hits
    matched_fns: int              # distinct functions with an anchored hit
    evidence: list                # anchored evidence rows (member at our->target offset)
    errors: int = 0
    anchored_members: dict = field(default_factory=dict)  # member name -> hit count


def confirm_candidate(cand: Candidate, units: dict, workers: int,
                      min_hits: int) -> Verdict:
    unit_rec = units.get(cand.unit[:-2] if cand.unit.endswith(".h") else cand.unit)
    predicted = set(cand.deltas)
    if unit_rec is None:
        return Verdict(cand.cls, cand.unit, cand.owner_pct, sorted(predicted),
                       "INCONCLUSIVE", 0, 0, 0, {}, 0, [],
                       )

    unit_name = unit_rec.get("name")
    fns = tu_functions(unit_rec)
    if not fns:
        return Verdict(cand.cls, cand.unit, cand.owner_pct, sorted(predicted),
                       "INCONCLUSIVE", 0, 0, 0, {}, 0, [])

    # Diff every sub-100% function in the TU and collect member deltas.
    all_deltas = []           # flat list of delta dicts (annotated with fn)
    diffed = 0
    errors = 0
    with ThreadPoolExecutor(max_workers=workers) as ex:
        futs = {ex.submit(member_deltas_for_function, sym, unit_name): (sym, pct)
                for sym, pct, _ in fns}
        for fut in as_completed(futs):
            sym, pct = futs[fut]
            ds, err = fut.result()
            if err:
                errors += 1
                continue
            diffed += 1
            for d in ds:
                d2 = dict(d)
                d2["fn"] = sym
                d2["fn_pct"] = pct
                all_deltas.append(d2)

    if diffed == 0:
        return Verdict(cand.cls, cand.unit, cand.owner_pct, sorted(predicted),
                       "INCONCLUSIVE", len(fns), 0, 0, {}, 0, [], errors)

    # ── OFFSET-ANCHORED matching ──────────────────────────────────────────────
    # Matching on the delta VALUE alone is far too loose: +4 / +8 / +12 are the
    # most common deltas in ordinary register-allocation and instruction-
    # scheduling noise (adjacent fields, vertex-array strides, sub-object math,
    # stack-buffer offsets), so a coincidental delta-value hit at an UNRELATED
    # offset will fire on almost every TU. The genuine layout-bug signature is
    # the SAME member accessed at OUR predicted-wrong offset on our side and at
    # the target's predicted offset on the target side:
    #
    #     base_disp == member.ours   AND   target_disp == member.target
    #
    # i.e. the instruction stream shows our build reading the field where the
    # header annotates it and the target reading it 'delta' bytes away — exactly
    # what struct_layout_audit predicted. We anchor every observed member-offset
    # diff to a concrete predicted member; only anchored hits count toward a
    # CONFIRM. (Bare delta-value hits are still tallied for context/diagnostics.)
    member_by_pair = {(m_ours, m_tgt): name
                      for (name, m_ours, m_tgt) in cand.members}

    matched_delta_hits: dict = {}        # predicted delta -> # of anchored hits
    anchored_members: dict = {}          # member name -> # of anchored hits
    matched_fns_set = set()
    anchored_evidence = []
    for d in all_deltas:
        name = member_by_pair.get((d["base_disp"], d["target_disp"]))
        if name is None:
            continue
        # This instruction reads exactly member `name` at our offset on our side
        # and at the target's offset on the target side: an anchored confirmation.
        matched_delta_hits[d["delta"]] = matched_delta_hits.get(d["delta"], 0) + 1
        anchored_members[name] = anchored_members.get(name, 0) + 1
        matched_fns_set.add(d["fn"])
        ev = dict(d)
        ev["member"] = name
        anchored_evidence.append(ev)

    # CONFIRMED requires an offset-anchored hit (a real member at our exact
    # predicted-wrong offset vs the target's offset) that RECURS — either the
    # same member across `min_hits` accesses, or two distinct members both
    # landing on their predicted offsets. A single lone anchored hit could still
    # be a coincidence, so we require recurrence (count >= min_hits) OR breadth
    # (>= 2 distinct anchored members confirming the same shift).
    total_anchored = sum(anchored_members.values())
    distinct_members = len(anchored_members)
    confirmed = total_anchored >= min_hits or distinct_members >= 2

    verdict = "CONFIRMED" if confirmed else "REFUTED"
    # Strongest evidence (most-recurring anchored delta) first.
    anchored_evidence.sort(key=lambda d: -matched_delta_hits.get(d["delta"], 0))
    return Verdict(
        cand.cls, cand.unit, cand.owner_pct, sorted(predicted), verdict,
        len(fns), diffed, len(all_deltas), matched_delta_hits,
        len(matched_fns_set), anchored_evidence[:8], errors,
        dict(anchored_members),
    )


# --- main -----------------------------------------------------------------------


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--candidates", metavar="FILE",
                    help="use an existing struct_layout_audit --json dump instead "
                         "of regenerating one")
    ap.add_argument("--all", action="store_true",
                    help="include sdk/network/rndwii/os/lib candidates")
    ap.add_argument("--filter", help="substring match on candidate class name")
    ap.add_argument("--unit", help="restrict generation to a src/ subtree "
                                   "(only used when regenerating candidates)")
    ap.add_argument("--max-pct", type=float, default=99.0,
                    help="defining-TU fuzzy%% ceiling for a candidate to be "
                         "actionable (default 99.0; mirrors struct_layout_audit)")
    ap.add_argument("--min-hits", type=int, default=2,
                    help="how many times a predicted delta must recur in the TU's "
                         "instruction diffs to CONFIRM (default 2)")
    ap.add_argument("--workers", type=int, default=6,
                    help="parallel objdiff workers (default 6)")
    ap.add_argument("--json", metavar="FILE", help="write all verdicts to FILE")
    args = ap.parse_args()

    os.makedirs(TMPDIR, exist_ok=True)
    if not OBJDIFF.exists():
        print(f"missing objdiff-cli: {OBJDIFF}", file=sys.stderr)
        return 2
    if not REPORT.exists():
        print(f"missing report.json: {REPORT}", file=sys.stderr)
        return 2

    cand_path = args.candidates or generate_candidates(
        args.max_pct, args.all, args.filter, args.unit)
    candidates = load_candidates(cand_path, args.max_pct, args.all, args.filter)
    if not candidates:
        print("no actionable (non-uniform, sub-max-pct, in-scope) candidates found.",
              file=sys.stderr)
        return 0

    units = load_units()
    print(f"confirming {len(candidates)} candidate class(es) against Bank-8 "
          f"instruction diffs ({args.workers} workers, read-only)...",
          file=sys.stderr)

    verdicts: list[Verdict] = []
    for i, cand in enumerate(sorted(candidates, key=lambda c: c.owner_pct), 1):
        print(f"  [{i}/{len(candidates)}] {cand.cls} ({cand.unit}, "
              f"{cand.owner_pct:.2f}%)...", file=sys.stderr)
        verdicts.append(confirm_candidate(cand, units, args.workers, args.min_hits))

    # ── report ──────────────────────────────────────────────────────────────
    order = {"CONFIRMED": 0, "REFUTED": 1, "INCONCLUSIVE": 2}
    verdicts.sort(key=lambda v: (order[v.verdict], v.owner_pct))
    counts = Counter(v.verdict for v in verdicts)

    print("\n" + "=" * 74)
    print("STRUCT-LAYOUT CANDIDATE CONFIRMATION (Bank-8 instruction-diff layer)")
    print("=" * 74)
    print("Bank-5 DWARF generates candidates; the Bank-8 target .o files confirm "
          "them.\nA real layout bug = an OFFSET-ANCHORED diff: the instruction "
          "stream shows a\npredicted member read at OUR offset on our side and at "
          "the TARGET offset on the\ntarget side (base_disp==ours & "
          "target_disp==target), recurring across the TU.\n")
    print(f"candidates    : {len(verdicts)}")
    print(f"  CONFIRMED   : {counts.get('CONFIRMED', 0)}  (real Bank-8 layout bug — fix the header)")
    print(f"  REFUTED     : {counts.get('REFUTED', 0)}  (Bank-5 proto divergence — header is correct)")
    print(f"  INCONCLUSIVE: {counts.get('INCONCLUSIVE', 0)}  (no diffable sub-100% functions)")

    def render(v: Verdict) -> None:
        print(f"\n[{v.verdict:12s}] {v.cls}  TU={v.unit} ({v.owner_pct:.2f}%)")
        print(f"    predicted deltas : {v.predicted}")
        print(f"    fns sub-100/diffed: {v.fns_total}/{v.fns_diffed}"
              f"   member-offset diffs: {v.member_diff_total}"
              f"   anchored hits: {sum(v.anchored_members.values())}"
              f" in {v.matched_fns} fn(s)")
        if v.anchored_members:
            mem = ", ".join(f"{n}×{c}" for n, c in
                            sorted(v.anchored_members.items(), key=lambda x: -x[1]))
            print(f"    anchored members : {mem}")
        for e in v.evidence[:5]:
            print(f"      {e['opcode']:5s} {e.get('member','?'):<14s}"
                  f" base 0x{e['base_disp']:x},{e['base_reg']:>3s}"
                  f" -> tgt 0x{e['target_disp']:x}  Δ={e['delta']:+d}"
                  f"   {e['fn'][:34]}")

    confirmed = [v for v in verdicts if v.verdict == "CONFIRMED"]
    if confirmed:
        print("\n" + "#" * 74)
        print("## CONFIRMED — real Bank-8 layout bugs (worth fixing the header)")
        print("#" * 74)
        for v in confirmed:
            render(v)

    print("\n" + "-" * 74)
    print("REFUTED — DWARF delta was Bank-5 proto noise; Bank-8 layout is correct")
    print("-" * 74)
    for v in verdicts:
        if v.verdict == "REFUTED":
            render(v)

    print("\n" + "-" * 74)
    print("INCONCLUSIVE — TU has no diffable sub-100% functions")
    print("-" * 74)
    for v in verdicts:
        if v.verdict == "INCONCLUSIVE":
            print(f"  {v.cls:30s} TU={v.unit} ({v.owner_pct:.2f}%)  "
                  f"sub-100 fns={v.fns_total} diffed={v.fns_diffed}")

    if args.json:
        Path(args.json).write_text(json.dumps([v.__dict__ for v in verdicts], indent=2))
        print(f"\nwrote {args.json} ({len(verdicts)} verdicts)", file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main())
