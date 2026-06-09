#!/usr/bin/env python3
"""Calibrate ghidriff's correlators against the CodeWarrior-map name oracle.

CONTEXT
-------
We ran ghidriff (VersionTrackingDiff, --no-symbols) on Bank 5 (band_r_wii.elf,
2009 DWARF build) vs Bank 8 (bank8_target.elf, 2010 target). Both banks have
full CW linker maps, and ~41k mangled symbols are common to both — those
same-name pairs are GROUND TRUTH for name-blind matching. This script scores
every correlator's matches against that oracle so we know which match types to
trust at which function sizes BEFORE pointing the pipeline at a cross-compiler
target (Xbox 360) where ground truth is scarce.

DATA SOURCES (all already on disk — this script runs NO Ghidra/ghidriff)
  *.ghidriff.matches.json   full proposal list: address_matches =
                            { bank5_addr_hex: [[bank8_addr_hex, [match_types]], ...] }
                            This is the COMPLETE per-correlator match record,
                            including pairs whose bodies were identical (which
                            the pdiff's functions.modified does NOT contain).
                            One-to-one correlators (Exact*Hasher, StringsRefs,
                            ...) record exactly their accepted matches here;
                            one-to-many correlators (BulkBasicBlockMnemonicHash)
                            record the full many-to-many explosion (~11.1M).
  *.ghidriff.json           the 3 GB pdiff; we stream ONLY functions.modified
                            for the accepted changed pairs WITH similarity
                            ratios (m_ratio/i_ratio) — used for the
                            Implied-Match m_ratio threshold sweep. (We join by
                            address, not by symbol, so no key collisions —
                            divergence_index.json loses ~23% of modified
                            entries to duplicate-b8_sym dict keys.)
  CW maps (Bank5 + Bank8)   addr -> (mangled, size) for .init/.text symbols;
                            the same-name intersection is the oracle.

METRICS
  precision (exact)  fraction of a correlator's matched pairs (both sides
                     resolvable to map symbols) where mangled b5_sym == b8_sym.
  precision (base)   softer: exact OR same base identifier before the first
                     "__" (distill_ghidriff.py's base_match logic) — counts
                     signature-only renames (Foo__A -> Foo__B) as correct.
  size buckets       Bank8 map size: <16B / 16-64B / 64-256B / 256B+.
  unique vs multi    a pair is "unique" if, WITHIN that correlator's proposals,
                     both endpoints appear exactly once (1:1). For one-to-many
                     correlators the multi subset is the pathological part.
  recall             of strict oracle pairs (name unique in both maps), which
                     correlator stage first proposed the CORRECT address pair.
  implied sweep      precision of Implied Match pairs at m_ratio gates,
                     informing a future --implied-min-ratio default.

SymbolsHash is reported for completeness but EXCLUDED from precision claims:
it matches BY NAME, i.e. it sees the oracle.

USAGE (use the ghidriff venv python — needs ijson/yajl2_c):
  build/SZBE69_B8/ghidra/ghidriff-venv/bin/python \
      tools/ghidra/calibrate_ghidriff.py [--skip-pdiff] [-o report.md]

Runtime: ~5-10 min (streams ~5.5 GB of JSON once each). Memory: < 1 GB.
"""
from __future__ import annotations

import argparse
import re
import sys
import time
from array import array
from collections import Counter, defaultdict
from pathlib import Path

RB3 = Path(__file__).resolve().parents[2]
GHIDRIFF_DIR = RB3 / "build" / "SZBE69_B8" / "ghidra" / "ghidriff"
DEFAULT_MATCHES = (GHIDRIFF_DIR / "json" /
                   "band_r_wii.elf-bank8_target.elf.ghidriff.matches.json")
DEFAULT_PDIFF = (GHIDRIFF_DIR / "json" /
                 "band_r_wii.elf-bank8_target.elf.ghidriff.json")
BANK8_MAP = RB3 / "orig" / "SZBE69_B8" / "files" / "band_r_wii.map"
BANK5_MAP = (RB3.parent / "milo-executable-library" / "rb3"
             / "Wii Proto (Bank 5) (Debug)" / "band_r_wii.map")

# Correlator run order (from ghidriff.log). Stage attribution for recall uses
# this order: the earliest correlator that proposed the correct pair.
ORDER = [
    "SymbolsHash",
    "ExactBytesFunctionHasher",
    "ExactInstructionsFunctionHasher",
    "StructuralGraphExactHash",
    "ExactMnemonicsFunctionHasher",
    "BulkInstructionHash",
    "SigCallingCalledHasher",
    "StringsRefsHasher",
    "StrUniqueFuncRefsHasher",
    "SwitchSigHasher",
    "StructuralGraphHash",
    "BulkBasicBlockMnemonicHash",
    "Implied Match",
    "Decomp Match",
]

CODE_SECTIONS = {".init", ".text"}
SECTION_RE = re.compile(r"^(\S+) section layout")
LINE_RE = re.compile(
    r"^  ([0-9a-fA-F]{8}) ([0-9a-fA-F]{6}) ([0-9a-fA-F]{8}) [0-9a-fA-F]{8}\s+\d+ (\S.*?) \t")

BUCKETS = ["<16B", "16-64B", "64-256B", "256B+"]


def bucket_of(size):
    if size is None:
        return None
    if size < 16:
        return 0
    if size < 64:
        return 1
    if size < 256:
        return 2
    return 3


def base_of(sym):
    return sym.split("__", 1)[0]


def base_match(a, b):
    """distill_ghidriff.py's base_match: same nonempty pre-'__' identifier."""
    return base_of(a) == base_of(b) and bool(base_of(a))


def all_section_names(path: Path):
    """All symbol names in the map, any section (for the headline common
    count — includes data symbols, which ghidriff function matching never
    sees; the calibration oracle below is code-sections-only)."""
    out = set()
    for line in path.read_text(errors="replace").splitlines():
        m = LINE_RE.match(line)
        if m and not m.group(4).startswith(".") and int(m.group(3), 16) != 0:
            out.add(m.group(4))
    return out


def parse_map(path: Path):
    """CW map -> (addr2symsize {addr:(sym,size)}, name2addrs {sym:set(addr)}),
    restricted to code sections (.init/.text). First symbol at an addr wins."""
    addr2 = {}
    name2 = defaultdict(set)
    section = None
    for line in path.read_text(errors="replace").splitlines():
        m = SECTION_RE.match(line)
        if m:
            section = m.group(1)
            continue
        if section not in CODE_SECTIONS:
            continue
        m = LINE_RE.match(line)
        if not m:
            continue
        size = int(m.group(2), 16)
        addr = int(m.group(3), 16)
        name = m.group(4)
        if name.startswith(".") or addr == 0:
            continue
        addr2.setdefault(addr, (name, size))
        name2[name].add(addr)
    return addr2, name2


class TypeStats:
    """Per-correlator, per-size-bucket precision accumulator."""
    __slots__ = ("n", "resolved", "exact", "base",
                 "u_n", "u_resolved", "u_exact", "u_base")

    def __init__(self):
        z = lambda: [0, 0, 0, 0, 0]  # 4 buckets + unknown-size
        self.n = 0          # all pairs proposed by this correlator
        self.resolved = z()  # both sides have map symbols, by b8-size bucket
        self.exact = z()
        self.base = z()      # exact OR base-identifier match
        self.u_n = 0        # unique (1:1 within this correlator) subset
        self.u_resolved = z()
        self.u_exact = z()
        self.u_base = z()


def stream_matches(path: Path, b5_map, b8_map, oracle_b5):
    """Single pass over address_matches. Returns:
    (records arrays, type_names, per-type degree dicts, oracle hit/prop masks).
    """
    import ijson

    type_bit = {}
    type_names = []

    def bit_of(t):
        b = type_bit.get(t)
        if b is None:
            b = len(type_names)
            type_bit[t] = b
            type_names.append(t)
        return b

    b5a = array("I")
    b8a = array("I")
    maska = array("H")
    fdeg_t = defaultdict(lambda: defaultdict(int))  # type -> b5 -> out-degree
    rdeg_t = defaultdict(lambda: defaultdict(int))  # type -> b8 -> in-degree
    hit_mask = {}    # oracle b5 addr -> mask of types that proposed CORRECT pair
    prop_mask = {}   # oracle b5 addr -> mask of types that proposed ANY pair
    n_pairs = 0
    t0 = time.time()
    with path.open("rb") as f:
        for b5_hex, cands in ijson.kvitems(f, "address_matches"):
            try:
                b5 = int(b5_hex, 16)
            except ValueError:
                continue
            expected_b8 = oracle_b5.get(b5)
            for cand in cands:
                b8_hex, types = cand[0], cand[1]
                try:
                    b8 = int(b8_hex, 16)
                except (ValueError, TypeError):
                    continue
                mask = 0
                for t in types:
                    bi = bit_of(t)
                    mask |= (1 << bi)
                    fdeg_t[bi][b5] += 1
                    rdeg_t[bi][b8] += 1
                b5a.append(b5)
                b8a.append(b8)
                maska.append(mask)
                n_pairs += 1
                if expected_b8 is not None:
                    prop_mask[b5] = prop_mask.get(b5, 0) | mask
                    if b8 == expected_b8:
                        hit_mask[b5] = hit_mask.get(b5, 0) | mask
                if n_pairs % 2_000_000 == 0:
                    print(f"  ... {n_pairs} pairs ({time.time()-t0:.0f}s)",
                          file=sys.stderr)
    print(f"[matches] {n_pairs} match pairs in {time.time()-t0:.0f}s",
          file=sys.stderr)
    return b5a, b8a, maska, type_names, fdeg_t, rdeg_t, hit_mask, prop_mask


def stream_pdiff_modified(path: Path):
    """Stream functions.modified from the pdiff: accepted changed pairs with
    ratios, joined by ADDRESS (collision-free, unlike divergence_index)."""
    import ijson

    out = []
    t0 = time.time()
    with path.open("rb") as f:
        for e in ijson.items(f, "functions.modified.item", use_float=True):
            new = e.get("new") or {}
            old = e.get("old") or {}
            try:
                b8 = int(new.get("address"), 16)
                b5 = int(old.get("address"), 16)
            except (TypeError, ValueError):
                continue
            out.append({
                "b5": b5, "b8": b8,
                "match_types": e.get("match_types") or [],
                "m_ratio": e.get("m_ratio"),
                "i_ratio": e.get("i_ratio"),
                "b8_len": new.get("length"),
            })
    print(f"[pdiff] {len(out)} functions.modified in {time.time()-t0:.0f}s",
          file=sys.stderr)
    return out


def fmt_pct(num, den):
    return f"{num/den:6.1%}" if den else "   n/a"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--matches", type=Path, default=DEFAULT_MATCHES)
    ap.add_argument("--pdiff", type=Path, default=DEFAULT_PDIFF)
    ap.add_argument("--skip-pdiff", action="store_true",
                    help="skip the 3 GB pdiff stream (no implied-ratio sweep)")
    ap.add_argument("-o", "--out", type=Path, default=None,
                    help="also write the report to this file")
    args = ap.parse_args()

    lines = []

    def emit(s=""):
        lines.append(s)
        print(s)

    # ---- maps + oracle -------------------------------------------------
    b5_addr2, b5_name2 = parse_map(BANK5_MAP)
    b8_addr2, b8_name2 = parse_map(BANK8_MAP)
    common = set(b5_name2) & set(b8_name2)
    # strict oracle: name maps to exactly ONE code address in EACH map
    oracle_b5 = {}   # b5 addr -> expected b8 addr
    oracle_b8 = set()
    ambiguous = 0
    for name in common:
        a5, a8 = b5_name2[name], b8_name2[name]
        if len(a5) == 1 and len(a8) == 1:
            b5 = next(iter(a5))
            b8 = next(iter(a8))
            oracle_b5[b5] = b8
            oracle_b8.add(b8)
        else:
            ambiguous += 1
    emit("# ghidriff correlator calibration — Bank5 vs Bank8 name oracle")
    emit()
    emit(f"- Bank5 map code symbols: {len(b5_addr2)} "
         f"({len(b5_name2)} unique names)")
    emit(f"- Bank8 map code symbols: {len(b8_addr2)} "
         f"({len(b8_name2)} unique names)")
    all_common = len(all_section_names(BANK5_MAP) & all_section_names(BANK8_MAP))
    emit(f"- names common to both maps, ALL sections (incl. data): {all_common}")
    emit(f"- code-section names common to both maps: {len(common)} "
         f"(strict 1:1 oracle pairs: {len(oracle_b5)}, "
         f"ambiguous/dup names excluded: {ambiguous})")
    emit()

    # ---- stream the full match list ------------------------------------
    if not args.matches.exists():
        print(f"error: {args.matches} not found", file=sys.stderr)
        return 1
    (b5a, b8a, maska, type_names, fdeg_t, rdeg_t,
     hit_mask, prop_mask) = stream_matches(args.matches, b5_addr2, b8_addr2,
                                           oracle_b5)

    # ---- aggregate precision per correlator ----------------------------
    stats = defaultdict(TypeStats)
    n_types = len(type_names)
    for i in range(len(b5a)):
        b5, b8, mask = b5a[i], b8a[i], maska[i]
        s5 = b5_addr2.get(b5)
        s8 = b8_addr2.get(b8)
        if s8 is not None:
            bk = bucket_of(s8[1])
        else:
            bk = None
        bki = bk if bk is not None else 4
        resolved = s5 is not None and s8 is not None
        if resolved:
            ex = s5[0] == s8[0]
            ba = ex or base_match(s5[0], s8[0])
        for ti in range(n_types):
            if not (mask >> ti) & 1:
                continue
            st = stats[ti]
            st.n += 1
            uniq = fdeg_t[ti][b5] == 1 and rdeg_t[ti][b8] == 1
            if uniq:
                st.u_n += 1
            if not resolved:
                continue
            st.resolved[bki] += 1
            if ex:
                st.exact[bki] += 1
            if ba:
                st.base[bki] += 1
            if uniq:
                st.u_resolved[bki] += 1
                if ex:
                    st.u_exact[bki] += 1
                if ba:
                    st.u_base[bki] += 1

    order_idx = {t: i for i, t in enumerate(ORDER)}
    tis = sorted(range(n_types), key=lambda ti: order_idx.get(type_names[ti], 99))

    emit("## Per-correlator precision vs name oracle "
         "(full match list, incl. unchanged pairs)")
    emit()
    emit("Pairs counted once per correlator that proposed them. `resolved` = "
         "both addresses have CW-map code symbols. Exact = identical mangled "
         "symbol; base = exact OR same pre-`__` base identifier (distill's "
         "base_match). SymbolsHash matches BY NAME — shown for reference only, "
         "excluded from all precision claims.")
    emit()
    emit("| correlator | pairs | resolved | exact | base | uniq 1:1 pairs | uniq exact | uniq base |")
    emit("|---|---|---|---|---|---|---|---|")
    for ti in tis:
        st = stats[ti]
        name = type_names[ti]
        tag = " *(name-based)*" if name == "SymbolsHash" else ""
        r = sum(st.resolved)
        e = sum(st.exact)
        b = sum(st.base)
        ur = sum(st.u_resolved)
        ue = sum(st.u_exact)
        ub = sum(st.u_base)
        emit(f"| {name}{tag} | {st.n} | {r} | {fmt_pct(e, r)} | {fmt_pct(b, r)} "
             f"| {st.u_n} | {fmt_pct(ue, ur)} | {fmt_pct(ub, ur)} |")
    emit()

    emit("### Precision by Bank8 size bucket (exact-name, on resolved pairs)")
    emit()
    hdr = " | ".join(f"{b} exact (n)" for b in BUCKETS)
    emit(f"| correlator | {hdr} | size-unknown n |")
    emit("|---|" + "---|" * 5)
    for ti in tis:
        st = stats[ti]
        name = type_names[ti]
        cells = []
        for bi in range(4):
            cells.append(f"{fmt_pct(st.exact[bi], st.resolved[bi])} "
                         f"({st.resolved[bi]})")
        emit(f"| {name} | " + " | ".join(cells) + f" | {st.resolved[4]} |")
    emit()
    emit("### Precision by size bucket (base-identifier, on resolved pairs)")
    emit()
    emit(f"| correlator | " + " | ".join(f"{b} base (n)" for b in BUCKETS) + " |")
    emit("|---|" + "---|" * 4)
    for ti in tis:
        st = stats[ti]
        cells = [f"{fmt_pct(st.base[bi], st.resolved[bi])} ({st.resolved[bi]})"
                 for bi in range(4)]
        emit(f"| {type_names[ti]} | " + " | ".join(cells) + " |")
    emit()

    # ---- validation: ExactBytes small-stub collisions -------------------
    emit("### Validation: ExactBytesFunctionHasher <16B collisions (sample)")
    emit()
    eb = next((ti for ti in range(n_types)
               if type_names[ti] == "ExactBytesFunctionHasher"), None)
    shown = 0
    if eb is not None:
        for i in range(len(b5a)):
            if not (maska[i] >> eb) & 1:
                continue
            s5 = b5_addr2.get(b5a[i])
            s8 = b8_addr2.get(b8a[i])
            if not s5 or not s8 or s8[1] >= 16 or s5[0] == s8[0]:
                continue
            emit(f"- `{s5[0]}` (b5) paired with `{s8[0]}` (b8, {s8[1]}B)")
            shown += 1
            if shown >= 8:
                break
    if not shown:
        emit("- NONE FOUND — join is suspect, do not trust this report!")
    emit()

    # ---- recall: which stage caught the oracle pairs --------------------
    emit("## Recall: which stage first proposed the correct pair")
    emit()
    sym_bit = next((ti for ti in range(n_types)
                    if type_names[ti] == "SymbolsHash"), None)
    stage_counter = Counter()
    caught = 0
    caught_by_name = 0
    wrong_only = 0
    unproposed = 0
    for b5, b8 in oracle_b5.items():
        hm = hit_mask.get(b5, 0)
        if hm:
            caught += 1
            # earliest stage in run order with a correct proposal
            best = min((order_idx.get(type_names[ti], 99), type_names[ti])
                       for ti in range(n_types) if (hm >> ti) & 1)
            stage_counter[best[1]] += 1
            if sym_bit is not None and hm == (1 << sym_bit):
                caught_by_name += 1
        elif prop_mask.get(b5, 0):
            wrong_only += 1
        else:
            unproposed += 1
    emit(f"- strict oracle pairs: {len(oracle_b5)}")
    emit(f"- correct pair proposed by >=1 correlator: {caught} "
         f"({caught/len(oracle_b5):.1%})  [of which ONLY SymbolsHash: "
         f"{caught_by_name}]")
    emit(f"- b5 side proposed but never to the correct b8: {wrong_only}")
    emit(f"- b5 side never appears in any match: {unproposed} "
         f"(includes functions Ghidra never defined, dead-stripped dupes, "
         f"and genuinely unmatched)")
    emit()
    emit("| first stage to catch | oracle pairs |")
    emit("|---|---|")
    for t in ORDER:
        if stage_counter.get(t):
            emit(f"| {t} | {stage_counter[t]} |")
    for t, c in stage_counter.items():
        if t not in ORDER:
            emit(f"| {t} | {c} |")
    emit()
    emit("NOTE: BulkBasicBlockMnemonicHash rows are PROPOSAL-only — its "
         "many-to-many output is recorded in address_matches but essentially "
         "never survives into ghidriff's accepted match set (it contributes "
         "0 accepted modified pairs below). 'Caught' here means the correct "
         "pair existed somewhere in its ~11M proposals.")
    emit()

    # ---- pdiff: accepted modified pairs + implied sweep ------------------
    imp_bit = next((ti for ti in range(n_types)
                    if type_names[ti] == "Implied Match"), None)
    imp_fdeg = fdeg_t.get(imp_bit, {}) if imp_bit is not None else {}
    imp_rdeg = rdeg_t.get(imp_bit, {}) if imp_bit is not None else {}
    if not args.skip_pdiff and args.pdiff.exists():
        mods = stream_pdiff_modified(args.pdiff)
        emit("## Accepted modified pairs (pdiff functions.modified, "
             "address-joined)")
        emit()
        per_t = defaultdict(lambda: [0, 0, 0])  # type -> [resolved, exact, base]
        implied = []
        for m in mods:
            s5 = b5_addr2.get(m["b5"])
            s8 = b8_addr2.get(m["b8"])
            ok = s5 is not None and s8 is not None
            ex = ok and s5[0] == s8[0]
            ba = ok and (ex or base_match(s5[0], s8[0]))
            for t in m["match_types"]:
                if ok:
                    per_t[t][0] += 1
                    per_t[t][1] += ex
                    per_t[t][2] += ba
            if "Implied Match" in m["match_types"] and ok:
                # 1:1 = this pair's endpoints each appear exactly once among
                # ALL Implied Match records (dedupes the one-to-many implieds)
                uniq = (imp_fdeg.get(m["b5"]) == 1
                        and imp_rdeg.get(m["b8"]) == 1)
                implied.append((m["m_ratio"], ex, ba, s8[1], uniq))
        emit("| correlator | resolved modified pairs | exact | base |")
        emit("|---|---|---|---|")
        for t in sorted(per_t, key=lambda t: order_idx.get(t, 99)):
            r, e, b = per_t[t]
            tag = " *(name-based)*" if t == "SymbolsHash" else ""
            emit(f"| {t}{tag} | {r} | {fmt_pct(e, r)} | {fmt_pct(b, r)} |")
        emit()
        emit("## Implied Match m_ratio threshold sweep (modified pairs)")
        emit()
        emit("`m_ratio` = mnemonic-similarity of the matched pair. Gate = keep "
             "only pairs with m_ratio >= threshold. (Unchanged implied pairs "
             "carry no ratios and are not in the pdiff; they are byte-identical "
             "and effectively exact.)")
        emit()
        emit("| gate | surviving | exact | base | exact, b8>=64B (n) "
             "| 1:1 surviving | 1:1 exact | 1:1 base |")
        emit("|---|---|---|---|---|---|---|---|")
        for gate in (0.0, 0.25, 0.5, 0.75, 0.9, 0.95):
            keep = [x for x in implied if (x[0] or 0.0) >= gate]
            n = len(keep)
            e = sum(1 for x in keep if x[1])
            b = sum(1 for x in keep if x[2])
            big = [x for x in keep if (x[3] or 0) >= 64]
            be = sum(1 for x in big if x[1])
            uk = [x for x in keep if x[4]]
            ue = sum(1 for x in uk if x[1])
            ub = sum(1 for x in uk if x[2])
            emit(f"| >={gate} | {n} | {fmt_pct(e, n)} | {fmt_pct(b, n)} "
                 f"| {fmt_pct(be, len(big))} ({len(big)}) "
                 f"| {len(uk)} | {fmt_pct(ue, len(uk))} "
                 f"| {fmt_pct(ub, len(uk))} |")
        emit()
    else:
        emit("## (pdiff stream skipped — no implied-ratio sweep)")
        emit()

    if args.out:
        args.out.write_text("\n".join(lines) + "\n")
        print(f"[report] wrote {args.out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
