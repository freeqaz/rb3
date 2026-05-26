#!/usr/bin/env python3
"""Audit functions the normalized metric counts as "matched" but that are NOT
byte-exact, to find whether the masked arg diffs are benign (register/branch/
stack-frame noise) or real semantic differences the metric is hiding:

  - reloc_target : a relocation points at a DIFFERENT symbol  (wrong call / wrong global)
  - member       : non-stack memory offset differs            (wrong struct field / vtable slot)
  - constant     : a literal immediate differs                (wrong constant)

These three are the bugs that survive into a native port (registers get
reallocated by the host compiler; constants/offsets/call-targets do not).

Read-only: diffs already-built .o files (NO --build), so it is safe to run
alongside the permuter fleet and never touches the ninja lock.

Usage:
    python3 scripts/analysis/audit_normalized_masking.py            # inflated set (norm==100 & raw<100)
    python3 scripts/analysis/audit_normalized_masking.py --all-gap  # every fn where norm>raw
    python3 scripts/analysis/audit_normalized_masking.py --workers 4 --limit 50
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OBJDIFF = os.path.join(REPO, "bin", "objdiff-cli")
REPORT = os.path.join(REPO, "build", "SZBE69_B8", "report.json")

# PPC D-form memory ops: [dataReg, dispImm, baseReg]
LOADSTORE = {
    "lwz", "lwzu", "lbz", "lbzu", "lhz", "lhzu", "lha", "lhau", "lwa",
    "lfs", "lfsu", "lfd", "lfdu", "lmw", "psq_l", "psq_lu",
    "stw", "stwu", "stb", "stbu", "sth", "sthu",
    "stfs", "stfsu", "stfd", "stfdu", "stmw", "psq_st", "psq_stu",
}
ADDR_CALC = {"addi", "addic", "addic.", "subi", "addis"}
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


def second_register(typed_args):
    regs = [a.get("value") for a in typed_args if a.get("type") == "Register"]
    return regs[1] if len(regs) > 1 else (regs[0] if regs else None)


REVIEW_CATS = ("reloc_target", "member", "constant", "addr_const")


def norm_sym(s):
    """Collapse benign symbol-name noise so genuine wrong-target diffs stand out."""
    s = str(s)
    # Pool float/double constants: same value, different storage placement (benign).
    if (s.startswith("@F_") or s.startswith("@D_")
            or "floatBase" in s or "doubleBase" in s or "@stringBase" in s):
        return "POOL"
    # Anonymous-namespace discriminators and permuter working-copy artifacts.
    s = re.sub(r"@unnamed@.*$", "", s)
    s = re.sub(r"@.*$", "", s)
    # Compiler discriminators: __FUNCTION__$58088, foo__123 (benign renumbering).
    s = re.sub(r"\$\d+$", "", s)
    s = re.sub(r"__\d+$", "", s)
    return s


def value_sig(opcode, typed_args):
    """Per-instruction value signature: opcode + non-register value tokens, with
    frame/SDA memory displacements dropped (stack layout is allowed to differ).
    Returns None if there's nothing value-bearing to compare."""
    if opcode in LOADSTORE:
        base = base_reg_after_imm(typed_args)
    elif opcode in ADDR_CALC:
        base = second_register(typed_args)
    else:
        base = None
    drop_imm = base in FRAME_REGS or base in SDA_REGS
    imms, syms = [], []
    for a in typed_args:
        t = a.get("type")
        v = a.get("value")
        if t == "Register":
            continue
        if t in ("Signed", "Unsigned"):
            if not drop_imm:
                imms.append(("imm", int(v)))
        else:  # Symbol / Reloc / address label
            sv = str(v)
            # Bare address literal (intra-function branch target / jump label):
            # pure relative-layout noise, never a semantic difference.
            if re.fullmatch(r"-?0x[0-9a-fA-F]+", sv) or re.fullmatch(r"-?\d+", sv):
                continue
            syms.append(("sym", norm_sym(sv)))
    # When a relocation symbol is present, the immediate is its addend (an offset
    # into a pooled data/string/vtable section) — layout-dependent, so drop it and
    # compare only the symbol. Pure-immediate instructions (no reloc) keep their
    # immediate: that's a genuine member offset / constant (the TempoMap class).
    vals = syms if syms else imms
    return (opcode, tuple(vals))


def categorize_sig(sig):
    """Bucket a leftover (unmatched) value signature."""
    opcode, vals = sig
    if not vals:
        return "frame"          # only frame/sda displacement remained → benign
    syms = [v for v in vals if v[0] == "sym"]
    if syms:
        if all(v[1] == "POOL" for v in syms):
            return "pool"       # pool constant placement → benign
        return "reloc_target"   # references a genuinely different symbol
    if opcode in LOADSTORE:
        return "member"         # non-frame memory offset (wrong field / vtable slot)
    if opcode in ADDR_CALC:
        return "addr_const"
    return "constant"           # cmpwi / li / ori / mulli ... wrong literal


# ---------------------------------------------------------------------------
# Three noise-suppression filters added 2026-05-26 after the first audit pass
# surfaced ~50 in-scope REVIEW candidates of which ~10 were real bugs and the
# rest were known-benign patterns. See
# docs/sessions/2026-05-26-audit-refinements.md for the writeup.
# ---------------------------------------------------------------------------

# Calls that mark "assert / debug-fail" expansion sites. The ~5 instructions
# immediately before any of these calls hold the line number (`li r5, LINE`),
# the format-string pool offset (`addi rN, rN, @stringBase0+OFF`), and the
# file/function name — all of which drift with line numbers and pool layout
# without indicating a real semantic difference.
ASSERT_CALL_FRAGMENTS = (
    "MakeString",
    "Notify__5Debug",
    "Fail__5Debug",
    "__assert",
)
ASSERT_WINDOW_LOOKBACK = 5  # instructions before the bl call
ASSERT_WINDOW_LOOKAHEAD = 1  # the bl itself


def find_assert_windows(instrs, side):
    """Return a set of instruction indices that fall inside an assert/fail
    expansion window. `side` selects which sub-object ('target' or 'base') we
    scan for the call site. Only PURE-immediate (no reloc, no symbol arg) diffs
    inside these windows should be suppressed — reloc-target/member diffs are
    real and surface even inside an assert window.
    """
    blocked = set()
    for ins in instrs:
        sub = ins.get(side) or {}
        if sub.get("opcode") != "bl":
            continue
        args = sub.get("args", "") or ""
        if not any(t in args for t in ASSERT_CALL_FRAGMENTS):
            continue
        idx = ins.get("index")
        if idx is None:
            continue
        for off in range(idx - ASSERT_WINDOW_LOOKBACK, idx + ASSERT_WINDOW_LOOKAHEAD):
            if off >= 0:
                blocked.add(off)
    return blocked


def find_pool_base_regs(instrs, side):
    """For each instruction index, return the set of registers known to hold
    the `@stringBase0` pool base at that point. An `addi/addic. rD, rB, K` where
    rB is in that set is computing a pool offset — the immediate is layout-only
    noise.

    Tracked sequence (MWCC pool-base load idiom):
        lis  rB, @stringBase0
        addi rB, rB, @stringBase0    -> rB holds the pool base from here on

    A subsequent write to rB (other than refresh of the same sequence) clears
    it. We track per-instruction-index so the window of validity is precise.
    """
    pool_at = {}  # idx -> frozenset of regs holding pool base BEFORE this insn
    holding = set()
    saw_lis = {}  # reg -> True if a `lis rB, @stringBase0` is pending the `addi`
    for ins in instrs:
        idx = ins.get("index")
        if idx is None:
            continue
        # Snapshot the state SEEN BY this instruction first — addic./addi reads
        # its src reg's value at this point, before its own write.
        pool_at[idx] = frozenset(holding)

        sub = ins.get(side) or {}
        opcode = sub.get("opcode") or ""
        typed = sub.get("typed_args") or []
        regs = [a.get("value") for a in typed if a.get("type") == "Register"]
        syms = [a.get("value") for a in typed if a.get("type") == "Symbol"]
        has_string_base = any("@stringBase" in (s or "") for s in syms)

        if opcode == "lis" and has_string_base and regs:
            saw_lis[regs[0]] = True
            holding.discard(regs[0])
        elif opcode in ("addi", "addic", "addic.") and has_string_base and regs:
            #   addi rD, rB, @stringBase0           -> rD now holds pool base
            #   addi rD, rB, K, @stringBase0        -> rD = pool base + K
            dst = regs[0]
            src = regs[1] if len(regs) > 1 else None
            if src and (src in holding or saw_lis.get(src)):
                holding.add(dst)
                saw_lis.pop(src, None)
            elif saw_lis.get(dst):
                holding.add(dst)
                saw_lis.pop(dst, None)
            else:
                # New pool-base addi (no preceding lis tracked): still becomes
                # a pool reg because the immediate is the @stringBase0 reloc.
                holding.add(dst)
        else:
            # Any non-pool-tracking write to a register clears its status.
            if regs and opcode not in ("cmpw", "cmpwi", "cmplw", "cmplwi",
                                       "stw", "stwu", "stb", "stbu", "sth",
                                       "sthu", "stfs", "stfsu", "stfd",
                                       "stfdu", "psq_st", "psq_stu", "stmw",
                                       "b", "ba", "bl", "bla", "beq", "bne",
                                       "blt", "bgt", "ble", "bge", "bdnz",
                                       "bdz", "blr", "bctr", "bctrl", "mtctr",
                                       "mtlr", "mtspr"):
                holding.discard(regs[0])
                saw_lis.pop(regs[0], None)
    return pool_at


def is_pool_offset_addr(opcode, typed_args, pool_regs_at_idx):
    """True if this `addi/addic./subi` instruction is computing a string-pool
    offset (base reg holds @stringBase0). Such immediates are layout-only and
    should not be flagged as semantic differences."""
    if opcode not in ("addi", "addic", "addic.", "subi"):
        return False
    if not pool_regs_at_idx:
        return False
    # The base reg of `addi rD, rB, IMM` is the SECOND Register typed_arg.
    regs = [a.get("value") for a in typed_args if a.get("type") == "Register"]
    if len(regs) < 2:
        return False
    return regs[1] in pool_regs_at_idx


VT_RELOC_PREFIX = "__vt__"


def is_low_half_imm(v):
    """A 16-bit-fitting immediate (signed or unsigned)."""
    try:
        iv = int(v)
    except (TypeError, ValueError):
        return False
    return -0x10000 <= iv <= 0xffff


def diff_one(symbol, unit):
    """Run a read-only objdiff and classify masked arg diffs. Returns dict or None."""
    fd, path = tempfile.mkstemp(suffix=".json", prefix="audit_", dir="/tmp/objdiff_audit")
    os.close(fd)
    try:
        cmd = [OBJDIFF, "diff", "-p", REPO, symbol, "-u", unit,
               "--include-instructions", "-f", "json", "-o", path]
        r = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, timeout=120)
        if r.returncode != 0:
            return {"symbol": symbol, "unit": unit, "error": (r.stderr or "")[-200:]}
        with open(path) as f:
            d = json.load(f)
    except Exception as e:  # noqa: BLE001
        return {"symbol": symbol, "unit": unit, "error": str(e)[:200]}
    finally:
        try:
            os.unlink(path)
        except OSError:
            pass

    instrs = d.get("instructions", [])

    # FILTER 1: pre-scan target+base for assert/fail call-windows. Pure-imm
    # diffs inside these are MILO_ASSERT/HANDLE_CHECK line-number drift and
    # @stringBase0 pool offsets — not semantic bugs.
    assert_block = find_assert_windows(instrs, "target") | find_assert_windows(instrs, "base")

    # FILTER 2: pre-scan for instructions where the source register holds the
    # `@stringBase0` pool base — their immediate is a layout-only pool offset.
    pool_at_tgt = find_pool_base_regs(instrs, "target")
    pool_at_base = find_pool_base_regs(instrs, "base")

    # FILTER 3: pre-scan diff_arg instructions for the "literal-vs-reloc
    # construction" pattern — one side's typed_args have a pure-immediate that
    # fits in low/high 16 bits while the other side's typed_args carry a
    # `__vt__...` Symbol relocation. Both resolve to the same absolute address
    # (`lis r0, 0x80be; addi r3, r3, 0x3c08` vs `lis r0, __vt__...; addi r3,
    # r3, __vt__...`). Mark the index as benign so neither side contributes a
    # sig. We use BOTH-sides check (one pure-imm, other has __vt__ Symbol).
    lit_vs_reloc_block = set()
    for ins in instrs:
        if ins.get("match_type") != "diff_arg":
            continue
        idx = ins.get("index")
        if idx is None:
            continue
        t_typed = (ins.get("target") or {}).get("typed_args") or []
        b_typed = (ins.get("base") or {}).get("typed_args") or []
        t_syms = [a.get("value") for a in t_typed if a.get("type") == "Symbol"]
        b_syms = [a.get("value") for a in b_typed if a.get("type") == "Symbol"]
        t_imms = [a.get("value") for a in t_typed
                  if a.get("type") in ("Signed", "Unsigned")]
        b_imms = [a.get("value") for a in b_typed
                  if a.get("type") in ("Signed", "Unsigned")]
        t_has_vt = any(s and VT_RELOC_PREFIX in s for s in t_syms)
        b_has_vt = any(s and VT_RELOC_PREFIX in s for s in b_syms)
        # Case A: target is pure-imm (no Symbol) and base carries __vt__ reloc.
        if not t_syms and b_has_vt and t_imms and all(is_low_half_imm(v) for v in t_imms):
            lit_vs_reloc_block.add(idx)
            continue
        # Case B: base is pure-imm and target carries __vt__ reloc.
        if not b_syms and t_has_vt and b_imms and all(is_low_half_imm(v) for v in b_imms):
            lit_vs_reloc_block.add(idx)

    # Surface-level counts for context.
    surf = Counter()        # reg / branch
    other_mt = Counter()    # non diff_arg, non equal (should be ~0 when norm==100)
    # Whole-function value multisets (registers dropped, frame/sda displacements
    # dropped). A pure register-rename + instruction-reorder nets to an EQUAL
    # multiset; only genuine value differences survive.
    tgt_sigs, base_sigs = Counter(), Counter()
    sig_text = {}           # sig -> (ttext, stext) example for reporting

    for ins in instrs:
        mt = ins.get("match_type")
        if mt == "equal":
            continue
        if mt != "diff_arg":
            other_mt[mt] += 1
            continue
        idx = ins.get("index")
        bd = ins.get("diff_breakdown") or {}
        for arg in bd.get("arguments", []):
            at = arg.get("arg_type")
            if arg.get("target", {}).get("value") == arg.get("base", {}).get("value"):
                continue
            if at == "register":
                surf["reg"] += 1
            elif at == "branch_dest":
                surf["branch"] += 1
        tgt = ins.get("target") or {}
        base = ins.get("base") or {}
        ttext = f"{tgt.get('opcode','?')} {tgt.get('args','')}"
        stext = f"{base.get('opcode','?')} {base.get('args','')}"
        tgt_op = tgt.get("opcode", "")
        base_op = base.get("opcode", "")
        tgt_typed = tgt.get("typed_args") or []
        base_typed = base.get("typed_args") or []

        in_assert_window = idx in assert_block

        ts = value_sig(tgt_op, tgt_typed)
        bs = value_sig(base_op, base_typed)

        # FILTER 2: if the addi/addic./subi base reg held @stringBase0, the
        # immediate is a pool offset (layout-dependent). Drop the sig.
        if ts is not None and is_pool_offset_addr(
                tgt_op, tgt_typed, pool_at_tgt.get(idx, frozenset())):
            ts = None
        if bs is not None and is_pool_offset_addr(
                base_op, base_typed, pool_at_base.get(idx, frozenset())):
            bs = None

        # FILTER 1: inside an assert window, drop pure-immediate sigs. A sig
        # with a Symbol token (reloc-target) survives — that could be a real
        # wrong-callee even inside an assert window.
        def _is_pure_imm(sig):
            if sig is None:
                return False
            _, vals = sig
            return bool(vals) and all(v[0] == "imm" for v in vals)
        if in_assert_window:
            if _is_pure_imm(ts):
                ts = None
            if _is_pure_imm(bs):
                bs = None

        # FILTER 3: literal-vs-reloc construction at this index — both sides
        # encode the same absolute address differently. Drop both sigs.
        if idx in lit_vs_reloc_block:
            ts = None
            bs = None

        if ts is not None:
            tgt_sigs[ts] += 1
            sig_text.setdefault(ts, (ttext, stext))
        if bs is not None:
            base_sigs[bs] += 1
            sig_text.setdefault(bs, (ttext, stext))

    # Leftover = value sigs present on one side but not matched on the other.
    leftover = (tgt_sigs - base_sigs) + (base_sigs - tgt_sigs)

    cats = Counter()
    concerns = []
    for sig, n in leftover.items():
        if n <= 0:
            continue
        cat = categorize_sig(sig)
        cats[cat] += n
        if cat in REVIEW_CATS:
            tt, st = sig_text.get(sig, ("", ""))
            concerns.append((cat, tt, st))

    SEV = {"reloc_target": 10, "member": 8, "constant": 5, "addr_const": 3}
    severity = sum(SEV.get(k, 0) * v for k, v in cats.items())
    verdict = "REVIEW" if any(cats[k] for k in REVIEW_CATS) else "BENIGN"
    return {
        "symbol": symbol, "unit": unit,
        "raw": d.get("raw_match_percent"),
        "norm": d.get("normalized_match_percent"),
        "surface": dict(surf),
        "cats": dict(cats),
        "other_mt": dict(other_mt),
        "severity": severity,
        "verdict": verdict,
        "concerns": concerns[:12],
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--workers", type=int, default=6)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--all-gap", action="store_true",
                    help="audit every fn where norm>raw (not just norm==100)")
    ap.add_argument("--out", default="/tmp/objdiff_audit/results.json")
    args = ap.parse_args()

    os.makedirs("/tmp/objdiff_audit", exist_ok=True)
    rep = json.load(open(REPORT))
    targets = []
    for u in rep["units"]:
        un = u.get("name")
        for f in (u.get("functions") or []):
            n = f.get("match_percent_normalized")
            raw = f.get("fuzzy_match_percent")
            if n is None or raw is None:
                continue
            keep = (n > raw) if args.all_gap else (n == 100.0 and raw < 100.0)
            if keep:
                targets.append((f["name"], un, raw, n, int(f.get("size", 0))))
    targets.sort(key=lambda t: t[2])  # lowest raw first
    if args.limit:
        targets = targets[:args.limit]
    print(f"Auditing {len(targets)} functions with {args.workers} workers (read-only)...",
          file=sys.stderr)

    results = []
    done = 0
    with ThreadPoolExecutor(max_workers=args.workers) as ex:
        futs = {ex.submit(diff_one, sym, un): (sym, un) for sym, un, *_ in targets}
        for fut in as_completed(futs):
            results.append(fut.result())
            done += 1
            if done % 100 == 0:
                print(f"  {done}/{len(targets)}", file=sys.stderr)

    json.dump(results, open(args.out, "w"), indent=1)
    review = [r for r in results if r.get("verdict") == "REVIEW"]
    benign = [r for r in results if r.get("verdict") == "BENIGN"]
    errors = [r for r in results if r.get("error")]

    agg = Counter()
    for r in results:
        for k, v in (r.get("cats") or {}).items():
            agg[k] += v

    print("\n" + "=" * 70)
    print("NORMALIZED-METRIC MASKING AUDIT")
    print("=" * 70)
    print(f"audited     : {len(results)}")
    print(f"  BENIGN    : {len(benign)}  (only reg/branch/frame/sda diffs)")
    print(f"  REVIEW    : {len(review)}  (has reloc-target / member-offset / constant diffs)")
    print(f"  errors    : {len(errors)}")
    print("\nUnmatched value-sig totals by category (genuine value differences only;")
    print("register renames and instruction reorders already cancelled out):")
    for k, v in agg.most_common():
        tag = {"frame": "benign", "pool": "benign", "addr_const": "REVIEW",
               "constant": "REVIEW", "member": "REVIEW", "reloc_target": "REVIEW"}.get(k, "?")
        print(f"  {k:14s}: {v:6d}  [{tag}]")

    review.sort(key=lambda r: -r.get("severity", 0))
    print("\n" + "-" * 70)
    print("TOP REVIEW CANDIDATES (metric may be hiding a real semantic diff)")
    print("-" * 70)
    for r in review[:50]:
        cats = r.get("cats", {})
        flags = ",".join(f"{k}={cats[k]}" for k in REVIEW_CATS if cats.get(k))
        print(f"\n[{r['severity']:3d}] raw {r['raw']:.2f}%  {r['unit']}  ::  {r['symbol'][:55]}")
        print(f"      {flags}")
        for cat, tgt, src in r.get("concerns", [])[:4]:
            print(f"        [{cat}]  TGT {tgt[:42]:42s}  SRC {src[:42]}")
    print(f"\nFull results: {args.out}")


if __name__ == "__main__":
    main()
