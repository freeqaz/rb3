#!/usr/bin/env python3
"""Compare stack-frame layouts between target and base compilations.

For a given function, walk the objdiff target+base instruction stream and
build per-offset "slot fingerprints" (opcode family, inferred size, access
count, instruction-index span). Diff the two fingerprint maps to identify:

  MATCH      -- same offset, same fingerprint
  SHIFTED    -- same fingerprint, offset differs by the dominant frame Δ
  SWAPPED    -- two slots' fingerprints exchanged (declaration reorder lever)
  DIFFER     -- same offset, different fingerprint (unresolved)
  TGT_ONLY   -- offset present only on target side
  BASE_ONLY  -- offset present only on our build

Also parses the prologue to report frame size and callee-saved register counts;
if the frame delta is fully explained by callee-saved counts, flags AT_LIMIT.

Usage:
    python3 scripts/analysis/stack_layout.py --symbol "Mangled__FName" [--unit U] [--show-equal]
"""

import argparse
import json
import os
import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from typing import Optional

# Reuse objdiff invocation from diff_inspect
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from diff_inspect import run_objdiff_for_symbol

# Lazy import: dwarf_locals depends on pyelftools and recompiles the file with
# debug info. If unavailable, we degrade to no-names mode without crashing.
def _try_extract_locals(symbol, project_dir):
    try:
        import dwarf_locals  # type: ignore
    except ImportError as exc:
        print(f"  [stack_layout] dwarf_locals unavailable: {exc}", file=sys.stderr)
        return {}
    try:
        return dwarf_locals.extract_locals(symbol, project_dir)
    except Exception as exc:
        print(f"  [stack_layout] name extraction failed: {exc}", file=sys.stderr)
        return {}


# ── Opcode classification ────────────────────────────────────────────────────

LOAD_OPCODES = {
    "lwz": ("int", 4), "lwzu": ("int", 4), "lwzx": ("int", 4),
    "lha": ("int", 2), "lhau": ("int", 2),
    "lhz": ("int", 2), "lhzu": ("int", 2),
    "lbz": ("int", 1), "lbzu": ("int", 1),
    "lmw": ("int", 4),
    "lfs": ("float", 4), "lfsu": ("float", 4),
    "lfd": ("float", 8), "lfdu": ("float", 8),
    "psq_l": ("paired", 8), "psq_lu": ("paired", 8),
}

STORE_OPCODES = {
    "stw": ("int", 4), "stwu": ("int", 4),
    "sth": ("int", 2), "sthu": ("int", 2),
    "stb": ("int", 1), "stbu": ("int", 1),
    "stmw": ("int", 4),
    "stfs": ("float", 4), "stfsu": ("float", 4),
    "stfd": ("float", 8), "stfdu": ("float", 8),
    "psq_st": ("paired", 8), "psq_stu": ("paired", 8),
}

ADDR_OPCODES = {"addi", "addic", "addic."}

LS_OPCODES = {**LOAD_OPCODES, **STORE_OPCODES}


def parse_stack_ref(opcode: str, args: str) -> Optional[tuple[int, str, int]]:
    """Return (offset, kind, size) if this instruction references N(r1), else None.

    objdiff arg format examples:
      stw   r0, 0x374, r1          -> (0x374, 'int',   4)
      stfd  f31, 0x360, r1         -> (0x360, 'float', 8)
      psq_st f31, 0x368, r1, 0, qr0 -> (0x368, 'paired', 8)
      addi  r11, r1, 0x270         -> (0x270, 'addr',  0)
    """
    parts = [p.strip() for p in args.split(",")]
    if len(parts) < 3:
        return None

    if opcode in LS_OPCODES:
        # rD, offset, base [, ...]
        try:
            offset = int(parts[1], 0)
        except ValueError:
            return None
        base = parts[2]
        kind, size = LS_OPCODES[opcode]
    elif opcode in ADDR_OPCODES:
        # rD, base, imm — but skip frame-arithmetic targets:
        #   • addi r1, r1, N   (epilogue: deallocate frame)
        #   • addi r11, r1, N  (prologue: helper save-area base)
        dest = parts[0]
        if dest in ("r1", "r11"):
            return None
        base = parts[1]
        try:
            offset = int(parts[2], 0)
        except ValueError:
            return None
        kind, size = "addr", 0
    else:
        return None

    if base != "r1":
        return None
    if offset < 0:
        return None  # frame-setup negative offsets, not a local slot
    return offset, kind, size


# ── Slot fingerprint ─────────────────────────────────────────────────────────

@dataclass
class SlotFingerprint:
    offset: int
    accesses: int = 0
    loads: int = 0
    stores: int = 0
    addrs: int = 0
    kinds: Counter = field(default_factory=Counter)
    sizes: Counter = field(default_factory=Counter)
    indices: list = field(default_factory=list)
    opcodes: Counter = field(default_factory=Counter)

    @property
    def first_idx(self) -> int:
        return min(self.indices) if self.indices else -1

    @property
    def last_idx(self) -> int:
        return max(self.indices) if self.indices else -1

    @property
    def inferred_size(self) -> int:
        if self.sizes:
            return self.sizes.most_common(1)[0][0]
        return 0

    @property
    def dominant_kind(self) -> str:
        if self.kinds:
            return self.kinds.most_common(1)[0][0]
        return "?"

    def fingerprint(self) -> tuple:
        """Compact identity used for cross-side matching."""
        return (self.dominant_kind, self.inferred_size, self.loads, self.stores)

    def short_repr(self) -> str:
        return (f"{self.dominant_kind:6s} sz={self.inferred_size:<2d} "
                f"L={self.loads:<2d} S={self.stores:<2d} A={self.accesses:<3d} "
                f"[{self.first_idx}..{self.last_idx}]")

    def opcodes_repr(self) -> str:
        return ", ".join(f"{op}×{n}" for op, n in self.opcodes.most_common(3))


def build_fingerprints(side_key: str, instrs: list, skip_prologue: int = 0,
                       skip_epilogue_from: int = -1) -> dict[int, SlotFingerprint]:
    slots: dict[int, SlotFingerprint] = {}
    for ins in instrs:
        idx = ins.get("index", -1)
        if skip_prologue and idx < skip_prologue:
            # We do *not* skip prologue accesses for slot building; the
            # prologue's stw/stfd to callee-saved slots IS what we want
            # to see. The skip-prologue parameter is reserved for future use.
            pass
        side = ins.get(side_key)
        if not side:
            continue
        opcode = side.get("opcode", "")
        args = side.get("args", "")
        ref = parse_stack_ref(opcode, args)
        if not ref:
            continue
        offset, kind, size = ref
        slot = slots.setdefault(offset, SlotFingerprint(offset))
        slot.accesses += 1
        slot.indices.append(idx)
        slot.opcodes[opcode] += 1
        slot.kinds[kind] += 1
        if size:
            slot.sizes[size] += 1
        if opcode in LOAD_OPCODES:
            slot.loads += 1
        elif opcode in STORE_OPCODES:
            slot.stores += 1
        elif opcode in ADDR_OPCODES:
            slot.addrs += 1
    return slots


# ── Prologue analysis ────────────────────────────────────────────────────────

@dataclass
class Prologue:
    frame_size: int = 0
    saved_gpr_count: int = 0
    saved_fpr_count: int = 0
    callee_save_slots: set = field(default_factory=set)  # offsets used for callee-saved regs
    raw_savegprlr: Optional[int] = None
    raw_savefpr: Optional[int] = None


def _try_int(s: str) -> Optional[int]:
    try:
        return int(s, 0)
    except (ValueError, TypeError):
        return None


def parse_prologue(instrs: list, side_key: str) -> Prologue:
    """Walk the prologue to extract frame size + callee-saved counts.

    Stops at the first instruction that doesn't match a prologue pattern,
    so it handles 11-instr stack frames and 40-instr ones equally well.

    Detects callee-save styles:
      • __save(gpr|fpr|gprlr)_NN helpers (bl, with one or two underscores)
      • stmw rNN, off, r1  (saves rNN..r31 to one block)
      • Manual stfd/psq_st of fNN (NN >= 14) at top-of-frame offsets
      • Manual stw of rNN (NN >= 13) at top-of-frame offsets
    """
    p = Prologue()
    callee_fprs: set[int] = set()
    callee_gprs: set[int] = set()
    seen_stmw_gpr: Optional[int] = None
    # Scan a generous window; callee-save patterns won't false-trigger on body
    # code because they require fNN >= 14 / rNN >= 13 destinations.
    horizon = min(80, len(instrs))

    # Terminator: a call to anything OTHER than __save* / _save* exits the prologue.
    # All other ops we don't recognize simply get skipped.

    for ins in instrs[:horizon]:
        side = ins.get(side_key)
        if not side:
            continue
        op = side.get("opcode", "")
        args = side.get("args", "")
        parts = [a.strip() for a in args.split(",")]

        # stwu r1, -N, r1  → frame size = N
        if op == "stwu" and len(parts) >= 3 and parts[0] == "r1" and parts[2] == "r1":
            n = _try_int(parts[1])
            if n is not None and n < 0:
                p.frame_size = -n

        # bl _save(gpr|fpr|gprlr)_NN  (one or two leading underscores)
        if op == "bl":
            m = re.search(r"_+save(gpr|fpr|gprlr)_(\d+)", args)
            if m:
                kind = m.group(1)
                nn = int(m.group(2))
                count = 32 - nn
                if kind in ("gpr", "gprlr"):
                    p.raw_savegprlr = nn
                    p.saved_gpr_count = max(p.saved_gpr_count, count)
                elif kind == "fpr":
                    p.raw_savefpr = nn
                    p.saved_fpr_count = max(p.saved_fpr_count, count)
            else:
                # bl to a non-save helper is the end of the prologue
                break

        # stmw rN, off, r1  → saves rN..r31
        if op == "stmw" and len(parts) >= 3 and parts[0].startswith("r") and parts[2] == "r1":
            r = _try_int(parts[0][1:])
            off = _try_int(parts[1])
            if r is not None and off is not None:
                seen_stmw_gpr = max(seen_stmw_gpr or 0, 32 - r)
                # Each saved GPR occupies 4 bytes starting at off
                for i, reg in enumerate(range(r, 32)):
                    p.callee_save_slots.add(off + i * 4)

        # Manual callee-save: stfd / psq_st of fNN (NN>=14).
        if op in ("stfd", "psq_st") and len(parts) >= 3 and parts[2] == "r1":
            f = _try_int(parts[0].lstrip("f").lstrip("r"))
            off = _try_int(parts[1])
            if f is not None and off is not None and 14 <= f <= 31:
                callee_fprs.add(f)
                p.callee_save_slots.add(off)

        # Indexed paired-single callee-save: psq_stx fN, r1, r0, ...
        # No literal offset — but we can still count the FPR.
        if op == "psq_stx" and len(parts) >= 3 and parts[1] == "r1":
            f = _try_int(parts[0].lstrip("f").lstrip("r"))
            if f is not None and 14 <= f <= 31:
                callee_fprs.add(f)
            # Best-effort offset: the prior `li r0, OFF` set the index.
            # We don't track that here; the slot stays unmarked (acceptable
            # since the matching stfd usually catches it).

        # Manual callee-save: stw of rNN (NN>=13) — only if frame is set up
        # AND the offset is in the top portion of the frame (typical
        # callee-save area for GPRs).
        if op == "stw" and len(parts) >= 3 and parts[2] == "r1":
            r = _try_int(parts[0].lstrip("r"))
            off = _try_int(parts[1])
            if (r is not None and off is not None and 13 <= r <= 31
                    and p.frame_size and off >= p.frame_size - 0xa0):
                callee_gprs.add(r)
                p.callee_save_slots.add(off)

    if seen_stmw_gpr is not None:
        p.saved_gpr_count = max(p.saved_gpr_count, seen_stmw_gpr)
    if callee_fprs:
        p.saved_fpr_count = max(p.saved_fpr_count, len(callee_fprs))
    if callee_gprs:
        p.saved_gpr_count = max(p.saved_gpr_count, len(callee_gprs))

    # Saved LR slot at frame_size+4 (above the frame, in caller's space).
    # Detect: stw r0, (frame_size+4), r1 near the top.
    for ins in instrs[:horizon]:
        side = ins.get(side_key)
        if not side:
            continue
        op = side.get("opcode", "")
        args = side.get("args", "")
        parts = [a.strip() for a in args.split(",")]
        if op == "stw" and len(parts) >= 3 and parts[0] == "r0" and parts[2] == "r1":
            off = _try_int(parts[1])
            if off is not None and p.frame_size and off == p.frame_size + 4:
                p.callee_save_slots.add(off)

    return p


# ── Diff classification ─────────────────────────────────────────────────────

@dataclass
class Row:
    tgt_off: Optional[int]
    base_off: Optional[int]
    tgt: Optional[SlotFingerprint]
    base: Optional[SlotFingerprint]
    verdict: str
    note: str = ""
    callee_save: bool = False


def classify_slots(tgt_slots: dict[int, SlotFingerprint],
                   base_slots: dict[int, SlotFingerprint],
                   dominant_delta: int,
                   tgt_callee_save: set[int],
                   base_callee_save: set[int]) -> list[Row]:
    rows: list[Row] = []
    seen_base: set[int] = set()

    def is_cs(t_off: Optional[int], b_off: Optional[int]) -> bool:
        return ((t_off is not None and t_off in tgt_callee_save) or
                (b_off is not None and b_off in base_callee_save))

    # Pass 1: exact-offset pairing.
    for off in sorted(tgt_slots):
        tfp = tgt_slots[off]
        if off in base_slots:
            bfp = base_slots[off]
            verdict = "MATCH" if tfp.fingerprint() == bfp.fingerprint() else "DIFFER"
            rows.append(Row(off, off, tfp, bfp, verdict, callee_save=is_cs(off, off)))
            seen_base.add(off)
        else:
            rows.append(Row(off, None, tfp, None, "TGT_ONLY", callee_save=is_cs(off, None)))

    for off in sorted(base_slots):
        if off not in seen_base:
            rows.append(Row(None, off, None, base_slots[off], "BASE_ONLY",
                            callee_save=is_cs(None, off)))

    # Pass 2: detect SWAPPED among DIFFER rows
    # (A swap = two rows whose fingerprints are exchanged across sides.)
    # Skip callee-save rows — source reorder can't fix those.
    differ_rows = [r for r in rows if r.verdict == "DIFFER" and not r.callee_save]
    for i, r1 in enumerate(differ_rows):
        if r1.verdict != "DIFFER":
            continue
        for r2 in differ_rows[i + 1:]:
            if r2.verdict != "DIFFER":
                continue
            assert r1.tgt is not None and r1.base is not None
            assert r2.tgt is not None and r2.base is not None
            if (r1.tgt.fingerprint() == r2.base.fingerprint() and
                    r2.tgt.fingerprint() == r1.base.fingerprint()):
                r1.verdict = "SWAPPED"
                r1.note = f"with 0x{r2.tgt_off:x}"
                r2.verdict = "SWAPPED"
                r2.note = f"with 0x{r1.tgt_off:x}"
                break

    # Pass 3: detect SHIFTED — fold TGT_ONLY+BASE_ONLY pairs whose fingerprints
    # match across sides and whose offset delta equals the dominant frame Δ.
    # Skip callee-save rows.
    tgt_only = [r for r in rows if r.verdict == "TGT_ONLY" and not r.callee_save]
    base_only = [r for r in rows if r.verdict == "BASE_ONLY" and not r.callee_save]
    used_base_idxs: set[int] = set()

    for tr in tgt_only:
        assert tr.tgt is not None and tr.tgt_off is not None
        best = None
        for j, br in enumerate(base_only):
            if j in used_base_idxs:
                continue
            assert br.base is not None and br.base_off is not None
            if tr.tgt.fingerprint() != br.base.fingerprint():
                continue
            delta = br.base_off - tr.tgt_off
            # Prefer the dominant delta; fall back to any consistent shift.
            score = (0 if delta == dominant_delta else abs(delta - dominant_delta))
            if best is None or score < best[0]:
                best = (score, j, br, delta)
        if best is not None:
            _, j, br, delta = best
            tr.verdict = "SHIFTED"
            tr.base_off = br.base_off
            tr.base = br.base
            tr.note = (f"Δ{delta:+#x} (dominant)" if delta == dominant_delta
                       else f"Δ{delta:+#x}")
            used_base_idxs.add(j)

    # Drop the now-merged BASE_ONLY rows
    base_only_drop = {id(base_only[j]) for j in used_base_idxs}
    rows = [r for r in rows if id(r) not in base_only_drop]

    return rows


# ── Printing ─────────────────────────────────────────────────────────────────

VERDICT_ORDER = ["SWAPPED", "DIFFER", "SHIFTED", "TGT_ONLY", "BASE_ONLY", "MATCH"]


def print_report(rows: list[Row], tgt_prol: Prologue, base_prol: Prologue,
                 show_equal: bool, show_callee_save: bool,
                 dominant_delta: int, base_names: dict | None = None) -> None:
    print("=" * 84)
    print("STACK LAYOUT DIFF")
    print("=" * 84)
    print()

    frame_delta = base_prol.frame_size - tgt_prol.frame_size
    gpr_delta = base_prol.saved_gpr_count - tgt_prol.saved_gpr_count
    fpr_delta = base_prol.saved_fpr_count - tgt_prol.saved_fpr_count

    print(f"  Frame size:          TGT 0x{tgt_prol.frame_size:x}     "
          f"BASE 0x{base_prol.frame_size:x}     Δ {frame_delta:+#x}")
    print(f"  Callee-saved GPRs:   TGT {tgt_prol.saved_gpr_count:<5d}   "
          f"BASE {base_prol.saved_gpr_count:<5d}   Δ {gpr_delta:+d}")
    print(f"  Callee-saved FPRs:   TGT {tgt_prol.saved_fpr_count:<5d}   "
          f"BASE {base_prol.saved_fpr_count:<5d}   Δ {fpr_delta:+d}")

    callee_bytes = gpr_delta * 4 + fpr_delta * 8
    if frame_delta == 0:
        print("  → Frame sizes match.")
    elif callee_bytes == frame_delta:
        print(f"  → Frame Δ fully explained by callee-saved counts ({gpr_delta} GPR + "
              f"{fpr_delta} FPR = {callee_bytes:+#x} bytes). AT_LIMIT (not source-fixable).")
    else:
        leftover = frame_delta - callee_bytes
        print(f"  → Callee-saved Δ = {callee_bytes:+#x}; structural Δ remaining = {leftover:+#x}.")
    if dominant_delta:
        print(f"  Dominant body-offset shift: {dominant_delta:+#x}")
    print()

    # Sort: actionable verdicts first, then by tgt offset (or base if no tgt)
    rows_sorted = sorted(rows, key=lambda r: (
        VERDICT_ORDER.index(r.verdict) if r.verdict in VERDICT_ORDER else 99,
        r.tgt_off if r.tgt_off is not None else r.base_off or 0,
    ))
    rows_visible = list(rows_sorted)
    if not show_equal:
        rows_visible = [r for r in rows_visible if r.verdict != "MATCH"]
    if not show_callee_save:
        rows_visible = [r for r in rows_visible if not r.callee_save]

    # Header
    name_col = bool(base_names)
    if name_col:
        print(f"  {'TGT':>6s}  {'BASE':>6s}  {'verdict':9s}  "
              f"{'target slot':36s}  {'base slot':36s}  {'base var':20s}  note")
        print(f"  {'-' * 6}  {'-' * 6}  {'-' * 9}  {'-' * 36}  {'-' * 36}  {'-' * 20}  ----")
    else:
        print(f"  {'TGT':>6s}  {'BASE':>6s}  {'verdict':9s}  "
              f"{'target slot':36s}  {'base slot':36s}  note")
        print(f"  {'-' * 6}  {'-' * 6}  {'-' * 9}  {'-' * 36}  {'-' * 36}  ----")

    def name_for(off):
        if base_names is None or off is None:
            return ""
        info = base_names.get(off)
        return info.name if info else ""

    for r in rows_visible:
        t_off = f"0x{r.tgt_off:x}" if r.tgt_off is not None else "—"
        b_off = f"0x{r.base_off:x}" if r.base_off is not None else "—"
        t_fp = r.tgt.short_repr() if r.tgt else "—"
        b_fp = r.base.short_repr() if r.base else "—"
        tag = " [CS]" if r.callee_save else ""
        note = (r.note + tag).strip()
        if name_col:
            name = name_for(r.base_off) or ""
            print(f"  {t_off:>6s}  {b_off:>6s}  {r.verdict:9s}  {t_fp:36s}  {b_fp:36s}  {name:20s}  {note}")
        else:
            print(f"  {t_off:>6s}  {b_off:>6s}  {r.verdict:9s}  {t_fp:36s}  {b_fp:36s}  {note}")

    # Per-verdict counts (separating user vs callee-save)
    user_rows = [r for r in rows if not r.callee_save]
    cs_rows = [r for r in rows if r.callee_save]
    counts = Counter(r.verdict for r in user_rows)
    cs_counts = Counter(r.verdict for r in cs_rows)
    print()
    print("  Summary (user slots):")
    for v in VERDICT_ORDER:
        if counts[v]:
            print(f"    {v:10s} {counts[v]}")
    if cs_rows:
        cs_non_match = sum(c for v, c in cs_counts.items() if v != "MATCH")
        if cs_non_match:
            print(f"  Callee-save slots: {cs_non_match} non-matching (filtered; use --show-callee-save to inspect)")

    # Hints
    print()
    print("  Action hints:")
    if counts["SWAPPED"]:
        swap_rows = [r for r in user_rows if r.verdict == "SWAPPED"]
        # Pair up swaps for naming. Each swap row has note "with 0x..."
        seen_swap_pairs = set()
        pair_lines = []
        for r in swap_rows:
            if r.tgt_off is None:
                continue
            m = re.search(r"0x([0-9a-fA-F]+)", r.note)
            if not m:
                continue
            other = int(m.group(1), 16)
            key = tuple(sorted([r.tgt_off, other]))
            if key in seen_swap_pairs:
                continue
            seen_swap_pairs.add(key)
            a_name = (base_names.get(key[0]).name if base_names and base_names.get(key[0]) else "")
            b_name = (base_names.get(key[1]).name if base_names and base_names.get(key[1]) else "")
            if a_name or b_name:
                pair_lines.append(f"0x{key[0]:x} ({a_name or '?'}) ↔ 0x{key[1]:x} ({b_name or '?'})")
            else:
                pair_lines.append(f"0x{key[0]:x} ↔ 0x{key[1]:x}")
        if pair_lines:
            print(f"    • {len(seen_swap_pairs)} swap pair(s) — reorder the named declarations:")
            for line in pair_lines[:6]:
                print(f"        {line}")
            if len(pair_lines) > 6:
                print(f"        ... and {len(pair_lines) - 6} more")
        else:
            print(f"    • {counts['SWAPPED']} user slot(s) appear SWAPPED — reorder paired decls.")
    if counts["SHIFTED"] and dominant_delta:
        print(f"    • {counts['SHIFTED']} user slot(s) SHIFTED by {dominant_delta:+#x} — usually "
              "one side has an extra local that pushes the rest.")
    if counts["DIFFER"]:
        # Surface the affected base variable names
        differ_rows = [r for r in user_rows if r.verdict == "DIFFER"]
        named = []
        for r in differ_rows:
            if base_names and r.base_off is not None:
                info = base_names.get(r.base_off)
                if info:
                    named.append(f"0x{r.base_off:x} ({info.name})")
        if named:
            print(f"    • {counts['DIFFER']} user slot(s) DIFFER — different variable lives there. "
                  f"Base vars: {', '.join(named[:4])}{'...' if len(named) > 4 else ''}")
        else:
            print(f"    • {counts['DIFFER']} user slot(s) with same offset but different fingerprint "
                  "— different variable lives in that slot on each side; reorder candidates.")
    if counts["TGT_ONLY"]:
        print(f"    • {counts['TGT_ONLY']} slot(s) only on target — a source local our build "
              "elides, or different spill choice.")
    if counts["BASE_ONLY"]:
        print(f"    • {counts['BASE_ONLY']} slot(s) only on our build — extra spill or "
              "compiler temp. Often correlates with register pressure.")
    if not any(counts[v] for v in ("SWAPPED", "SHIFTED", "DIFFER", "TGT_ONLY", "BASE_ONLY")):
        if frame_delta == 0:
            print("    • User-slot layouts match. If diff is still poor, root cause is not "
                  "stack-layout (check regswaps / replaces / inserts).")
        elif callee_bytes == frame_delta:
            print("    • Pure callee-saved-register shift. AT_LIMIT.")
        else:
            print("    • Frame Δ exists but no user-slot mismatches surfaced — fingerprint "
                  "matching may be too coarse; inspect --show-callee-save and --show-equal.")


def dominant_delta_from_rows(tgt_slots: dict[int, SlotFingerprint],
                              base_slots: dict[int, SlotFingerprint]) -> int:
    """Best-effort dominant body offset delta — most common (base_off - tgt_off)
    across fingerprint-matched slot pairs."""
    deltas: Counter = Counter()
    base_by_fp: dict[tuple, list[int]] = defaultdict(list)
    for off, b in base_slots.items():
        base_by_fp[b.fingerprint()].append(off)
    for off, t in tgt_slots.items():
        for b_off in base_by_fp.get(t.fingerprint(), []):
            deltas[b_off - off] += 1
    if not deltas:
        return 0
    delta, _ = deltas.most_common(1)[0]
    return delta


# ── Main ─────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--symbol", required=True, help="Mangled symbol name")
    parser.add_argument("--unit", default=None, help="Unit for objdiff disambiguation")
    parser.add_argument("--project-dir", default=None, help="Project root")
    parser.add_argument("--show-equal", action="store_true",
                        help="Also list MATCH rows (default: hide)")
    parser.add_argument("--show-callee-save", action="store_true",
                        help="Also list callee-save (prologue/epilogue) slots (default: hide)")
    parser.add_argument("--no-names", action="store_true",
                        help="Skip MWCC DWARF recompile + name extraction. "
                             "By default the tool labels base-side slots with their source "
                             "variable names from a debug recompile (~1s extra).")
    parser.add_argument("--json-file", default=None,
                        help="Skip objdiff invocation; load diff JSON from this path")
    args = parser.parse_args()

    if args.json_file:
        json_path = args.json_file
    else:
        json_path = run_objdiff_for_symbol(
            args.symbol, project_dir=args.project_dir, unit=args.unit)

    with open(json_path) as f:
        data = json.load(f)

    instrs = data.get("instructions", [])
    if not instrs:
        print("No instructions in JSON.", file=sys.stderr)
        sys.exit(1)

    tgt_slots = build_fingerprints("target", instrs)
    base_slots = build_fingerprints("base", instrs)

    tgt_prol = parse_prologue(instrs, "target")
    base_prol = parse_prologue(instrs, "base")

    dominant_delta = dominant_delta_from_rows(tgt_slots, base_slots)
    rows = classify_slots(tgt_slots, base_slots, dominant_delta,
                          tgt_prol.callee_save_slots, base_prol.callee_save_slots)

    base_names: dict | None = None
    if not args.no_names:
        base_names = _try_extract_locals(args.symbol, args.project_dir)
        if not base_names:
            base_names = None  # collapse empty dict → no column

    print_report(rows, tgt_prol, base_prol, args.show_equal,
                 args.show_callee_save, dominant_delta, base_names)


if __name__ == "__main__":
    main()
