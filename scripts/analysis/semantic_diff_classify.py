#!/usr/bin/env python3
"""
Decompiler-level semantic diff classifier for RB3 (Bank 8) decomp.

objdiff gives an *asm-exact* match% but never decompiles. This tool takes a
sub-100% function and decides whether the residual asm mismatch is:

  PERMUTER-CLASS  (verdict IDENTICAL) -- our build and the target are logically
                  the same; the asm differs only by regalloc / scheduling / CSE
                  / the string-pool base-pointer noise. Stop hand-reasoning; mark
                  the function at-limit.

  REAL SOURCE BUG (verdict DIFFERENT)  -- the decompiled control flow / operators
                  / call targets / struct offsets actually diverge. Worth fixing
                  in C++.

Method: m2c-decompile BOTH the target asm (build/SZBE69_B8/asm/<unit>.s) and our
build .o (disassembled with dtk to the same `.fn SYMBOL, global` format), extract
*only* the named function body from each, canonicalize away the systematic m2c
noise (see normalize_body), then diff. Zero residual => IDENTICAL.

Usage:
  python3 scripts/analysis/semantic_diff_classify.py \
      --symbol UpdateScrolling__10VocalTrackFf \
      --unit main/band3/bandtrack/VocalTrack --json

Discrimination (deliberately mismatched pair: target=<--symbol>, build=<impostor>):
  python3 scripts/analysis/semantic_diff_classify.py \
      --symbol UpdateScrolling__10VocalTrackFf \
      --unit main/band3/bandtrack/VocalTrack \
      --impostor-symbol <SomeOtherSymbol> --json

cwd is expected to be the rb3 repo root.
"""

import argparse
import difflib
import json
import os
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent          # .../rb3


def _find_m2c() -> Path:
    """Locate m2c.py robustly. Honour $RB3_M2C, else walk up from ROOT looking
    for a sibling `m2c/m2c.py` (so it works from a git worktree under
    .claude/worktrees/<name>, whose ROOT.parent is NOT the milohax dir)."""
    env = os.environ.get("RB3_M2C")
    if env and Path(env).exists():
        return Path(env)
    for anc in [ROOT, *ROOT.parents]:
        cand = anc.parent / "m2c" / "m2c.py"
        if cand.exists():
            return cand
        cand = anc / "m2c" / "m2c.py"
        if cand.exists():
            return cand
    return ROOT.parent / "m2c" / "m2c.py"                      # best-effort default


M2C = _find_m2c()
DTK = ROOT / "build" / "tools" / "dtk"
# objdiff.json semantics: obj/ = TARGET objects (split from the original binary),
# src/ = BASE objects (compiled from OUR decompiled source). The classifier diffs
# our BUILD (src/) against the TARGET (obj/) -- NOT the target against itself.
TARGET_OBJ_DIR = ROOT / "build" / "SZBE69_B8" / "obj"          # objdiff target_path
BUILD_OBJ_DIR  = ROOT / "build" / "SZBE69_B8" / "src"          # objdiff base_path (our build)
ASM_DIR = ROOT / "build" / "SZBE69_B8" / "asm"                 # target disasm (TU lookup only)
TMP_ROOT = Path("/tmp/claude")


# ---------------------------------------------------------------------------
# Symbol demangling (just enough to recognise the function's definition line)
# ---------------------------------------------------------------------------

def unmangled_method_name(symbol: str) -> str:
    """
    Best-effort extraction of the bare method/function name from a MWCC mangled
    symbol, used only to spot the m2c definition line.

      UpdateScrolling__10VocalTrackFf       -> UpdateScrolling
      Execute__9DataArrayFv                 -> Execute
      __dt__5MyObjFv                        -> __dt
      Find<8RndGroup>__9ObjectDirFPCcb...   -> Find  (template suffix kept off)

    m2c prints the *mangled* symbol verbatim as the C function name, so the
    definition line always contains the full mangled symbol followed by '('.
    We therefore match on the mangled symbol directly; this helper is only a
    fallback / readability aid.
    """
    # Operator / special members keep their leading-underscore form.
    head = symbol.split("__", 1)[0]
    # Template args (rare in the name head) -- strip for the readable form.
    head = re.sub(r"<.*", "", head)
    return head or symbol


# ---------------------------------------------------------------------------
# Path resolution
# ---------------------------------------------------------------------------

def resolve_target_asm(unit: str) -> Path:
    """main/band3/bandtrack/VocalTrack -> build/.../asm/band3/bandtrack/VocalTrack.s"""
    u = unit.removeprefix("main/")
    cand = ASM_DIR / (u + ".s")
    if cand.exists():
        return cand
    for prefix in ("band3/", "system/", "network/", "lib/", "sdk/"):
        cand = ASM_DIR / prefix / (u + ".s")
        if cand.exists():
            return cand
    raise FileNotFoundError(f"target asm not found for unit '{unit}' (tried {ASM_DIR / (u + '.s')})")


def _resolve_o(base_dir: Path, unit: str, what: str) -> Path:
    u = unit.removeprefix("main/")
    cand = base_dir / (u + ".o")
    if cand.exists():
        return cand
    for prefix in ("band3/", "system/", "network/", "lib/", "sdk/"):
        cand = base_dir / prefix / (u + ".o")
        if cand.exists():
            return cand
    raise FileNotFoundError(f"{what} .o not found for unit '{unit}' (tried {base_dir / (u + '.o')})")


def resolve_target_obj(unit: str) -> Path:
    """obj/ = the TARGET object (split from the original binary)."""
    return _resolve_o(TARGET_OBJ_DIR, unit, "target")


def resolve_build_obj(unit: str) -> Path:
    """src/ = the BASE object compiled from OUR decompiled source (our build)."""
    return _resolve_o(BUILD_OBJ_DIR, unit, "build")


def unit_for_symbol(symbol: str) -> str | None:
    """
    Locate which asm TU defines `symbol` (used for --impostor-symbol when its
    unit isn't supplied). Returns a 'band3/.../TU' style unit (no .s).
    """
    needle = f".fn {symbol},"
    try:
        r = subprocess.run(
            ["grep", "-rl", "--include=*.s", needle, str(ASM_DIR)],
            capture_output=True, text=True,
        )
        if r.returncode == 0 and r.stdout.strip():
            p = Path(r.stdout.strip().splitlines()[0])
            return str(p.relative_to(ASM_DIR).with_suffix(""))
    except FileNotFoundError:
        pass
    return None


# ---------------------------------------------------------------------------
# Disasm + m2c subprocess drivers
# ---------------------------------------------------------------------------

def dtk_disasm(obj: Path, out_s: Path) -> None:
    r = subprocess.run(
        [str(DTK), "elf", "disasm", str(obj), str(out_s)],
        capture_output=True, text=True,
    )
    if r.returncode != 0:
        raise RuntimeError(f"dtk disasm failed for {obj}:\n{r.stderr.strip()}")


def run_m2c(symbol: str, asm: Path) -> str:
    cmd = [
        sys.executable, str(M2C),
        "--target", "ppc",
        "-f", symbol,
        "--passes", "4",
        "--deterministic-vars",
        str(asm),
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"m2c failed for {symbol} on {asm}:\n{r.stderr.strip()[:2000]}")
    if not r.stdout.strip():
        raise RuntimeError(f"m2c produced no output for {symbol} on {asm}")
    return r.stdout


# ---------------------------------------------------------------------------
# Function-body extraction (brace balance from the definition line)
# ---------------------------------------------------------------------------

def extract_body(m2c_output: str, symbol: str) -> list[str]:
    """
    Return the lines of the single function whose definition line contains the
    mangled `symbol` immediately followed by '(' and which opens a '{' (the
    actual definition, not a forward-decl ending in ';').
    Raises if not found.
    """
    lines = m2c_output.splitlines()
    sym_call = symbol + "("
    start = None
    for i, line in enumerate(lines):
        if sym_call not in line:
            continue
        # forward-decl / prototype lines end in ';' (often '... ); /* ... */')
        # the real definition opens a body brace somewhere from here on.
        stripped = line.rstrip()
        if stripped.endswith(";"):
            continue
        # Confirm a body brace opens at/after this line before the next def.
        if "{" in line or _opens_brace_soon(lines, i):
            start = i
            break
    if start is None:
        raise RuntimeError(f"definition for {symbol} not found in m2c output")

    # Brace-balance from `start` to the closing brace.
    body = []
    depth = 0
    seen_open = False
    for j in range(start, len(lines)):
        line = lines[j]
        body.append(line)
        for ch in line:
            if ch == "{":
                depth += 1
                seen_open = True
            elif ch == "}":
                depth -= 1
        if seen_open and depth == 0:
            return body
    raise RuntimeError(f"unbalanced braces extracting {symbol}")


def _opens_brace_soon(lines: list[str], i: int, lookahead: int = 3) -> bool:
    for k in range(i, min(i + lookahead + 1, len(lines))):
        if "{" in lines[k]:
            return True
        if lines[k].rstrip().endswith(";"):
            return False
    return False


# ===========================================================================
# THE NORMALIZER  --  keep this clearly separated; it's the part you tune.
# ===========================================================================
#
# Canonicalizes the SYSTEMATIC, build-vs-target-only m2c noise so that two
# logically-identical functions diff to zero. Anything that survives is real
# semantic signal (control flow, operators, call targets, struct offsets).
#
# Noise classes neutralised (see the task's NORMALIZER CONTRACT):
#
#   1. String / global *base pointers*. The same rodata offset shows up as,
#      depending on which side and how m2c resolved the pool:
#         target:  temp_rN_xxxx + 0xOFF            (carrier temp = "big literal")
#                  "big literal\0...\0" + 0xOFF    (inlined pool literal)
#         build:   &@stringBase0 + 0xOFF
#                  @<hexaddr> + 0xOFF  /  &@<hexaddr> + 0xOFF  /  @<hexaddr>.field
#      All collapse to a single token keyed ONLY by the offset:  DATA(0xOFF).
#      The carrier register/symbol name must not affect the result.
#
#      CRUCIAL: a string-base carrier is detected structurally -- a temp whose
#      RHS is a string literal or &@stringBase0 -- so a *genuine* pointer field
#      access like `temp_r3 + 0x10` (carrier = a function-call result) is left
#      alone and still counts as real signal.
#
#   2. m2c temp/var/phi names: temp_rN_yyyy / var_rN_yyyy / phi_rN ->
#      positional placeholders (strip the numeric suffix). --deterministic-vars
#      already aligns most of these; we strip the residual hash suffix.
#
#   3. @LOCAL@<fn>@name  ->  LOCAL(name).
#
#   4. forward-decl noise: `? *temp_rN_yyyy;`, pure-whitespace lines -> dropped.
# ===========================================================================

# --- raw token regexes ----------------------------------------------------
_RE_TEMP_DECL      = re.compile(r"^\s*\?\s*\*?(?:temp|var|phi)_r\d+(?:_[0-9a-f]+)?\s*;\s*$")
_RE_STRINGBASE_OFF = re.compile(r"&?@stringBase\d*\s*\+\s*0x[0-9A-Fa-f]+")
_RE_STRINGBASE_BARE= re.compile(r"&?@stringBase\d*")
# Anonymous absolute-address data symbols m2c invents, e.g. `@7120`. These are
# the build-side rendering of a global object the target side carried in a temp
# (or vice-versa). Forms, all of which canonicalise:
#     &@7120 + 0x6C   ->  DATA(0x6c)        (address-of a field)
#     @7120.unk6C     ->  DATA(base)->unk6C (a field read)
#     @7120           ->  DATA(base)
# IMPORTANT: do NOT match the *typed* const-pool symbols @F_xxxx (float),
# @D_xxxx (double), @R_xxxx (raw) -- those are bit-pattern keyed and SYMMETRIC
# across the two sides, so they are real signal and must be left untouched.
_RE_HEXADDR_FIELD  = re.compile(r"&?@(?![FDR]_)[0-9A-Fa-f]{4,8}\.([A-Za-z0-9_]+)")
_RE_HEXADDR_OFF    = re.compile(r"&?@(?![FDR]_)[0-9A-Fa-f]{4,8}\s*\+\s*0x([0-9A-Fa-f]+)")
_RE_HEXADDR_BARE   = re.compile(r"&?@(?![FDR]_)[0-9A-Fa-f]{4,8}\b")
# inlined rodata-pool string literal followed by an offset add: "...." + 0xOFF
_RE_LITERAL_OFF    = re.compile(r'"(?:[^"\\]|\\.)*"\s*\+\s*0x([0-9A-Fa-f]+)')
# bare string literal (assert filename / message). The same content shows up on
# the build side as a named @STRING@<sym>[@N] symbol -> both -> STR.
_RE_STR_LITERAL    = re.compile(r'"(?:[^"\\]|\\.)*"')
# @STRING@<symbol>[@digits] used as STRING DATA (NOT immediately a call -- guard
# on a following '('). Matches both `@STRING@Foo@0` and bare `@STRING@Foo`.
# Template-arg-named symbols like
# `@STRING@__rf__36ObjOwnerPtr<10CharLookAt,9ObjectDir>CFv@0` contain commas that
# MUST fold into the symbol -- but an arg-separator comma (`@STRING@Foo, 0x1EB`)
# must NOT. The discriminator: a template comma is followed by a non-space
# (`,9ObjectDir`) while m2c's arg separator is comma+SPACE (`, 0x1EB`). So allow a
# comma only via `(?:,[name]+)*` (comma immediately followed by name chars); a
# `, ` separator can't match that and is preserved. '$' is allowed for mangled
# template instantiations.
_RE_STRING_SYM     = re.compile(
    r"@STRING@[A-Za-z0-9_<>$]+(?:,[A-Za-z0-9_<>$]+)*(?:@\d+)?(?!\s*\()")
# m2c's generated assert-name string static: `__FUNCTION__$22811` ( s8[] = the
# function name). The target side inlines the same literal; both -> STR.
_RE_FUNCTION_SYM   = re.compile(r"__FUNCTION__(?:\$\d+)?")
_RE_LOCAL          = re.compile(r"@LOCAL@[A-Za-z0-9_<>,$]+?@([A-Za-z0-9_@]+)")
# redundant parens directly wrapping a DATA token: target renders `(temp + 0xb)`
# -> `(DATA(0xb))` while build renders `&@stringBase0 + 0xb` -> `DATA(0xb)`.
_RE_DATA_PAREN     = re.compile(r"\((DATA\((?:base|0x[0-9a-f]+)\))\)")
# TARGET-ONLY forward-decl of a string-base carrier that survived as DATA(base):
# `? (*DATA(base))(...);`, `s8 *DATA(base);`, `? *DATA(base);`. Build resolves
# these inline (no decl). A real *use* never ENDS in `DATA(base)[(...)];`, and the
# non-greedy `[^=;]` prefix can't cross a statement boundary, so no statement is
# ever dropped.
_RE_DATABASE_DECL  = re.compile(r"^[^=;]*?\(?\*?DATA\(base\)\)?\s*(?:\([^;]*\))?\s*;\s*$")
# a bare string-pool base used as a VALUE (no offset, not a struct deref) is just
# a string pointer == an inlined @STRING@/literal (STR). Struct derefs
# (`DATA(base)->fld`) are preserved by the negative lookahead.
_RE_DATABASE_VAL   = re.compile(r"\bDATA\(base\)(?!\s*(?:->|\.))")
_RE_TEMP_SUFFIX    = re.compile(r"\b((?:temp|var)_[a-z]+\d+)_[0-9a-f]+\b")
_RE_PHI            = re.compile(r"\bphi_r(\d+)\b")
# typed const-pool symbols. Under the CORRECT build-vs-target comparison these are
# NOT symmetric: the target object names a float via @F_<le-hex-bits> while our
# build may inline it as a literal (`-1.0f`). Resolve BOTH the symbol and bare
# float/double literals to a single value-keyed token so the *rendering* cancels
# but the *value* still differs for a real constant change.
_RE_FLOAT_SYM      = re.compile(r"@F_([0-9A-Fa-f]{8})\b")
_RE_DOUBLE_SYM     = re.compile(r"@D_([0-9A-Fa-f]{16})\b")
_RE_FLOAT_LIT      = re.compile(r"(?<![\w.])[-+]?\d+\.\d+(?:e[-+]?\d+)?f?\b")
# m2c local-variable forward-declarations (after temp-suffix stripping): stack
# locals `spXX` and temps/vars/phi. Pure decls (no '=', terminated by ';'); they
# differ by frame layout / regalloc, not logic, so drop them. Also drop the
# function self-decl and standalone /* ... */ comment lines.
_RE_LOCAL_DECL     = re.compile(
    r"^\s*(?:\?|[A-Za-z_][\w:<>,\s\*]*?)\*?\s*"
    r"(?:sp[0-9A-Fa-f]+|temp_[a-z]+\d+|var_[a-z]+\d+|phi_[a-z]+\d+)\s*;"
    r"\s*(?:/\*.*\*/\s*)?$")
_RE_COMMENT_ONLY   = re.compile(r"^\s*/\*.*\*/\s*$")
_RE_SELF_DECL      = re.compile(r"^\s*[A-Za-z_][\w:<>,\s\*]*\s\**\w+__\w*\(.*\);\s*(?:/\*.*\*/)?\s*$")


def _fconst(value: float) -> str:
    return f"FCONST({value:.6g})"
_RE_OFF_KEY        = re.compile(r"0x([0-9A-Fa-f]+)")


def _hex_off(s: str) -> str:
    m = _RE_OFF_KEY.search(s)
    return m.group(1).lower() if m else "0"


_RE_ASSIGN = re.compile(r"\b((?:temp|var)_r\d+_[0-9a-f]+)\s*=\s*(.+?);")


def find_string_base_carriers(body: list[str]) -> set[str]:
    """
    Identify temp/var carriers that hold a string-pool / global-data BASE
    pointer, so their `<carrier> + 0xOFF` and `<carrier>->field` uses fold to
    DATA(...). A carrier qualifies if its RHS is, directly or TRANSITIVELY:

        temp_r3_8200 = "popping unbaked plate\0..."   -> string literal base
        temp_r7_8489 = &@stringBase0                  -> stringBase
        temp_r16_9032 = temp_r9_8910 + 0x866          -> DERIVED from a carrier
        temp_r16_9032 = &@stringBase0 + 0x866         -> stringBase + off
        temp_r20_8027 = "...List_node..."             -> string-typed global ptr
        temp_r3_8937  = GetVocalNoteList(...)          -> NOT a base (real ptr)

    Derivation is resolved to a fixpoint so chained pointers all collapse.
    Returns the set of *suffixed* carrier names (e.g. {"temp_r3_8200", ...}).
    """
    # Collect every (name, rhs_head) assignment once.
    assigns: list[tuple[str, str]] = []
    for line in body:
        m = _RE_ASSIGN.search(line)
        if m:
            assigns.append((m.group(1), m.group(2).strip().lstrip("(").lstrip()))

    carriers: set[str] = set()

    def rhs_is_base(rhs: str, known: set[str]) -> bool:
        if rhs.startswith('"'):
            return True
        if _RE_STRINGBASE_BARE.match(rhs):
            return True
        if _RE_HEXADDR_BARE.match(rhs) or _RE_HEXADDR_OFF.match(rhs):
            return True
        if _RE_STRING_SYM.match(rhs):
            return True
        # derived: <known-carrier> + 0xOFF
        m = re.match(r"&?((?:temp|var)_r\d+_[0-9a-f]+)\s*\+\s*0x[0-9A-Fa-f]+", rhs)
        if m and m.group(1) in known:
            return True
        return False

    # Fixpoint: keep adding carriers until stable (handles a -> b -> c chains).
    changed = True
    while changed:
        changed = False
        for name, rhs in assigns:
            if name not in carriers and rhs_is_base(rhs, carriers):
                carriers.add(name)
                changed = True
    return carriers


def normalize_body(body: list[str]) -> list[str]:
    """
    Apply the normalizer contract to a function body (list of source lines) and
    return the canonicalized, noise-free list of non-empty lines.
    """
    carriers = find_string_base_carriers(body)
    # carrier + 0xOFF  -> DATA(off);  bare carrier (a base load) -> DATA(base)
    carrier_off = [
        re.compile(rf"&?\b{re.escape(c)}\b\s*\+\s*0x([0-9A-Fa-f]+)") for c in carriers
    ]
    carrier_bare = [re.compile(rf"&?\b{re.escape(c)}\b") for c in carriers]

    out: list[str] = []
    for raw in body:
        line = raw

        # (4) drop `? *temp_rN;` forward-decls outright.
        if _RE_TEMP_DECL.match(line):
            continue

        # (1) string-base carrier uses -> DATA(off). Do offset form first so the
        #     bare-carrier rule can't swallow the "+ 0xOFF" tail.
        for rx in carrier_off:
            line = rx.sub(lambda m: f"DATA(0x{m.group(1).lower()})", line)

        # (1) drop the carrier *assignment* line entirely (it's pure noise: the
        #     RHS is a giant literal / stringBase, and all its uses are now DATA).
        #     Detect by: a carrier appears on the LHS of '='.
        if any(re.match(rf"\s*{re.escape(c)}\s*=", line) for c in carriers):
            continue

        # (1) any remaining bare carrier reference -> DATA(base)
        for rx in carrier_bare:
            line = rx.sub("DATA(base)", line)

        # (1) inlined rodata pool literal "...." + 0xOFF -> DATA(off)
        line = _RE_LITERAL_OFF.sub(lambda m: f"DATA(0x{m.group(1).lower()})", line)

        # (1) &@stringBase0 + 0xOFF -> DATA(off); bare &@stringBase0 -> DATA(base)
        line = _RE_STRINGBASE_OFF.sub(lambda m: f"DATA(0x{_hex_off(m.group(0))})", line)
        line = _RE_STRINGBASE_BARE.sub("DATA(base)", line)

        # (1) anonymous absolute-address data symbol forms. Order matters: field
        #     and offset forms first, then the bare symbol.
        #       @7120.unk6C   -> DATA(base)->unk6C   (mirror target's carrier->fld)
        #       &@7120 + 0x6C -> DATA(0x6c)          (mirror target's carrier+off)
        #       @7120         -> DATA(base)
        line = _RE_HEXADDR_FIELD.sub(lambda m: f"DATA(base)->{m.group(1)}", line)
        line = _RE_HEXADDR_OFF.sub(lambda m: f"DATA(0x{m.group(1).lower()})", line)
        line = _RE_HEXADDR_BARE.sub("DATA(base)", line)

        # (1b) strip redundant parens directly wrapping a DATA token.
        line = _RE_DATA_PAREN.sub(r"\1", line)

        # (4) drop target-only forward-decls of a DATA(base) carrier (build inlines
        #     them, so they are pure target-side noise).
        if _RE_DATABASE_DECL.match(line):
            continue

        # (1) string CONTENT canonicalisation. The same assert filename / message
        #     appears inline ("VocalTrack.cpp") on one side and as a named symbol
        #     (@STRING@NewGem__...@0) on the other -- both -> STR. The @STRING@
        #     regex deliberately skips call targets (followed by '(').
        line = _RE_STRING_SYM.sub("STR", line)
        line = _RE_FUNCTION_SYM.sub("STR", line)
        line = _RE_STR_LITERAL.sub("STR", line)

        # (1c) a bare string-pool base used as a value == a string pointer (STR),
        #      to reconcile target's carried base (DATA(base)) with build's inlined
        #      @STRING@/literal (already STR above).
        line = _RE_DATABASE_VAL.sub("STR", line)

        # (3) @LOCAL@<fn>@name -> LOCAL(name)
        line = _RE_LOCAL.sub(lambda m: f"LOCAL({m.group(1)})", line)

        # (2) temp_rN_yyyy / var_rN_yyyy -> temp_rN / var_rN ; phi_rN -> phi_rN
        line = _RE_TEMP_SUFFIX.sub(lambda m: m.group(1), line)
        line = _RE_PHI.sub(lambda m: f"phi_r{m.group(1)}", line)

        # const-pool float/double symbols + bare float literals -> value token, so
        # `@F_<bits>` (target) and `-1.0f` (build) reconcile, but a real constant
        # change still differs.
        line = _RE_FLOAT_SYM.sub(
            lambda m: _fconst(struct.unpack("<f", bytes.fromhex(m.group(1)))[0]), line)
        line = _RE_DOUBLE_SYM.sub(
            lambda m: _fconst(struct.unpack("<d", bytes.fromhex(m.group(1)))[0]), line)
        line = _RE_FLOAT_LIT.sub(lambda m: _fconst(float(m.group(0).rstrip("f"))), line)

        # drop local-variable forward-decls / function self-decl / comment-only
        # lines -- frame-layout & regalloc noise, never logic.
        if _RE_LOCAL_DECL.match(line) or _RE_SELF_DECL.match(line) or _RE_COMMENT_ONLY.match(line):
            continue

        # (4) drop pure-whitespace lines.
        if line.strip() == "":
            continue

        # Canonicalise indentation/trailing whitespace so layout never shows up
        # as a residual diff -- only token content matters.
        out.append(line.strip())

    return out


# ===========================================================================
# Diff + verdict
# ===========================================================================

def diff_lines(a: list[str], b: list[str]) -> list[str]:
    """Unified-ish residual: only the +/- lines (changed content)."""
    res = []
    for d in difflib.unified_diff(a, b, lineterm="", n=0):
        if d.startswith("+++") or d.startswith("---") or d.startswith("@@"):
            continue
        if d.startswith("+") or d.startswith("-"):
            res.append(d)
    return res


def classify(symbol: str, unit: str, impostor_symbol: str | None) -> dict:
    result = {
        "symbol": symbol,
        "unit": unit,
        "verdict": None,
        "raw_diff_lines": None,
        "normalized_diff_lines": None,
        "sample_residual": [],
        "error": None,
    }
    if impostor_symbol:
        result["impostor_symbol"] = impostor_symbol

    workdir: Path | None = None
    try:
        TMP_ROOT.mkdir(parents=True, exist_ok=True)
        workdir = Path(tempfile.mkdtemp(prefix="semdiff_", dir=str(TMP_ROOT)))

        # 1. TARGET side: obj/ object (split from the original binary) -> disasm.
        target_obj = resolve_target_obj(unit)
        target_asm = workdir / "target.s"
        dtk_disasm(target_obj, target_asm)

        # 2. BUILD side: src/ object (compiled from OUR source) -> disasm. Both
        #    sides go through the SAME dtk path so rodata rendering stays
        #    symmetric. With --impostor-symbol the build side is a DIFFERENT
        #    function's object (a deliberately-mismatched negative control).
        build_symbol = symbol
        if impostor_symbol:
            imp_unit = unit_for_symbol(impostor_symbol)
            if imp_unit is None:
                raise RuntimeError(f"impostor symbol {impostor_symbol} not found in any asm TU")
            build_obj = resolve_build_obj(imp_unit)
            build_symbol = impostor_symbol
        else:
            build_obj = resolve_build_obj(unit)
        build_asm = workdir / "build.s"
        dtk_disasm(build_obj, build_asm)

        # 3. m2c both sides
        target_out = run_m2c(symbol, target_asm)
        build_out = run_m2c(build_symbol, build_asm)

        # 4. extract just the function body
        target_body = extract_body(target_out, symbol)
        build_body = extract_body(build_out, build_symbol)

        # raw (pre-normalization) residual, for visibility
        raw = diff_lines(
            [l.strip() for l in target_body if l.strip()],
            [l.strip() for l in build_body if l.strip()],
        )
        result["raw_diff_lines"] = len(raw)

        # 5. normalize
        nt = normalize_body(target_body)
        nb = normalize_body(build_body)

        # 6. diff + verdict
        norm = diff_lines(nt, nb)
        result["normalized_diff_lines"] = len(norm)
        result["sample_residual"] = norm[:12]
        result["verdict"] = "IDENTICAL" if len(norm) == 0 else "DIFFERENT"

    except Exception as e:  # noqa: BLE001 -- surface any failure as ERROR verdict
        result["verdict"] = "ERROR"
        result["error"] = f"{type(e).__name__}: {e}"
    finally:
        if workdir is not None:
            import shutil
            shutil.rmtree(workdir, ignore_errors=True)

    return result


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--symbol", required=True, help="mangled TARGET symbol")
    ap.add_argument("--unit", required=True, help="unit path, e.g. main/band3/bandtrack/VocalTrack")
    ap.add_argument("--impostor-symbol", default=None,
                    help="use this symbol's BUILD body (deliberately mismatched pair)")
    ap.add_argument("--json", action="store_true", help="emit JSON (default human-readable)")
    args = ap.parse_args()

    res = classify(args.symbol, args.unit, args.impostor_symbol)

    if args.json:
        print(json.dumps(res, indent=2))
    else:
        print(f"symbol : {res['symbol']}")
        print(f"unit   : {res['unit']}")
        if res.get("impostor_symbol"):
            print(f"impostor: {res['impostor_symbol']}")
        print(f"verdict: {res['verdict']}")
        print(f"raw diff lines       : {res['raw_diff_lines']}")
        print(f"normalized diff lines: {res['normalized_diff_lines']}")
        if res["error"]:
            print(f"error  : {res['error']}")
        if res["sample_residual"]:
            print("sample residual:")
            for l in res["sample_residual"]:
                print(f"  {l}")

    if res["verdict"] == "ERROR":
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
