#!/usr/bin/env python3
"""Build ghidriff --seed-matches input for the RB3 Wii Bank8 <-> RB3 Xbox360 diff.

Produces build/SZBE69_B8/ghidra/xenon-seeds/seeds.json: a JSON list of
{"p1_addr": "<hex>", "p2_addr": "<hex>"} pairs (the exact schema parsed by
ghidriff's GhidraDiffEngine.load_seed_matches on the rb3-improvements branch)
where we have HIGH confidence both addresses are the same source function.

  p1 = bank8_target.elf (RB3 Wii Bank 8, MWCC/Gekko). ADDRESS SPACE: the DOL
       virtual addresses, i.e. the CodeWarrior map's "Virtual address" column
       (0x80xxxxxx). The earlier ghidriff run's log prints "Image base address:
       0x00000000" for this ELF, but that is just the program image base —
       its PROGBITS sections carry absolute vaddrs (.text0 @ 0x80004000,
       .text1 @ 0x8000de20), and tools/ghidra/distill_ghidriff.py successfully
       joined the pdiff's function addresses directly against the map's
       virtual column (b8_addr like 0x80b09530). So map virtual addrs are
       exactly what ghidriff sees for p1.
  p2 = default.xex (RB3 Xbox 360, MSVC/Xenon, stripped). ADDRESS SPACE: the
       xex virtual addresses used everywhere in rb3-xenon
       (config/45410914/symbols.txt .text spans 0x82260000..0x82C148D4).

How identities are established (pure data joining, no Ghidra):

  Xenon side:  rb3-xenon/tools/bindiff_match.json maps Xenon addresses to
               DC3 MSVC-mangled names (BinDiff vs the symbolized DC3 360
               binary). We keep only similarity == 1.0 AND confidence >= 0.95
               entries, demangle the MSVC name with llvm-undname, and
               normalize to (scope, method, argcount, constness).
  Wii side:    orig/SZBE69_B8/files/band_r_wii.map (.text/.init function
               symbols, MWCC-mangled), demangled in one batch with
               tools/batch-demangle (Rust), normalized identically.
  Join key:    (scope-sans-template-arg-order, method, argcount, const).
               Template args are canonicalized to a sorted token multiset so
               the two demanglers' renderings ("char const *" vs
               "const char*", "class Foo" vs "Foo") agree; a canonicalization
               mismatch can only LOSE a seed, never fabricate one.
  Ambiguity:   if a key maps to more than one address on EITHER side
               (overloads collapsing to the same arity, multiple template
               instantiations, duplicated static fns), the key is DROPPED.
               1:1 uniqueness is the strongest precision gate.
  extern "C":  plain (unmangled) names join by exact string equality, with a
               libc/CRT blocklist (MSVC CRT memcpy and Wii MSL memcpy are the
               same NAME but not the same SOURCE) and a leading-underscore
               filter. zlib/flex/engine extern-"C" functions survive.
  Holdout:     rb3-xenon/docs/decomp/gameid/crossval_agree.json (the 146
               cross-validated BinDiff∩BSim Wii<->Xenon function identities,
               95% measured precision) is the evaluation holdout: any seed
               whose p2 address appears there is EXCLUDED from seeds.json and
               the whole holdout is copied to holdout.json. NOTE: that file
               carries only the Xenon address + TU stem (no Wii address), so
               exclusion is by p2 only.

Usage:
    python3 tools/ghidra/build_xenon_seeds.py            # default paths
    python3 tools/ghidra/build_xenon_seeds.py --spot 10  # print N spot checks
    python3 tools/ghidra/build_xenon_seeds.py --min-conf 0.95 --min-sim 1.0

Outputs (gitignored, under build/):
    build/SZBE69_B8/ghidra/xenon-seeds/seeds.json
    build/SZBE69_B8/ghidra/xenon-seeds/holdout.json
    build/SZBE69_B8/ghidra/xenon-seeds/stats.json
"""
from __future__ import annotations

import argparse
import json
import random
import re
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

RB3 = Path(__file__).resolve().parents[2]
XENON = RB3.parent / "rb3-xenon"

DEFAULT_BINDIFF = XENON / "tools" / "bindiff_match.json"
DEFAULT_HOLDOUT = XENON / "docs" / "decomp" / "gameid" / "crossval_agree.json"
DEFAULT_WII_MAP = RB3 / "orig" / "SZBE69_B8" / "files" / "band_r_wii.map"
DEFAULT_XENON_SYMS = XENON / "config" / "45410914" / "symbols.txt"
DEFAULT_BANK8_ELF = RB3 / "build" / "SZBE69_B8" / "ghidra" / "bank8_target.elf"
BATCH_DEMANGLE = RB3 / "tools" / "batch-demangle" / "target" / "release" / "rb3-batch-demangle"
DEFAULT_OUT_DIR = RB3 / "build" / "SZBE69_B8" / "ghidra" / "xenon-seeds"

# Fallback executable ranges if the artifacts are unavailable at run time.
# bank8_target.elf PROGBITS+X sections (readelf -S), xex .text from symbols.txt.
FALLBACK_WII_EXEC = [(0x80004000, 0x80006740), (0x8000DE20, 0x80B34820)]
FALLBACK_XEX_TEXT = (0x82260000, 0x82C148D4)

# extern-"C" names that exist on both sides but are NOT shared source
# (MSVC CRT / libm / intrinsics vs Wii MSL). Blocklist applies to the
# plain-name join only; mangled C++ joins are unaffected.
CRT_BLOCKLIST = {
    "main", "memcpy", "memmove", "memset", "memcmp", "memchr",
    "strcpy", "strncpy", "strcat", "strncat", "strcmp", "strncmp",
    "stricmp", "strnicmp", "strcasecmp", "strncasecmp",
    "strlen", "strchr", "strrchr", "strstr", "strtok", "strspn", "strcspn",
    "strpbrk", "strdup", "strtol", "strtoul", "strtod", "strtoll", "strtoull",
    "atoi", "atol", "atof", "abs", "labs", "div", "ldiv",
    "malloc", "calloc", "realloc", "free",
    "printf", "sprintf", "snprintf", "vsprintf", "vsnprintf", "fprintf",
    "sscanf", "fscanf", "scanf", "vprintf", "vfprintf",
    "fopen", "fclose", "fread", "fwrite", "fseek", "ftell", "fflush",
    "fgets", "fputs", "fgetc", "fputc", "ungetc", "setvbuf", "rewind",
    "qsort", "bsearch", "rand", "srand", "exit", "abort", "atexit",
    "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "sinh", "cosh",
    "tanh", "exp", "log", "log10", "pow", "sqrt", "ceil", "floor", "fmod",
    "frexp", "ldexp", "modf", "fabs", "hypot",
    "sinf", "cosf", "tanf", "asinf", "acosf", "atanf", "atan2f", "expf",
    "logf", "log10f", "powf", "sqrtf", "ceilf", "floorf", "fmodf", "fabsf",
    "isalpha", "isdigit", "isalnum", "isspace", "isupper", "islower",
    "toupper", "tolower", "isxdigit", "ispunct", "isprint", "iscntrl",
    "setjmp", "longjmp", "raise", "signal",
    "wcslen", "wcscpy", "wcsncpy", "wcscmp", "wcsncmp", "wcscat",
    "swprintf", "vswprintf", "towupper", "towlower",
    "time", "clock", "localtime", "gmtime", "mktime", "strftime",
}

OPERATOR_TOKENS = {
    "==", "!=", "<", "<=", ">", ">=", "=", "+", "-", "*", "/", "%",
    "^", "&", "|", "~", "!", "+=", "-=", "*=", "/=", "%=", "^=", "&=",
    "|=", "<<", ">>", "<<=", ">>=", "++", "--", ",", "->", "->*",
    "()", "[]",
}

NAMESPACE_ALIASES = {
    "stlpmtx_std": "std",  # DC3 360 STLport namespace vs Wii's plain std
}

# Pure namespace tokens dropped from template-arg multisets: llvm-undname
# renders STLport's private namespace as "stlpmtx_std::priv" (two tokens)
# while the MWCC demangler renders "stlp_priv" (one token), so keeping them
# fails the join for otherwise-identical instantiations (measured +37 1:1
# joins when dropped). They carry no discriminating power between
# instantiations of the same template.
TEMPLATE_NS_TOKENS = {"stlpmtx_std", "stlp_priv", "priv", "std"}


# ---------------------------------------------------------------------------
# Depth-aware string helpers ( <>, (), [] )
# ---------------------------------------------------------------------------
_OPEN = {"<": 1, "(": 1, "[": 1}
_CLOSE = {">": 1, ")": 1, "]": 1}


def _split_top(s: str, sep: str) -> List[str]:
    """Split on `sep` (1- or 2-char) at bracket depth 0."""
    out, depth, i, last = [], 0, 0, 0
    n = len(s)
    w = len(sep)
    while i < n:
        c = s[i]
        if c in _OPEN:
            # "->" / "->*" / "operator<" false positives don't matter for the
            # separators we use ("::" and " ").
            depth += 1
        elif c in _CLOSE:
            depth -= 1
        elif depth == 0 and s[i:i + w] == sep:
            out.append(s[last:i])
            last = i + w
            i += w
            continue
        i += 1
    out.append(s[last:])
    return out


def _count_top_args(args: str) -> int:
    a = args.strip()
    if a in ("", "void"):
        return 0
    return len(_split_top(a, ","))


def _canon_component(comp: str) -> str:
    """Canonicalize one scope component: keep the base identifier, reduce any
    template-arg string to a sorted token multiset so MSVC ("class Foo") and
    MWCC ("Foo") renderings, cv-qualifier order, and spacing all agree.
    A mismatch here can only fail a join (drop), never create a false one —
    and over-collapse is gated by the 1:1-uniqueness rule afterwards."""
    comp = comp.strip()
    lt = comp.find("<")
    if lt == -1:
        return NAMESPACE_ALIASES.get(comp, comp)
    base = comp[:lt].strip()
    base = NAMESPACE_ALIASES.get(base, base)
    targs = comp[lt:]
    targs = re.sub(r"\b(class|struct|enum|union)\b", " ", targs)
    toks = sorted(t for t in re.findall(r"\w+|\*|&", targs)
                  if t not in TEMPLATE_NS_TOKENS)
    return base + "<" + ",".join(toks) + ">"


def normalize_demangled(dem: str) -> Tuple[Optional[tuple], str]:
    """Demangled C++ signature -> normalized identity key.

    Returns (key, "ok") or (None, drop_cause). Accepts both llvm-undname
    output ("public: void __cdecl App::DrawRegular(void)") and
    rb3-batch-demangle/MWCC output ("App::DrawRegular(void)").
    key = (scope_tuple, method, argcount, is_const)
    """
    s = dem.strip()
    if s.startswith("[thunk]"):
        return None, "thunk"
    if "`" in s or "'" in s:
        return None, "compiler_special"  # `vector ctor iterator', anon ns, ...
    for p in ("public:", "protected:", "private:"):
        if s.startswith(p):
            s = s[len(p):].strip()
    changed = True
    while changed:
        changed = False
        for kw in ("static ", "virtual "):
            if s.startswith(kw):
                s = s[len(kw):]
                changed = True
    s = re.sub(r"__(cdecl|stdcall|fastcall|thiscall|ptr64)\s*", "", s).strip()

    # Locate the argument list: last ')' + its backward-matching '('.
    rp = s.rfind(")")
    if rp == -1:
        return None, "no_arg_list"  # data symbol / vtable / parse oddity
    depth, lp = 0, -1
    for i in range(rp, -1, -1):
        c = s[i]
        if c == ")":
            depth += 1
        elif c == "(":
            depth -= 1
            if depth == 0:
                lp = i
                break
    if lp <= 0:
        return None, "parse_fail"
    tail = s[rp + 1:]
    is_const = bool(re.search(r"\bconst\b", tail))
    argc = _count_top_args(s[lp + 1:rp])
    name_part = s[:lp].strip()

    # operator functions: find the rightmost top-level "operator" token.
    op_idx = -1
    depth = 0
    for i in range(len(name_part) - 7):
        c = name_part[i]
        if c in _OPEN:
            depth += 1
        elif c in _CLOSE:
            depth -= 1
        elif depth == 0 and name_part.startswith("operator", i):
            before_ok = i == 0 or name_part[i - 1] in ": "
            after = name_part[i + 8:i + 9]
            if before_ok and (after == "" or not (after.isalnum() or after == "_")):
                op_idx = i
    if op_idx >= 0:
        opname = name_part[op_idx + 8:].strip()
        if opname not in OPERATOR_TOKENS:
            # conversion operator / operator new / operator delete: type
            # rendering differs across demanglers -> not safely joinable.
            return None, "conversion_or_alloc_operator"
        scope_str = name_part[:op_idx].rstrip(":").strip()
        # strip any return type: scope is the last top-level space chunk
        scope_str = _split_top(scope_str, " ")[-1] if scope_str else ""
        method = "operator" + opname
        comps = [c for c in _split_top(scope_str, "::") if c] if scope_str else []
        scope = tuple(_canon_component(c) for c in comps)
        return (scope, method, argc, is_const), "ok"

    qualified = _split_top(name_part, " ")[-1]  # drop return type tokens
    comps = [c for c in _split_top(qualified, "::") if c]
    if not comps:
        return None, "parse_fail"
    method = comps[-1]
    scope_raw = comps[:-1]

    # strip method template args ("Method<...>", template-class ctor renders
    # as "Cls<T>::Cls<T>" under llvm-undname)
    mlt = method.find("<")
    method_base = method[:mlt] if mlt != -1 else method

    if not method_base:
        return None, "parse_fail"

    # ctor / dtor normalization (compare base names, template args stripped)
    if scope_raw:
        owner = scope_raw[-1]
        olt = owner.find("<")
        owner_base = owner[:olt] if olt != -1 else owner
        if method_base == owner_base:
            method_base = "#ctor"
        elif method_base == "~" + owner_base:
            method_base = "#dtor"
        elif method_base.startswith("~"):
            method_base = "#dtor"
    scope = tuple(_canon_component(c) for c in scope_raw)
    return (scope, method_base, argc, is_const), "ok"


# ---------------------------------------------------------------------------
# Wii side: CodeWarrior map -> {key: {addr}}, with mangled names kept
# ---------------------------------------------------------------------------
# `  <sect-off:8> <size:6> <virt:8> <fileoff:8> <align> <symbol> \t<lib/obj>`
_MAP_LINE = re.compile(
    r"^\s+([0-9a-f]{8})\s+([0-9a-f]{6})\s+([0-9a-f]{8})\s+[0-9a-f]{8}\s+\d+\s+(\S+)(?:\s*\t(.+))?$"
)


def parse_wii_map(map_path: Path) -> Dict[str, Set[int]]:
    """Mangled/plain symbol -> set of virtual addresses, .text/.init only."""
    out: Dict[str, Set[int]] = defaultdict(set)
    in_code = False
    with map_path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            stripped = line.rstrip("\n")
            if stripped.endswith("section layout"):
                in_code = stripped.startswith((".text", ".init"))
                continue
            if not in_code:
                continue
            m = _MAP_LINE.match(stripped)
            if not m:
                continue
            size = int(m.group(2), 16)
            if size == 0:
                continue
            sym = m.group(4)
            if sym[0] in "@*.":
                continue  # string literals, *fill*, per-object section lines
            out[sym].add(int(m.group(3), 16))
    return out


def demangle_wii_batch(symbols: List[str]) -> Dict[str, str]:
    """mangled -> demangled via tools/batch-demangle (one process)."""
    if not BATCH_DEMANGLE.exists():
        raise FileNotFoundError(
            f"{BATCH_DEMANGLE} missing — build with: cargo build --release "
            f"(in {BATCH_DEMANGLE.parents[2]})")
    out: Dict[str, str] = {}
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as tf:
        for i, sym in enumerate(symbols):
            tf.write(f"{sym} = .text:0x{0x10000000 + i * 4:08x}; "
                     f"// type:function size:0x4\n")
        tmp = tf.name
    try:
        r = subprocess.run([str(BATCH_DEMANGLE), tmp],
                           capture_output=True, text=True, check=True)
    finally:
        Path(tmp).unlink(missing_ok=True)
    for line in r.stdout.splitlines():
        parts = line.split("|||")
        if len(parts) >= 3:
            out[parts[1]] = parts[2]
    return out


# ---------------------------------------------------------------------------
# Xenon side: llvm-undname over the filtered bindiff names
# ---------------------------------------------------------------------------
def demangle_msvc_batch(names: List[str]) -> Dict[str, str]:
    """MSVC mangled -> demangled via llvm-undname (chunked argv).
    Output blocks: 'mangled\\n[demangled\\n]\\n' — a missing middle line means
    the demangle failed."""
    out: Dict[str, str] = {}
    CHUNK = 400
    for i in range(0, len(names), CHUNK):
        chunk = names[i:i + CHUNK]
        r = subprocess.run(["llvm-undname", *chunk], capture_output=True,
                           text=True, check=False)
        blocks = r.stdout.split("\n\n")
        for blk in blocks:
            lines = [ln for ln in blk.splitlines() if ln]
            if len(lines) >= 2:
                out[lines[0]] = lines[1]
    return out


# ---------------------------------------------------------------------------
# Range checks
# ---------------------------------------------------------------------------
def wii_exec_ranges(elf: Path) -> List[Tuple[int, int]]:
    """Executable PROGBITS ranges from bank8_target.elf via readelf -S."""
    try:
        r = subprocess.run(["readelf", "-S", "-W", str(elf)],
                           capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError):
        print(f"[warn] readelf on {elf} unavailable — using fallback ranges",
              file=sys.stderr)
        return FALLBACK_WII_EXEC
    ranges = []
    for m in re.finditer(
            r"\]\s+(\S+)\s+PROGBITS\s+([0-9a-f]{8})\s+[0-9a-f]+\s+([0-9a-f]+)"
            r"\s+\S+\s+([A-Z]+)", r.stdout):
        if "X" in m.group(4):
            start = int(m.group(2), 16)
            ranges.append((start, start + int(m.group(3), 16)))
    return ranges or FALLBACK_WII_EXEC


def xex_text_range(symbols_txt: Path) -> Tuple[int, int]:
    """Full .text range of the xex from rb3-xenon's symbols.txt."""
    lo, hi = None, None
    try:
        with symbols_txt.open("r", encoding="utf-8", errors="replace") as f:
            for line in f:
                m = re.search(
                    r"\.text:0x([0-9A-Fa-f]{8}).*?size:0x([0-9A-Fa-f]+)", line)
                if not m:
                    continue
                a, sz = int(m.group(1), 16), int(m.group(2), 16)
                lo = a if lo is None or a < lo else lo
                hi = a + sz if hi is None or a + sz > hi else hi
    except OSError:
        pass
    if lo is None:
        print(f"[warn] {symbols_txt} unreadable — using fallback xex range",
              file=sys.stderr)
        return FALLBACK_XEX_TEXT
    return lo, hi


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bindiff", type=Path, default=DEFAULT_BINDIFF)
    ap.add_argument("--holdout", type=Path, default=DEFAULT_HOLDOUT)
    ap.add_argument("--wii-map", type=Path, default=DEFAULT_WII_MAP)
    ap.add_argument("--xenon-symbols", type=Path, default=DEFAULT_XENON_SYMS)
    ap.add_argument("--bank8-elf", type=Path, default=DEFAULT_BANK8_ELF)
    ap.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    ap.add_argument("--min-sim", type=float, default=1.0)
    ap.add_argument("--min-conf", type=float, default=0.95)
    ap.add_argument("--spot", type=int, default=10,
                    help="print N random spot-check pairs (default 10)")
    ap.add_argument("--seed", type=int, default=20260609,
                    help="RNG seed for the spot-check sample")
    args = ap.parse_args()

    drops: Counter = Counter()
    stats: dict = {}

    # ---- Xenon side ------------------------------------------------------
    bindiff = json.loads(args.bindiff.read_text())
    stats["bindiff_total"] = len(bindiff)
    filt = [e for e in bindiff
            if e["similarity"] >= args.min_sim and e["confidence"] >= args.min_conf]
    stats["bindiff_filtered"] = len(filt)

    xen_plain: Dict[str, Set[int]] = defaultdict(set)   # C name -> addrs
    xen_mangled: List[Tuple[str, int]] = []             # (msvc, addr)
    for e in filt:
        addr = int(e["rb3_addr"], 16)
        name = e["dc3_name"]
        if not name.startswith("?"):
            if name.startswith("_") or name in CRT_BLOCKLIST:
                drops["xen_crt_blocklist"] += 1
            else:
                xen_plain[name].add(addr)
            continue
        if name.startswith("??_"):
            drops["xen_msvc_special"] += 1   # ??_G/??_E/??_H/vtables/...
            continue
        xen_mangled.append((name, addr))
    stats["xen_plain_names"] = len(xen_plain)
    stats["xen_mangled_in"] = len(xen_mangled)

    msvc_dem = demangle_msvc_batch(sorted({n for n, _ in xen_mangled}))
    xen_by_key: Dict[tuple, Set[int]] = defaultdict(set)
    xen_key_names: Dict[tuple, Set[str]] = defaultdict(set)
    for name, addr in xen_mangled:
        dem = msvc_dem.get(name)
        if dem is None:
            drops["xen_undemangleable"] += 1
            continue
        key, cause = normalize_demangled(dem)
        if key is None:
            drops["xen_" + cause] += 1
            continue
        xen_by_key[key].add(addr)
        xen_key_names[key].add(name)
    stats["xen_keys"] = len(xen_by_key)

    # ---- Wii side --------------------------------------------------------
    wii_syms = parse_wii_map(args.wii_map)
    stats["wii_map_symbols"] = len(wii_syms)
    wii_mangled = sorted(s for s in wii_syms if "__" in s)
    wii_dem = demangle_wii_batch(wii_mangled)

    wii_by_key: Dict[tuple, Set[int]] = defaultdict(set)
    wii_key_names: Dict[tuple, Set[str]] = defaultdict(set)
    wii_plain: Dict[str, Set[int]] = defaultdict(set)
    for sym, addrs in wii_syms.items():
        dem = wii_dem.get(sym)
        if dem is None or dem == sym:
            # not MWCC-demangleable: treat as extern-"C"/plain when it looks
            # like a C identifier (no MWCC '__' separator)
            if "__" not in sym and re.fullmatch(r"[A-Za-z_]\w*", sym):
                wii_plain[sym].update(addrs)
            continue
        key, cause = normalize_demangled(dem)
        if key is None:
            drops["wii_" + cause] += 1
            continue
        wii_by_key[key].update(addrs)
        wii_key_names[key].add(sym)
    stats["wii_keys"] = len(wii_by_key)
    stats["wii_plain_names"] = len(wii_plain)

    # ---- Join (1:1-unique on both sides) ---------------------------------
    pairs: List[dict] = []  # {"p1": int, "p2": int, "wii": str, "xen": str, key}
    joined_keys = set(xen_by_key) & set(wii_by_key)
    stats["join_keys_cpp"] = len(joined_keys)
    for key in joined_keys:
        xa, wa = xen_by_key[key], wii_by_key[key]
        if len(xa) > 1:
            drops["ambiguous_xenon_multi_addr"] += 1
            continue
        if len(wa) > 1:
            drops["ambiguous_wii_multi_addr"] += 1
            continue
        pairs.append({
            "p1": next(iter(wa)), "p2": next(iter(xa)),
            "wii": sorted(wii_key_names[key])[0],
            "xen": sorted(xen_key_names[key])[0],
            "key": key,
        })

    plain_joined = set(xen_plain) & set(wii_plain)
    stats["join_keys_plain"] = len(plain_joined)
    for name in plain_joined:
        xa, wa = xen_plain[name], wii_plain[name]
        if len(xa) > 1:
            drops["ambiguous_xenon_multi_addr"] += 1
            continue
        if len(wa) > 1:
            drops["ambiguous_wii_multi_addr"] += 1
            continue
        pairs.append({
            "p1": next(iter(wa)), "p2": next(iter(xa)),
            "wii": name, "xen": name, "key": (("#C",), name, None, False),
        })

    # ---- Holdout exclusion ------------------------------------------------
    holdout_entries: List[dict] = []
    holdout_p2: Set[int] = set()
    holdout_found = args.holdout.is_file()
    if holdout_found:
        hd = json.loads(args.holdout.read_text())
        holdout_entries = hd.get("agree_fns", [])
        holdout_p2 = {int(e["addr"], 16) for e in holdout_entries}
    else:
        print(f"[WARN] HOLDOUT FILE NOT FOUND: {args.holdout} — proceeding "
              f"without exclusion; subtract later!", file=sys.stderr)
    before = len(pairs)
    pairs = [p for p in pairs if p["p2"] not in holdout_p2]
    drops["excluded_eval_holdout"] = before - len(pairs)

    # ---- Range checks ------------------------------------------------------
    wranges = wii_exec_ranges(args.bank8_elf)
    xlo, xhi = xex_text_range(args.xenon_symbols)
    stats["wii_exec_ranges"] = [[hex(a), hex(b)] for a, b in wranges]
    stats["xex_text_range"] = [hex(xlo), hex(xhi)]
    ok_pairs = []
    for p in pairs:
        if not any(lo <= p["p1"] < hi for lo, hi in wranges):
            drops["p1_out_of_range"] += 1
            continue
        if not (xlo <= p["p2"] < xhi):
            drops["p2_out_of_range"] += 1
            continue
        ok_pairs.append(p)
    pairs = ok_pairs

    # invariants: each address used at most once
    assert len({p["p1"] for p in pairs}) == len(pairs), "duplicate p1 in seeds"
    assert len({p["p2"] for p in pairs}) == len(pairs), "duplicate p2 in seeds"
    pairs.sort(key=lambda p: p["p1"])
    stats["final_seeds"] = len(pairs)
    stats["drops"] = dict(drops.most_common())

    # ---- Outputs -----------------------------------------------------------
    out_dir = args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)
    seeds = [{"p1_addr": f"0x{p['p1']:08x}", "p2_addr": f"0x{p['p2']:08x}"}
             for p in pairs]
    (out_dir / "seeds.json").write_text(json.dumps(seeds, indent=1) + "\n")
    # provenance sidecar (NOT consumed by ghidriff; human/eval use)
    detail = [{"p1_addr": f"0x{p['p1']:08x}", "p2_addr": f"0x{p['p2']:08x}",
               "wii_symbol": p["wii"], "xenon_dc3_name": p["xen"],
               "key": [list(p["key"][0]), p["key"][1], p["key"][2], p["key"][3]]}
              for p in pairs]
    (out_dir / "seeds_detail.json").write_text(json.dumps(detail, indent=1) + "\n")
    if holdout_found:
        (out_dir / "holdout.json").write_text(json.dumps({
            "source": str(args.holdout),
            "note": "146 cross-validated BinDiff∩BSim Wii<->Xenon identities "
                    "(95% precision) — EVAL HOLDOUT, excluded from seeds.json "
                    "by p2 address. Carries Xenon addr + TU stem only; no "
                    "Wii-side address.",
            "entries": holdout_entries,
        }, indent=1) + "\n")
    (out_dir / "stats.json").write_text(json.dumps(stats, indent=1) + "\n")

    # ---- Report ------------------------------------------------------------
    print(json.dumps(stats, indent=1))
    if args.spot > 0 and pairs:
        rng = random.Random(args.seed)
        sample = rng.sample(pairs, min(args.spot, len(pairs)))
        print(f"\n--- spot check ({len(sample)} random seeds) ---")
        for p in sorted(sample, key=lambda q: q["p1"]):
            scope, meth, argc, cst = p["key"]
            norm = "::".join(list(scope) + [meth]) + \
                   (f"/{argc}" if argc is not None else "") + \
                   (" const" if cst else "")
            print(f"p1=0x{p['p1']:08x} p2=0x{p['p2']:08x}\n"
                  f"   wii: {p['wii']}\n   xen: {p['xen']}\n   norm: {norm}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
