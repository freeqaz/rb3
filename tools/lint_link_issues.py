"""Pre-flight link-error linter for RB3 source promotions.

Three static checks (no build required):

  1. NON_STATIC_DUP — file-scope globals (not `static`, not `extern`) defined
     by the same identifier in 2+ TUs. Catches the `sNullMicClientID`
     copy-paste pattern that link-errors on promotion.

  2. EXTERN_NO_DEF — `extern <T> <name>;` declared in a header but no .cpp
     defines it. (Diagnostic, not a strict fail.)

  3. ORPHAN_VIRTUAL — class declares `virtual <ret> <method>(...) [const];`
     in a header but:
       a) the target binary contains no override symbol for it (per
          orig/.../band_r_wii.map), AND
       b) no .cpp file in src/ implements `<Class>::<method>(`, AND
       c) it's not pure virtual (`= 0;`) or inline.
     Catches the redundant-override-decl pattern that link-errors at
     vtable emission.

  4. ORPHAN_NONVIRTUAL — the non-virtual sibling of (3): a derived class
     re-declares a NON-virtual member that a base class already
     declares+implements. C++ name-hiding makes unqualified calls resolve to
     the body-less derived decl -> undefined symbol. Conditions:
       a) the derived has no .cpp impl of the method, AND
       b) a transitive base DOES implement it (the name-hiding signature), AND
       c) the target binary lacks a derived-specific symbol but HAS the base's.
     Unlike (3) there's no vtable reference, so on native/web it resolves to a
     silent no-op stub (returns garbage) instead of link-erroring. Reference
     bug: BandCharacter::NameToDrumVenue (rb3 727cf01e) -> drum-gear 404s.
     See docs/decomp/patterns/fixable-struct-layout.md.

This linter is static — it runs in seconds and surfaces issues that would
otherwise only appear after a full `ninja main.dol` link cycle.

CLI:
    bin/lint-link-issues               # scan src/ tree, report all hits
    bin/lint-link-issues --unit X.cpp  # restrict to one TU + its includes
    bin/lint-link-issues --json
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
SRC_ROOT = REPO_ROOT / "src"

sys.path.insert(0, str(REPO_ROOT / "tools"))
import mwcc_symbols  # noqa: E402


# ---------------------------------------------------------------------------
# Stripping comments and string literals
# ---------------------------------------------------------------------------

_BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.DOTALL)
_LINE_COMMENT = re.compile(r"//[^\n]*")
_STRING_LIT = re.compile(r'"(?:[^"\\]|\\.)*"')
_CHAR_LIT = re.compile(r"'(?:[^'\\]|\\.)*'")


def strip_comments_and_strings(text: str) -> str:
    # Preserve newlines so line numbers stay accurate.
    def _keep_newlines(m: re.Match[str]) -> str:
        return "\n" * m.group(0).count("\n")

    text = _BLOCK_COMMENT.sub(_keep_newlines, text)
    text = _LINE_COMMENT.sub("", text)
    text = _STRING_LIT.sub('""', text)
    text = _CHAR_LIT.sub("' '", text)
    return text


# ---------------------------------------------------------------------------
# Issue model
# ---------------------------------------------------------------------------

@dataclass
class Issue:
    kind: str           # NON_STATIC_DUP | EXTERN_NO_DEF | ORPHAN_VIRTUAL
    name: str           # identifier or "Class::method"
    locations: list[str]  # "<file>:<line>" entries
    hint: str = ""      # one-line suggestion


# ---------------------------------------------------------------------------
# Check 1+2: file-scope globals
# ---------------------------------------------------------------------------

# Match a candidate file-scope object definition (after comment-stripping).
# Tail forms accepted: `;`, `=`, `(`, `[`.
#
# This is intentionally restrictive — we err on the side of missing some
# definitions rather than producing false positives. Real bugs of this
# class are always `Type name;` or `Type name(...);` at column 0 (or after
# optional `static`/`extern`).
_GLOBAL_DEFN_RE = re.compile(
    r"""^(?P<lead>(?:static\s+|extern\s+|inline\s+|const\s+|volatile\s+|register\s+|typedef\s+)*)
        (?P<type>[A-Za-z_:][A-Za-z0-9_:<>,\s*&]*?)
        \s+
        (?P<name>[A-Za-z_][A-Za-z0-9_]*)
        (?P<arr>\s*\[[^\]]*\])?
        \s*
        (?P<init>[=({;])""",
    re.VERBOSE,
)

# Compound type/decl keywords that disqualify a match (function defs,
# templates, class bodies). Doing this with a regex requires running over
# whole-file column-0 statements which we approximate by lines.
_FUNCTION_HINT_RE = re.compile(r"\)\s*(?:const)?\s*(?:override)?\s*(?:=\s*0)?\s*[{;]")
_BAD_TYPE_TOKENS = {
    "class",
    "struct",
    "union",
    "enum",
    "namespace",
    "template",
    "using",
    "typedef",
    "if",
    "else",
    "for",
    "while",
    "switch",
    "case",
    "return",
    "throw",
}


def _statement_terminator(lines: list[str], start: int) -> str | None:
    """Look ahead from start line to find the next `;` or `{` at parens
    depth 0, ignoring strings/comments (already stripped). Returns the
    character, or None if the statement is unterminated within scan
    window.
    """
    depth = 0
    for i in range(start, min(start + 20, len(lines))):
        for ch in lines[i]:
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
            elif ch == ";" and depth == 0:
                return ";"
            elif ch == "{" and depth == 0:
                return "{"
    return None


def find_globals(cpp_text: str) -> list[tuple[str, int, bool]]:
    """Return [(name, line, is_static_or_extern)] for top-level object defs.

    Conservative: only matches lines that begin at column 0 (no indent),
    where real file-scope globals always live. Anything inside a macro
    body, function body, or class declaration is indented and therefore
    skipped. This trades recall for precision — the goal is zero false
    positives on the BEGIN_LOADS-style macro pattern.
    """
    stripped = strip_comments_and_strings(cpp_text)
    lines = stripped.splitlines()
    out: list[tuple[str, int, bool]] = []
    for line_no, raw in enumerate(lines, start=1):
        if not raw or raw[0] in " \t":
            continue
        line = raw.strip()
        if not line or line.endswith("\\"):
            continue
        first = line.split(None, 1)[0]
        if first in _BAD_TYPE_TOKENS:
            continue
        # Function definitions/declarations on a single line.
        if "(" in line and _FUNCTION_HINT_RE.search(line):
            continue
        m = _GLOBAL_DEFN_RE.match(line)
        if not m:
            continue
        type_tok = m.group("type")
        if type_tok and "(" in line[: m.end("type")]:
            continue
        if "<" in type_tok and ">" not in type_tok:
            continue
        if any(op in line[: m.start("name")] for op in ("<<", ">>", "+=", "-=", "->")):
            continue
        # Multi-line lookahead: if the init token is `(`, the statement
        # might be either a global with ctor args (`Foo bar(...);`) or a
        # function definition (`void f(...) {`). Distinguish by scanning
        # ahead for `;` vs `{`.
        if m.group("init") == "(":
            term = _statement_terminator(lines, line_no - 1)
            if term != ";":
                continue
        name = m.group("name")
        lead = m.group("lead") or ""
        is_scoped = "static" in lead or "extern" in lead
        out.append((name, line_no, is_scoped))
    return out


def check_globals(cpp_files: list[Path]) -> list[Issue]:
    # Dedupe per file — multiple matches in one TU don't link-conflict.
    by_name: dict[str, dict[Path, tuple[int, bool]]] = defaultdict(dict)
    for cpp in cpp_files:
        try:
            text = cpp.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for name, line_no, scoped in find_globals(text):
            if cpp not in by_name[name]:
                by_name[name][cpp] = (line_no, scoped)

    issues: list[Issue] = []
    for name, per_file in by_name.items():
        non_scoped = [(p, ln) for p, (ln, sc) in per_file.items() if not sc]
        if len(non_scoped) < 2:
            continue
        locations = [f"{p.relative_to(REPO_ROOT)}:{ln}" for p, ln in non_scoped]
        issues.append(
            Issue(
                kind="NON_STATIC_DUP",
                name=name,
                locations=locations,
                hint=f"add `static` to all but one definition of `{name}`",
            )
        )
    return issues


# ---------------------------------------------------------------------------
# Check 3: orphan virtual override declarations
# ---------------------------------------------------------------------------

# Class header: `class Foo : public Bar, ... {` (single inheritance is the
# common case; we capture all derivations).
_CLASS_HEADER_RE = re.compile(
    r"""\b(?:class|struct)\s+
        (?P<name>[A-Za-z_][A-Za-z0-9_]*)
        (?P<derive>\s*:\s*[^{;]+)?
        \s*\{""",
    re.VERBOSE,
)

# Virtual method declaration. `virtual <ret> <name>(<args>) [const][...];`
# Excludes inline definitions (those have `{` instead of `;`) and pure
# virtuals (`= 0`).
_VIRTUAL_DECL_RE = re.compile(
    r"""virtual\s+
        (?P<ret>[A-Za-z_][\w:<>,\s*&]*?)
        \s+
        (?P<name>~?[A-Za-z_][A-Za-z0-9_]*)
        \s*\(
        (?P<args>[^)]*)
        \)
        \s*(?P<const>const)?
        \s*(?P<pure>=\s*0)?
        \s*;""",
    re.VERBOSE,
)


def _enclosing_class(header_text: str, decl_pos: int) -> Optional[str]:
    """Walk back from decl_pos to find the most recent unmatched `{` and
    extract the class name that opened it. Returns None for top-level."""
    depth = 0
    for i in range(decl_pos - 1, -1, -1):
        ch = header_text[i]
        if ch == "}":
            depth += 1
        elif ch == "{":
            if depth == 0:
                # Look back for `class Foo ... {`
                window = header_text[max(0, i - 400) : i + 1]
                m = list(_CLASS_HEADER_RE.finditer(window))
                if m:
                    return m[-1].group("name")
                return None
            depth -= 1
    return None


def find_virtual_decls(header_text: str) -> list[tuple[str, str, int]]:
    """Return [(class_name, method_name, line_no)] for non-inline,
    non-pure-virtual decls."""
    stripped = strip_comments_and_strings(header_text)
    out: list[tuple[str, str, int]] = []
    for m in _VIRTUAL_DECL_RE.finditer(stripped):
        if m.group("pure"):
            continue
        klass = _enclosing_class(stripped, m.start())
        if klass is None:
            continue
        method = m.group("name")
        if method.startswith("~"):
            # Destructor: rarely the bug pattern; skip to keep noise low.
            continue
        line_no = stripped.count("\n", 0, m.start()) + 1
        out.append((klass, method, line_no))
    return out


# Macros from src/system/obj/ObjMacros.h that synthesize method bodies.
# Match the macro invocation `BEGIN_X(ClassName)` and record the synthesized
# method on that class.
_MACRO_METHOD_MAP = {
    "BEGIN_HANDLERS": "Handle",
    "BEGIN_PROPSYNCS": "SyncProperty",
    "BEGIN_LOADS": "Load",
    "BEGIN_SAVES": "Save",
    "BEGIN_COPYS": "Copy",
    "SAVE_OBJ": "Save",
}
_MACRO_INVOKE_RE = re.compile(
    r"\b("
    + "|".join(_MACRO_METHOD_MAP.keys())
    + r")\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)"
)


def _build_impl_index(cpp_files: list[Path]) -> dict[str, set[str]]:
    """Return {class: {method, ...}} for every `<Class>::<method>(` body
    found in .cpp files, plus methods synthesized by Milo's BEGIN_HANDLERS-
    family macros (so the linter doesn't flag `Handle`, `SyncProperty`, etc.
    as orphan when they're macro-generated)."""
    impls: dict[str, set[str]] = defaultdict(set)
    impl_re = re.compile(
        r"([A-Za-z_][A-Za-z0-9_]*)\s*::\s*(~?[A-Za-z_][A-Za-z0-9_]*)\s*\("
    )
    for cpp in cpp_files:
        try:
            text = strip_comments_and_strings(cpp.read_text("utf-8", errors="replace"))
        except OSError:
            continue
        for m in impl_re.finditer(text):
            impls[m.group(1)].add(m.group(2))
        for m in _MACRO_INVOKE_RE.finditer(text):
            macro, klass = m.group(1), m.group(2)
            method = _MACRO_METHOD_MAP[macro]
            impls[klass].add(method)
    return impls


def check_virtuals(
    header_files: list[Path],
    cpp_files: list[Path],
    map_index: mwcc_symbols.MapIndex,
) -> list[Issue]:
    impl_index = _build_impl_index(cpp_files)
    issues: list[Issue] = []
    for hdr in header_files:
        try:
            text = hdr.read_text("utf-8", errors="replace")
        except OSError:
            continue
        for klass, method, line_no in find_virtual_decls(text):
            if method in impl_index.get(klass, set()):
                continue  # implemented in some .cpp
            if mwcc_symbols.find_override(klass, method, map_index):
                continue  # target binary has the symbol
            issues.append(
                Issue(
                    kind="ORPHAN_VIRTUAL",
                    name=f"{klass}::{method}",
                    locations=[f"{hdr.relative_to(REPO_ROOT)}:{line_no}"],
                    hint=(
                        f"remove redundant virtual decl — {klass} doesn't "
                        f"override {method}"
                    ),
                )
            )
    return issues


# ---------------------------------------------------------------------------
# Check 4: orphan NON-virtual member redeclarations (name-hiding "phantom stub")
# ---------------------------------------------------------------------------
# The non-virtual sibling of ORPHAN_VIRTUAL. A derived class re-declares a
# non-virtual member that a BASE class already declares+implements. C++
# name-hiding makes an unqualified call inside the derived class resolve to the
# (body-less) derived declaration -> undefined symbol. Unlike the virtual case
# there's no vtable reference, so it only bites when a call site names it; on
# native/web it resolves to a silent no-op stub returning garbage.
# Reference bug: BandCharacter::NameToDrumVenue (rb3 727cf01e) -> drum-gear 404.
# See docs/decomp/patterns/fixable-struct-layout.md.

# Non-virtual member decl: `[static|inline] <ret> <name>(<args>) [const];`
# with NO `virtual`. The base-defines gate (below) is what makes this precise,
# so the regex itself can be permissive.
_NONVIRTUAL_DECL_RE = re.compile(
    r"""(?P<stor>(?:static|inline)\s+)*
        (?P<ret>[A-Za-z_][\w:<>,\s*&]*?)
        \s+
        (?P<name>[A-Za-z_][A-Za-z0-9_]*)
        \s*\(
        (?P<args>[^)]*)
        \)
        \s*(?P<const>const)?
        \s*;""",
    re.VERBOSE,
)


def _build_inheritance(header_files: list[Path]) -> dict[str, list[str]]:
    """{class: [direct base names]} parsed from `class X : public A, B {`."""
    graph: dict[str, list[str]] = {}
    for hdr in header_files:
        try:
            text = strip_comments_and_strings(hdr.read_text("utf-8", errors="replace"))
        except OSError:
            continue
        for m in _CLASS_HEADER_RE.finditer(text):
            name = m.group("name")
            derive = m.group("derive") or ""
            # derive is like "  : public A, public B" — drop leading ws + colon
            derive = derive.strip().lstrip(":")
            bases: list[str] = []
            for b in derive.split(","):
                b = re.sub(r"\b(public|protected|private|virtual)\b", "", b)
                b = b.strip().split("<")[0].split("::")[-1].strip()
                if b:
                    bases.append(b)
            if name not in graph or bases:
                graph[name] = bases
    return graph


def _all_bases(cls: str, graph: dict[str, list[str]],
               seen: Optional[set[str]] = None) -> set[str]:
    if seen is None:
        seen = set()
    for b in graph.get(cls, ()):
        if b not in seen:
            seen.add(b)
            _all_bases(b, graph, seen)
    return seen


def find_nonvirtual_decls(header_text: str) -> list[tuple[str, str, int]]:
    """[(class, method, line)] for non-inline, non-virtual member decls."""
    stripped = strip_comments_and_strings(header_text)
    out: list[tuple[str, str, int]] = []
    for m in _NONVIRTUAL_DECL_RE.finditer(stripped):
        # the matched span must not be a virtual decl (handled separately)
        line_start = stripped.rfind("\n", 0, m.start()) + 1
        if "virtual" in stripped[line_start:m.end()]:
            continue
        name = m.group("name")
        ret = m.group("ret").strip()
        # skip ctors/dtors/operators and control keywords masquerading as ret
        if ret in ("return", "else", "case", "for", "while", "if", "switch",
                   "do", "sizeof") or name in ("if", "for", "while", "switch"):
            continue
        klass = _enclosing_class(stripped, m.start())
        if klass is None or name == klass:
            continue
        line_no = stripped.count("\n", 0, m.start()) + 1
        out.append((klass, name, line_no))
    return out


def check_nonvirtual_phantoms(
    header_files: list[Path],
    cpp_files: list[Path],
    map_index: mwcc_symbols.MapIndex,
    graph_headers: Optional[list[Path]] = None,
) -> list[Issue]:
    impl_index = _build_impl_index(cpp_files)
    # the inheritance graph must span ALL headers (a base may live outside the
    # scan roots), not just the ones we scan for decls.
    graph = _build_inheritance(graph_headers if graph_headers is not None
                              else header_files)
    issues: list[Issue] = []
    for hdr in header_files:
        try:
            text = hdr.read_text("utf-8", errors="replace")
        except OSError:
            continue
        for klass, method, line_no in find_nonvirtual_decls(text):
            # derived must NOT implement it (else it's a real override)
            if method in impl_index.get(klass, set()):
                continue
            # a transitive BASE must declare+implement it — this is the
            # name-hiding signature (and the precision gate)
            bases = _all_bases(klass, graph)
            base_definers = [b for b in bases if method in impl_index.get(b, set())]
            if not base_definers:
                continue
            # target binary must lack a derived-specific symbol (proof the
            # original never had this override) ...
            if mwcc_symbols.find_override(klass, method, map_index):
                continue
            # ... while it DOES have the base's real implementation
            if not any(mwcc_symbols.find_override(b, method, map_index)
                       for b in base_definers):
                continue
            issues.append(
                Issue(
                    kind="ORPHAN_NONVIRTUAL",
                    name=f"{klass}::{method}",
                    locations=[f"{hdr.relative_to(REPO_ROOT)}:{line_no}"],
                    hint=(
                        f"remove redundant non-virtual decl — {klass} hides "
                        f"inherited {base_definers[0]}::{method} (name-hiding "
                        f"phantom; resolves to a silent stub on native/web)"
                    ),
                )
            )
    return issues


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

def _is_scratch_path(p: Path) -> bool:
    # Concurrent-agent permuter scratch files and worktree backups.
    name = p.name
    return (
        name.startswith(".permuter_work_")
        or name.endswith(".bak")
        or name.endswith(".permuter_bak")
        or any(part.startswith(".permuter_work_") for part in p.parts)
    )


# Paths replaced wholesale in the native port — out of scope by default
# (see CLAUDE.md "Skip / deprioritize").
_OUT_OF_SCOPE_PREFIXES = (
    "src/sdk/",
    "src/lib/",
    "src/network/",
    "src/system/rndwii/",
    "src/system/os/",
    "src/system/stlport/",
    # Third-party libraries: vorbis, speex, zlib, soundtouch — replaced by
    # upstream sources in the native port, never source-promoted.
    "src/system/oggvorbis/",
    "src/system/speex/",
    "src/system/zlib/",
    "src/system/synthwii/soundtouch/",
)


# Specific files (not directory prefixes) excluded by default — generated
# scanner output that's never source-linked into the dol.
_OUT_OF_SCOPE_FILES = (
    "src/system/obj/DataFlex.c",
)


def _is_out_of_scope(p: Path) -> bool:
    try:
        rel = p.resolve().relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return False
    if rel.startswith(_OUT_OF_SCOPE_PREFIXES):
        return True
    if rel in _OUT_OF_SCOPE_FILES:
        return True
    return False


def collect_sources(
    roots: list[Path], *, in_scope_only: bool = True
) -> tuple[list[Path], list[Path]]:
    cpps: list[Path] = []
    hdrs: list[Path] = []

    def admit(p: Path) -> bool:
        if _is_scratch_path(p):
            return False
        if in_scope_only and _is_out_of_scope(p):
            return False
        return True

    for root in roots:
        if root.is_file():
            if not admit(root):
                continue
            if root.suffix in (".cpp", ".cc", ".cxx", ".c"):
                cpps.append(root)
            elif root.suffix in (".h", ".hpp", ".hxx"):
                hdrs.append(root)
            continue
        for p in root.rglob("*"):
            if not p.is_file() or not admit(p):
                continue
            if p.suffix in (".cpp", ".cc", ".cxx", ".c"):
                cpps.append(p)
            elif p.suffix in (".h", ".hpp", ".hxx"):
                hdrs.append(p)
    return cpps, hdrs


def _selftest() -> int:
    """Offline regression test for the parser/attribution logic (no map needed).
    Locks in the BandCharacter::NameToDrumVenue phantom detection (rb3 727cf01e)
    and the cross-class-boundary contamination guard."""
    import tempfile

    ok = True

    def check(cond: bool, msg: str):
        nonlocal ok
        print(f"  {'PASS' if cond else 'FAIL'}: {msg}")
        ok = ok and cond

    # 1) non-virtual decl extraction + correct enclosing-class attribution
    hdr = """
class BandCharDesc { public: static int NameToDrumVenue(const char *); };
class BandCharacter : public Character, public BandCharDesc {
public:
    static int NameToDrumVenue(const char *);   // phantom redeclaration
    void SetGroupName(const char *);
};
class OwnedSongSortNode : public SongSortNode {
public:
    int GetTotalStars(bool);   // sibling-class non-virtual decl
};
class FunctionSortNode : public LeafSortNode { public: void FinishSort(NodeSort *); };
"""
    decls = set((k, m) for k, m, _ in find_nonvirtual_decls(hdr))
    check(("BandCharacter", "NameToDrumVenue") in decls,
          "extracts the derived phantom decl")
    check(("BandCharDesc", "NameToDrumVenue") in decls,
          "extracts the base decl")
    check(("FunctionSortNode", "GetTotalStars") not in decls,
          "no cross-class-boundary contamination (GetTotalStars stays in its class)")

    # 2) inheritance parse — single AND multiple inheritance, first base clean
    with tempfile.NamedTemporaryFile("w", suffix=".h", delete=False) as f:
        f.write(hdr)
        p = Path(f.name)
    g = _build_inheritance([p])
    check(g.get("BandCharacter", [None])[0] == "Character",
          "first base of multi-inheritance parses without leading colon")
    check("BandCharDesc" in _all_bases("BandCharacter", g),
          "transitive base resolution finds the definer")

    print("SELFTEST:", "OK" if ok else "FAILED")
    return 0 if ok else 1


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="Pre-flight link-error linter.")
    ap.add_argument(
        "--selftest",
        action="store_true",
        help="run the offline parser regression test and exit",
    )
    ap.add_argument(
        "paths",
        nargs="*",
        help="Files or directories to scan (default: src/)",
    )
    ap.add_argument(
        "--check",
        choices=["all", "globals", "virtuals", "nonvirtuals"],
        default="all",
    )
    ap.add_argument("--json", action="store_true")
    ap.add_argument(
        "--all",
        action="store_true",
        help="Include out-of-scope paths (sdk/, lib/, system/os, system/rndwii).",
    )
    args = ap.parse_args(argv)

    if args.selftest:
        return _selftest()

    in_scope = not args.all
    roots = [Path(p) for p in args.paths] if args.paths else [SRC_ROOT]
    cpp_files, hdr_files = collect_sources(roots, in_scope_only=in_scope)

    issues: list[Issue] = []
    if args.check in ("all", "globals"):
        issues.extend(check_globals(cpp_files))
    if args.check in ("all", "virtuals", "nonvirtuals"):
        idx = mwcc_symbols.load_map()
        # We always need *all* cpps for the impl-index (and all headers for the
        # inheritance graph) so we don't false-positive on impls/bases outside
        # the scan roots.
        all_cpps = cpp_files
        all_hdrs = hdr_files
        if any(r != SRC_ROOT for r in roots):
            all_cpps, all_hdrs = collect_sources([SRC_ROOT], in_scope_only=in_scope)
        if args.check in ("all", "virtuals"):
            issues.extend(check_virtuals(hdr_files, all_cpps, idx))
        if args.check in ("all", "nonvirtuals"):
            issues.extend(check_nonvirtual_phantoms(
                hdr_files, all_cpps, idx, graph_headers=all_hdrs))

    if args.json:
        json.dump([asdict(i) for i in issues], sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        if not issues:
            print("no link issues detected")
            return 0
        for issue in issues:
            print(f"[{issue.kind}] {issue.name}")
            for loc in issue.locations:
                print(f"    {loc}")
            if issue.hint:
                print(f"    → {issue.hint}")
            print()
        print(f"{len(issues)} issue(s) found")

    return 1 if issues else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
