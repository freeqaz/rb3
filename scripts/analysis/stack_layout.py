#!/usr/bin/env python3
"""Compare stack-frame layouts between target and base compilations.

For a given function, walk the objdiff target+base instruction stream and
build per-offset "slot fingerprints" (opcode family, inferred size, access
count, instruction-index span). Diff the two fingerprint maps to identify:

  MATCH      -- same offset, same fingerprint, AND same aligned access rows
  PERMUTED   -- same offset, same fingerprint, but touched at DIFFERENT rows
  SHIFTED    -- same fingerprint, offset differs by the dominant frame Δ
  SWAPPED    -- two slots' fingerprints exchanged (declaration reorder lever)
  DIFFER     -- same offset, different fingerprint (unresolved)
  TGT_ONLY   -- offset present only on target side
  BASE_ONLY  -- offset present only on our build

Also parses the prologue to report frame size and callee-saved register counts;
if the frame delta is fully explained by callee-saved counts, flags AT_LIMIT.

Toolchain: MWCC CodeWarrior for Wii (PowerPC Gekko/Broadway). Frame-allocation
forms MEASURED over all 41,254 functions in build/SZBE69_B8/asm (see
_scan_frame_size for the table).

Usage:
    python3 scripts/analysis/stack_layout.py --symbol "Mangled__FName" [--unit U] [--show-equal]
    python3 scripts/analysis/stack_layout.py --selftest
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
        """Compact identity used for cross-side matching.

        NOT sufficient on its own to conclude "same variable" — see index_set().
        """
        return (self.dominant_kind, self.inferred_size, self.loads, self.stores)

    def index_set(self) -> frozenset:
        """Aligned-row indices at which this slot is touched.

        ★ THIS is the discriminator v1 threw away. objdiff aligns target and
        base into ONE row stream, so row N names the same program point on both
        sides. Two slots at the same offset holding the SAME variable are
        touched at the SAME rows; a permutation of variables across a set of
        identically-shaped slots keeps every fingerprint equal but moves the
        rows. v1's fingerprint() is (kind,size,loads,stores) only -- which for a
        run of same-typed locals is CONSTANT, so v1 could not tell "same
        variable" from "some other variable that happens to look alike".
        short_repr() has been PRINTING [first..last] the whole time; the verdict
        just never consulted it.
        """
        return frozenset(self.indices)

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
    # ★ TRI-STATE, and half the point of the 2026-08-04 repair (back-ported from
    #   rb3-xenon 111ea902, lane DQ-2).
    #   None  = COULD NOT DETERMINE (an allocation form we do not model)
    #   0     = scanned, positively found NO frame allocation (frameless leaf)
    #   N > 0 = measured frame size
    # v1 used a plain `int = 0`, conflating "unparsed" with "frameless", so an
    # unparsed prologue on BOTH sides printed "→ Frame sizes match." off 0 == 0.
    frame_size: Optional[int] = None
    frame_evidence: str = "not scanned"
    # Same tri-state for the callee-save counts, so an unrecognised save helper
    # cannot silently read 0 against a real count on the other side.
    saved_gpr_count: Optional[int] = None
    saved_fpr_count: Optional[int] = None
    callee_save_slots: set = field(default_factory=set)  # offsets used for callee-saved regs
    raw_savegprlr: Optional[int] = None
    raw_savefpr: Optional[int] = None
    # `bl _save*` targets whose register count we could NOT parse. Empty in the
    # SZBE69_B8 target today (measured: 14,184 numbered call sites, 0 bare), but
    # recording them is what keeps a future unmodelled form from reading as 0.
    unparsed_saves: list = field(default_factory=list)

    @property
    def frame_known(self) -> bool:
        return self.frame_size is not None


def _try_int(s: str) -> Optional[int]:
    try:
        return int(s, 0)
    except (ValueError, TypeError):
        return None


def _maxi(a: Optional[int], b: Optional[int]) -> Optional[int]:
    """max() that treats None as 'no value yet' instead of crashing."""
    if a is None:
        return b
    if b is None:
        return a
    return max(a, b)


# ── Frame-size scanning ──────────────────────────────────────────────────────
#
# MEASURED over all 41,254 functions in 1,876 target .s files under
# build/SZBE69_B8/asm (scripts count == report.json's total_functions, which is
# the freshness control -- see the commit message for the staleness discussion):
#
#     STWU    26,856  65.10%   stwu r1, -N, r1               <- v1 handled only this
#     NONE    14,323  34.72%   no frame allocation (leaf)
#     STWUX       73   0.18%   clrlwi/subfic + stwux r1,r1,rX <- v1 read 0 here
#     ADDI         2   0.00%   subi r1, r1, N                 <- v1 read 0 here
#
# The Wii STWUX form is NOT the Xenon one. MWCC emits it for over-aligned
# frames, and the allocation is DYNAMIC:
#
#     clrlwi r11, r1, 27        ; r11 = r1 & 0x1f  (current misalignment)
#     mr     r12, r1            ; stash the old sp
#     subfic r11, r11, -0x200   ; r11 = -0x200 - (r1 & 0x1f)
#     stwux  r1, r1, r11        ; allocate
#
# so the true size is NOMINAL + (sp & 0x1f) and is only known at run time. All
# 73 sites in the binary have exactly this shape (measured: every stwux r1,r1,rX
# is preceded by clrlwi + mr + subfic, 73/73). We report the NOMINAL constant --
# both sides compute the alignment slack identically, so nominal-vs-nominal is a
# sound comparison -- and say so in the evidence string.
#
# The *representation* bug is the one that generalises: v1's `int = 0` meant any
# form outside the 65.10% silently read 0, and 0 == 0 printed "frames match".


def _scan_frame_size_v1(instrs: list, side_key: str, horizon: int) -> int:
    """SUPERSEDED 2026-08-04. Retained ONLY so the selftest can assert it STAYS
    WRONG. Handles `stwu r1, -N, r1` and nothing else, and returns a plain 0 --
    indistinguishable from a frameless leaf -- otherwise."""
    size = 0
    for ins in instrs[:horizon]:
        side = ins.get(side_key)
        if not side:
            continue
        op = side.get("opcode", "")
        parts = [a.strip() for a in side.get("args", "").split(",")]
        if op == "stwu" and len(parts) >= 3 and parts[0] == "r1" and parts[2] == "r1":
            n = _try_int(parts[1])
            if n is not None and n < 0:
                size = -n
    return size


def _scan_frame_size(instrs: list, side_key: str, horizon: int) -> tuple:
    """-> (size_or_None, evidence_str).

    Preference order: stwu > stwux(nominal) > addi/subi > positively-frameless.
    Returns None -- never 0 -- when an allocation is present but undecodable.
    """
    consts: dict = {}          # reg -> last materialized constant (signed)
    stwu_size = None
    stwux_size = None
    stwux_note = None
    addi_size = None
    addi_note = None
    saw_alloc_form = False

    def _sext(v: int) -> int:
        return v - (1 << 32) if v >= (1 << 31) else v

    for ins in instrs[:horizon]:
        side = ins.get(side_key)
        if not side:
            continue
        op = side.get("opcode", "")
        parts = [a.strip() for a in side.get("args", "").split(",")]

        # --- constant materialization: lis / li / ori / subfic ---------------
        if op == "lis" and len(parts) == 2:
            v = _try_int(parts[1])
            if v is not None:
                consts[parts[0]] = _sext((v << 16) & 0xFFFFFFFF)
            continue
        if op == "li" and len(parts) == 2:
            v = _try_int(parts[1])
            if v is not None:
                consts[parts[0]] = v
            continue
        if op == "ori" and len(parts) == 3 and parts[0] == parts[1]:
            v = _try_int(parts[2])
            if v is not None and parts[0] in consts:
                consts[parts[0]] = _sext((consts[parts[0]] | (v & 0xFFFF)) & 0xFFFFFFFF)
            continue
        # subfic rD, rA, SIMM  =>  rD = SIMM - rA. In the MWCC aligned-frame
        # prologue rA is the alignment slack (r1 & 0x1f); SIMM is the NOMINAL
        # negative frame size, which is what we want.
        if op == "subfic" and len(parts) == 3:
            v = _try_int(parts[2])
            if v is not None:
                consts[parts[0]] = v
            continue

        # --- stwu r1, -N, r1  (65.10% of functions) -------------------------
        if op == "stwu" and len(parts) >= 3 and parts[0] == "r1" and parts[2] == "r1":
            saw_alloc_form = True
            n = _try_int(parts[1])
            if n is not None and n < 0 and stwu_size is None:
                stwu_size = -n
            continue

        # --- stwux r1, r1, rX  (MWCC over-aligned frames; 73 functions) ------
        if op == "stwux" and len(parts) >= 3 and parts[0] == "r1" and parts[1] == "r1":
            saw_alloc_form = True
            reg = parts[2]
            raw = consts.get(reg)
            if raw is None:
                stwux_note = f"stwux via {reg} (constant not resolvable)"
            elif raw < 0 and stwux_size is None:
                stwux_size = -raw
                stwux_note = (f"stwux r1,r1,{reg}: nominal {stwux_size:#x} "
                              "(+ runtime 32B-alignment slack)")
            else:
                stwux_note = f"stwux via {reg}={raw:#x} (not a negative allocation)"
            continue

        # --- subi/addi r1, r1, +-N  (non-update allocation; 2 functions) -----
        if op in ("subi", "addi", "addic") and len(parts) >= 3 \
                and parts[0] == "r1" and parts[1] == "r1":
            n = _try_int(parts[2])
            if n is None:
                saw_alloc_form = True
                continue
            size = n if op == "subi" else -n
            if size > 0:
                saw_alloc_form = True
                if addi_size is None:
                    addi_size = size
                    addi_note = f"{op} r1, r1, {parts[2]}"
            continue

    if stwu_size is not None:
        return stwu_size, f"stwu r1, -{stwu_size:#x}, r1"
    if stwux_size is not None:
        return stwux_size, stwux_note
    if addi_size is not None:
        return addi_size, addi_note
    if saw_alloc_form:
        # We SAW an allocation but could not decode it. Never report 0 here.
        return None, (stwux_note or "frame allocation present but not decodable")
    return 0, "no frame allocation found in prologue (frameless/leaf)"


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

    # Pass 1: frame size (tri-state; see _scan_frame_size for the measured forms).
    p.frame_size, p.frame_evidence = _scan_frame_size(instrs, side_key, horizon)

    # Terminator: a call to anything OTHER than __save* / _save* exits the prologue.
    # All other ops we don't recognize simply get skipped.

    for ins in instrs[:horizon]:
        side = ins.get(side_key)
        if not side:
            continue
        op = side.get("opcode", "")
        args = side.get("args", "")
        parts = [a.strip() for a in args.split(",")]

        # (frame size is handled by _scan_frame_size above, tri-state.)

        # bl _save(gpr|fpr|gprlr)_NN  (one or two leading underscores)
        if op == "bl":
            m = re.search(r"_+save(gpr|fpr|gprlr)_(\d+)", args)
            if m:
                kind = m.group(1)
                nn = int(m.group(2))
                count = 32 - nn
                if kind in ("gpr", "gprlr"):
                    p.raw_savegprlr = nn
                    p.saved_gpr_count = _maxi(p.saved_gpr_count, count)
                elif kind == "fpr":
                    p.raw_savefpr = nn
                    p.saved_fpr_count = _maxi(p.saved_fpr_count, count)
            elif re.search(r"_+save(gpr|fpr|gprlr|vmx)", args):
                # ★ A save helper whose register count we could NOT read. v1 had
                #   no such branch: the `else` below simply broke out of the
                #   prologue scan and the count stayed at its `int = 0` default,
                #   which is indistinguishable from "positively saves nothing".
                #   That is the xenon `bl __savegprlr` (bare, no _NN) bug --
                #   MEASURED ABSENT here (14,184 numbered call sites, 0 bare, and
                #   config/SZBE69_B8/symbols.txt defines no unnumbered
                #   _savegpr/_savefpr symbol at all), so this branch records
                #   rather than guesses. It exists so the next unmodelled form
                #   surfaces instead of reading as a confident zero.
                p.unparsed_saves.append(args.strip())
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
        p.saved_gpr_count = _maxi(p.saved_gpr_count, seen_stmw_gpr)
    if callee_fprs:
        p.saved_fpr_count = _maxi(p.saved_fpr_count, len(callee_fprs))
    if callee_gprs:
        p.saved_gpr_count = _maxi(p.saved_gpr_count, len(callee_gprs))

    # We scanned the whole prologue window. If nothing at all was found AND no
    # save helper went unparsed, that is a positive "no callee saves", not an
    # unknown — settle it to 0 rather than leaving None to propagate as a fake
    # delta. If a save WAS seen but not decoded, the count stays None.
    if p.saved_gpr_count is None and not p.unparsed_saves:
        p.saved_gpr_count = 0
    if p.saved_fpr_count is None and not p.unparsed_saves:
        p.saved_fpr_count = 0

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


def _positional_partner(t: SlotFingerprint,
                        base_slots: dict[int, SlotFingerprint]) -> Optional[int]:
    """Base offset whose access rows overlap this target slot's the most.

    Fingerprint-free, so it works precisely where fingerprints are degenerate.
    Returns None when nothing overlaps at all.
    """
    ts = t.index_set()
    if not ts:
        return None
    best_off, best_n = None, 0
    for off, b in base_slots.items():
        n = len(ts & b.index_set())
        if n > best_n:
            best_off, best_n = off, n
    return best_off


def classify_slots_v1(tgt_slots, base_slots, dominant_delta,
                      tgt_callee_save, base_callee_save) -> list[Row]:
    """SUPERSEDED 2026-08-04. Retained ONLY so the selftest can assert it STAYS
    WRONG -- it reports MATCH on a pure variable permutation.
    Do not call it for analysis."""
    return _classify_impl(tgt_slots, base_slots, dominant_delta,
                          tgt_callee_save, base_callee_save, positional=False)


def classify_slots(tgt_slots: dict[int, SlotFingerprint],
                   base_slots: dict[int, SlotFingerprint],
                   dominant_delta: int,
                   tgt_callee_save: set[int],
                   base_callee_save: set[int]) -> list[Row]:
    return _classify_impl(tgt_slots, base_slots, dominant_delta,
                          tgt_callee_save, base_callee_save, positional=True)


def _classify_impl(tgt_slots: dict[int, SlotFingerprint],
                   base_slots: dict[int, SlotFingerprint],
                   dominant_delta: int,
                   tgt_callee_save: set[int],
                   base_callee_save: set[int],
                   positional: bool = True) -> list[Row]:
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
            note = ""
            if tfp.fingerprint() != bfp.fingerprint():
                verdict = "DIFFER"
            elif not positional:
                verdict = "MATCH"          # ← v1: the vacuous verdict
            elif tfp.index_set() == bfp.index_set():
                verdict = "MATCH"          # same shape AND same program points
            else:
                # Same offset, indistinguishable fingerprints, but the two sides
                # touch the slot at DIFFERENT program points => it does not hold
                # the same variable. Never call this MATCH.
                verdict = "PERMUTED"
                partner = _positional_partner(tfp, base_slots)
                if partner is not None and partner != off:
                    note = f"target's slot ↔ base 0x{partner:x}"
                else:
                    note = "same offset, accesses do not line up"
            rows.append(Row(off, off, tfp, bfp, verdict, note=note,
                            callee_save=is_cs(off, off)))
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
            # ★ Ambiguity disclosure: this pairing is by FINGERPRINT, so when
            # several base-only slots share it the choice is arbitrary. Say so
            # rather than presenting an arbitrary pick as a finding.
            rivals = sum(1 for k, b2 in enumerate(base_only)
                         if k not in used_base_idxs and b2.base is not None
                         and b2.base.fingerprint() == tr.tgt.fingerprint())
            if rivals > 1:
                tr.note += f"  ⚠ ambiguous: {rivals} equal-fingerprint candidates"
            used_base_idxs.add(j)

    # Drop the now-merged BASE_ONLY rows
    base_only_drop = {id(base_only[j]) for j in used_base_idxs}
    rows = [r for r in rows if id(r) not in base_only_drop]

    return rows


# ── Printing ─────────────────────────────────────────────────────────────────

VERDICT_ORDER = ["SWAPPED", "DIFFER", "PERMUTED", "SHIFTED",
                 "TGT_ONLY", "BASE_ONLY", "MATCH"]


def print_report(rows: list[Row], tgt_prol: Prologue, base_prol: Prologue,
                 show_equal: bool, show_callee_save: bool,
                 dominant_delta: int, base_names: dict | None = None) -> bool:
    """Print the report. Returns True if the frame comparison is TRUSTWORTHY,
    False if it had to refuse (caller turns that into a nonzero exit)."""
    print("=" * 84)
    print("STACK LAYOUT DIFF")
    print("=" * 84)
    print()

    frame_ok = tgt_prol.frame_known and base_prol.frame_known
    saves_known = (tgt_prol.saved_gpr_count is not None
                   and base_prol.saved_gpr_count is not None
                   and tgt_prol.saved_fpr_count is not None
                   and base_prol.saved_fpr_count is not None)
    gpr_delta = ((base_prol.saved_gpr_count - tgt_prol.saved_gpr_count)
                 if saves_known else 0)
    fpr_delta = ((base_prol.saved_fpr_count - tgt_prol.saved_fpr_count)
                 if saves_known else 0)

    def fs(p):
        return f"0x{p.frame_size:x}" if p.frame_known else "UNKNOWN"

    def cnt(v):
        return str(v) if v is not None else "UNKNOWN"

    if frame_ok:
        frame_delta = base_prol.frame_size - tgt_prol.frame_size
        print(f"  Frame size:          TGT {fs(tgt_prol):<9s} "
              f"BASE {fs(base_prol):<9s} Δ {frame_delta:+#x}")
    else:
        frame_delta = None
        print(f"  Frame size:          TGT {fs(tgt_prol):<9s} "
              f"BASE {fs(base_prol):<9s} Δ UNKNOWN")
    print(f"  Callee-saved GPRs:   TGT {cnt(tgt_prol.saved_gpr_count):<7s} "
          f"BASE {cnt(base_prol.saved_gpr_count):<7s} "
          f"Δ {(f'{gpr_delta:+d}' if saves_known else 'UNKNOWN')}")
    print(f"  Callee-saved FPRs:   TGT {cnt(tgt_prol.saved_fpr_count):<7s} "
          f"BASE {cnt(base_prol.saved_fpr_count):<7s} "
          f"Δ {(f'{fpr_delta:+d}' if saves_known else 'UNKNOWN')}")
    print(f"    frame evidence: TGT {tgt_prol.frame_evidence}")
    print(f"                    BASE {base_prol.frame_evidence}")
    for side, prol in (("TGT", tgt_prol), ("BASE", base_prol)):
        if prol.unparsed_saves:
            print(f"    ⚠ {side} save helper(s) not decoded: "
                  f"{', '.join(prol.unparsed_saves[:3])}")

    callee_bytes = gpr_delta * 4 + fpr_delta * 8
    if not frame_ok:
        # ★ REFUSE. v1 defaulted an unparsed frame to 0 and then printed
        #   "→ Frame sizes match." off 0 == 0 -- a vacuous success. A frame we
        #   could not read is NOT a frame that matches.
        print()
        print("  ⛔ REFUSED: frame size could not be determined on "
              + ("both sides." if not (tgt_prol.frame_known or base_prol.frame_known)
                 else ("the TARGET side." if not tgt_prol.frame_known
                       else "the BASE side.")))
        print("     No frame verdict is reported. Note the callee-save slot filter")
        print("     is derived from the frame size, so the table below is NOT")
        print("     filtered for prologue slots and may contain them.")
    elif frame_delta == 0:
        print("  → Frame sizes match.")
    elif not saves_known:
        print(f"  → Frame Δ {frame_delta:+#x}, but the callee-save counts are UNKNOWN, "
              "so it cannot be attributed. No AT_LIMIT verdict.")
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

    # ★ Discriminating power of the per-slot signature. If many user slots share
    #   one fingerprint, any fingerprint-based pairing among them is arbitrary,
    #   and a bare "MATCH n" would be reporting a coincidence as a finding.
    fp_counts: Counter = Counter()
    for r in rows:
        if r.callee_save or r.tgt is None:
            continue
        fp_counts[r.tgt.fingerprint()] += 1
    degenerate = {f: n for f, n in fp_counts.items() if n > 1}
    if degenerate:
        worst = max(degenerate.values())
        n_amb = sum(degenerate.values())
        print(f"    ── signature discriminating power: {n_amb} of "
              f"{sum(fp_counts.values())} target slots share a fingerprint with "
              f"another (largest group {worst}).")
        print("       Fingerprint-only pairing is ARBITRARY within those groups; "
              "MATCH/PERMUTED\n       above is decided by aligned access rows, "
              "not by fingerprint.")
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
    if counts["PERMUTED"]:
        print(f"    • {counts['PERMUTED']} user slot(s) PERMUTED — both sides use the same "
              "slot at\n      different program points, i.e. the SAME SET of slots with "
              "variables\n      assigned differently. Slot-allocation shaping, not a "
              "declaration\n      count difference. Read the '↔ base 0x..' notes for the "
              "mapping.")
    if counts["TGT_ONLY"]:
        print(f"    • {counts['TGT_ONLY']} slot(s) only on target — a source local our build "
              "elides, or different spill choice.")
    if counts["BASE_ONLY"]:
        print(f"    • {counts['BASE_ONLY']} slot(s) only on our build — extra spill or "
              "compiler temp. Often correlates with register pressure.")
    if not any(counts[v] for v in ("SWAPPED", "SHIFTED", "DIFFER", "PERMUTED",
                                   "TGT_ONLY", "BASE_ONLY")):
        if frame_delta is None:
            print("    • No user-slot mismatches, but the frame size is UNKNOWN — this is "
                  "NOT a clean bill of health.")
        elif frame_delta == 0:
            print("    • User-slot layouts match. If diff is still poor, root cause is not "
                  "stack-layout (check regswaps / replaces / inserts).")
        elif saves_known and callee_bytes == frame_delta:
            print("    • Pure callee-saved-register shift. AT_LIMIT.")
        else:
            print("    • Frame Δ exists but no user-slot mismatches surfaced — fingerprint "
                  "matching may be too coarse; inspect --show-callee-save and --show-equal.")
    return frame_ok


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


# ── Selftest / regression fixtures ───────────────────────────────────────────
#
# Every fixture is built IN MEMORY: no toolchain, no objdiff, no filesystem, so
# this can never silently SKIP.
#
# ★ The load-bearing property is NOT "the new code passes". It is that the OLD
#   code is asserted to STAY WRONG on the same fixtures. A selftest that only
#   demonstrates the new comparator would pass just as happily after somebody
#   "simplified" the fix back out.

def _ins(idx, t=None, b=None):
    """One objdiff-shaped aligned row. t/b are (opcode, args) or None."""
    d = {"index": idx}
    if t is not None:
        d["target"] = {"opcode": t[0], "args": t[1]}
    if b is not None:
        d["base"] = {"opcode": b[0], "args": b[1]}
    return d


def _aligned_frame_prologue(nominal, idx0=0):
    """The MWCC over-aligned-frame shape MEASURED in all 73 rb3 stwux sites:
        clrlwi r11, r1, 27 / mr r12, r1 / subfic r11, r11, -N / stwux r1,r1,r11
    """
    return [
        _ins(idx0 + 0, ("clrlwi", "r11, r1, 27"), ("clrlwi", "r11, r1, 27")),
        _ins(idx0 + 1, ("mr", "r12, r1"), ("mr", "r12, r1")),
        _ins(idx0 + 2, ("subfic", f"r11, r11, -{nominal:#x}"),
                       ("subfic", f"r11, r11, -{nominal:#x}")),
        _ins(idx0 + 3, ("stwux", "r1, r1, r11"), ("stwux", "r1, r1, r11")),
    ]


def _mix(tgt_rows, base_rows):
    """Splice two same-length single-side fixtures into one aligned stream."""
    out = []
    for t, b in zip(tgt_rows, base_rows):
        ts = t.get("target")
        bs = b.get("base")
        out.append(_ins(t["index"],
                        (ts["opcode"], ts["args"]) if ts else None,
                        (bs["opcode"], bs["args"]) if bs else None))
    return out


def selftest():
    out = []
    ok = True
    n_checks = 0

    def check(label, got, want):
        nonlocal ok, n_checks
        n_checks += 1
        good = got == want
        if not good:
            ok = False
        out.append(f"  [{'ok' if good else 'FAIL'}] {label}: got {got!r}, want {want!r}")

    H = 80

    # ── Fixture 1: over-aligned frame, and the two sides genuinely DIFFER ────
    # This is the killer. v1 reads 0 on both sides and therefore declares the
    # frames EQUAL, when they in fact differ by 0x40. A wrong number would be
    # bad; a confident "match" on a real difference is the vacuous-success shape.
    mixed = _mix(_aligned_frame_prologue(0x200), _aligned_frame_prologue(0x240))
    out.append("fixture 1 — MWCC over-aligned frame (clrlwi/subfic/stwux), "
               "sides differ by 0x40:")
    check("v1 TARGET frame (STAYS WRONG)", _scan_frame_size_v1(mixed, "target", H), 0)
    check("v1 BASE   frame (STAYS WRONG)", _scan_frame_size_v1(mixed, "base", H), 0)
    check("v1 verdict is a FALSE 'frames match'",
          _scan_frame_size_v1(mixed, "target", H) == _scan_frame_size_v1(mixed, "base", H),
          True)
    check("v2 TARGET frame (nominal)", _scan_frame_size(mixed, "target", H)[0], 0x200)
    check("v2 BASE   frame (nominal)", _scan_frame_size(mixed, "base", H)[0], 0x240)
    check("v2 correctly sees a difference",
          _scan_frame_size(mixed, "target", H)[0] != _scan_frame_size(mixed, "base", H)[0],
          True)
    check("v2 evidence names the dynamic slack",
          "alignment slack" in _scan_frame_size(mixed, "target", H)[1], True)

    # ── Fixture 2: the non-update allocation form (subi r1, r1, N) ──────────
    # 2 functions in the binary (InitMetroTRK & friends). v1 reads 0 for both
    # sides again -> another vacuous "frames match".
    sub = [_ins(0, ("subi", "r1, r1, 0x4"), ("subi", "r1, r1, 0x8")),
           _ins(1, ("stw", "r3, 0x0, r1"), ("stw", "r3, 0x0, r1"))]
    out.append("fixture 2 — `subi r1, r1, N` allocation:")
    check("v1 TARGET frame (STAYS WRONG)", _scan_frame_size_v1(sub, "target", H), 0)
    check("v1 declares a FALSE match",
          _scan_frame_size_v1(sub, "target", H) == _scan_frame_size_v1(sub, "base", H), True)
    check("v2 TARGET frame", _scan_frame_size(sub, "target", H)[0], 0x4)
    check("v2 BASE   frame", _scan_frame_size(sub, "base", H)[0], 0x8)

    # ── Fixture 3: CONTROL — v2 must not over-fire ──────────────────────────
    # A plain stwu frame must still parse identically to v1, and a frameless
    # leaf must read 0-KNOWN, not UNKNOWN. Without this, v2 could be
    # `return None` and fixtures 1-2 would still pass.
    out.append("fixture 3 — CONTROL: v2 must agree with v1 where v1 was right:")
    plain = [_ins(0, ("stwu", "r1, -0x50, r1"), ("stwu", "r1, -0x50, r1")),
             _ins(1, ("mflr", "r0"), ("mflr", "r0")),
             _ins(2, ("bl", "_savegpr_27"), ("bl", "_savegpr_27"))]
    check("v1 plain stwu frame", _scan_frame_size_v1(plain, "target", H), 0x50)
    check("v2 plain stwu frame (must AGREE)", _scan_frame_size(plain, "target", H)[0], 0x50)
    check("v2 saved GPRs for _savegpr_27",
          parse_prologue(plain, "target").saved_gpr_count, 5)
    leaf = [_ins(0, ("mr", "r11, r3"), ("mr", "r11, r3")),
            _ins(1, ("blr", ""), ("blr", ""))]
    check("v2 frameless leaf is KNOWN-zero, not UNKNOWN",
          _scan_frame_size(leaf, "target", H)[0], 0)
    check("v2 frameless leaf reports frame_known",
          parse_prologue(leaf, "target").frame_known, True)
    # ...and an allocation form we do NOT model must be UNKNOWN, never 0.
    weird = [_ins(0, ("stwux", "r1, r1, r7"), ("stwux", "r1, r1, r7"))]
    check("v2 undecodable allocation is UNKNOWN (None), never 0",
          _scan_frame_size(weird, "target", H)[0], None)
    check("v2 undecodable allocation sets frame_known False",
          parse_prologue(weird, "target").frame_known, False)

    # ── Fixture 4: the degenerate slot comparison ───────────────────────────
    # Two same-shaped locals at 0x60 and 0x68, with the ASSIGNMENT SWAPPED
    # between the sides. Every fingerprint is equal, so v1 reports MATCH twice
    # and zero actionable rows -- while the two builds demonstrably put
    # different variables in those slots.
    out.append("fixture 4 — variable permutation across identical-shaped slots:")
    perm = [
        _ins(0, ("stwu", "r1, -0x90, r1"), ("stwu", "r1, -0x90, r1")),
        _ins(1, ("lwz", "r3, 0x60, r1"), ("lwz", "r3, 0x68, r1")),
        _ins(2, ("lwz", "r4, 0x68, r1"), ("lwz", "r4, 0x60, r1")),
    ]
    ts = build_fingerprints("target", perm)
    bs = build_fingerprints("base", perm)
    v1c = Counter(r.verdict for r in classify_slots_v1(ts, bs, 0, set(), set()))
    v2_rows = classify_slots(ts, bs, 0, set(), set())
    v2c = Counter(r.verdict for r in v2_rows)
    check("v1 says MATCH x2 (STAYS WRONG)", v1c["MATCH"], 2)
    check("v1 finds ZERO actionable rows (STAYS WRONG)",
          sum(v for k, v in v1c.items() if k != "MATCH"), 0)
    check("v2 says MATCH x0", v2c["MATCH"], 0)
    check("v2 says PERMUTED x2", v2c["PERMUTED"], 2)
    check("v2 reports the positional mapping 0x60 ↔ base 0x68",
          any("0x68" in r.note for r in v2_rows if r.tgt_off == 0x60), True)

    # ── Fixture 5: CONTROL — a genuinely identical layout ───────────────────
    # Without this, v2 could be `return PERMUTED` and fixture 4 would pass.
    out.append("fixture 5 — CONTROL: identical layout must still read MATCH:")
    same = [
        _ins(0, ("stwu", "r1, -0x90, r1"), ("stwu", "r1, -0x90, r1")),
        _ins(1, ("lwz", "r3, 0x60, r1"), ("lwz", "r3, 0x60, r1")),
        _ins(2, ("lwz", "r4, 0x68, r1"), ("lwz", "r4, 0x68, r1")),
    ]
    ts2 = build_fingerprints("target", same)
    bs2 = build_fingerprints("base", same)
    v2c2 = Counter(r.verdict for r in classify_slots(ts2, bs2, 0, set(), set()))
    check("v2 says MATCH x2 on an identical layout", v2c2["MATCH"], 2)
    check("v2 says PERMUTED x0 on an identical layout", v2c2["PERMUTED"], 0)

    # ── Fixture 6: paired-single (Gekko-only) slots take the same path ──────
    # psq_st/psq_l are the Wii-specific opcodes; make sure the permutation test
    # is not accidentally int-only.
    out.append("fixture 6 — Gekko paired-single slots permute too:")
    ps = [
        _ins(0, ("stwu", "r1, -0x40, r1"), ("stwu", "r1, -0x40, r1")),
        _ins(1, ("psq_st", "f1, 0x10, r1, 0, qr0"), ("psq_st", "f1, 0x18, r1, 0, qr0")),
        _ins(2, ("psq_st", "f2, 0x18, r1, 0, qr0"), ("psq_st", "f2, 0x10, r1, 0, qr0")),
    ]
    tp = build_fingerprints("target", ps)
    bp = build_fingerprints("base", ps)
    check("paired slots are recognised", sorted(tp), [0x10, 0x18])
    v6 = Counter(r.verdict for r in classify_slots(tp, bp, 0, set(), set()))
    check("v2 says PERMUTED x2 on permuted paired-single slots", v6["PERMUTED"], 2)
    check("v1 says MATCH x2 on the same (STAYS WRONG)",
          Counter(r.verdict for r in classify_slots_v1(tp, bp, 0, set(), set()))["MATCH"], 2)

    # ── Fixture 7: save-helper counts stay tri-state ────────────────────────
    # rb3 has NO bare `bl _savegpr` (measured: 14,184 numbered, 0 bare), so the
    # xenon bug-3 shape cannot be reproduced from the binary. What we DO assert
    # is that an unrecognised save helper is recorded rather than read as 0.
    out.append("fixture 7 — an undecodable save helper must not read as 0 saves:")
    bare = [_ins(0, ("stwu", "r1, -0x50, r1"), ("stwu", "r1, -0x50, r1")),
            _ins(1, ("bl", "_savegpr"), ("bl", "_savegpr_14"))]
    pt = parse_prologue(bare, "target")
    pb = parse_prologue(bare, "base")
    check("TARGET records the unparsed helper", pt.unparsed_saves, ["_savegpr"])
    check("TARGET GPR count is UNKNOWN, not 0", pt.saved_gpr_count, None)
    check("BASE GPR count is read normally", pb.saved_gpr_count, 18)

    return ok, out, n_checks


# ── Main ─────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--symbol", help="Mangled symbol name")
    parser.add_argument("--selftest", action="store_true",
                        help="Run the in-memory regression fixtures (no toolchain, "
                             "no objdiff, no filesystem) and exit.")
    parser.add_argument("--allow-unknown-frame", action="store_true",
                        help="Exit 0 even when the frame size could not be determined. "
                             "Default is to REFUSE with exit 2.")
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

    if args.selftest:
        ok, lines, n_checks = selftest()
        print("# stack_layout selftest — frame-parse + slot-comparator regression")
        for ln in lines:
            print(ln)
        print(f"PASS ({n_checks} checks)" if ok else f"FAIL ({n_checks} checks)")
        sys.exit(0 if ok else 1)

    if not args.symbol:
        parser.error("--symbol is required (or use --selftest)")

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

    frame_ok = print_report(rows, tgt_prol, base_prol, args.show_equal,
                            args.show_callee_save, dominant_delta, base_names)
    if not frame_ok and not args.allow_unknown_frame:
        sys.exit(2)


if __name__ == "__main__":
    main()
