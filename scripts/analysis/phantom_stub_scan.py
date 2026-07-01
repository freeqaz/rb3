#!/usr/bin/env python3
"""phantom_stub_scan.py — find "phantom link-stub" candidates.

A *phantom stub* is an undefined symbol that exists only because a derived class
RE-DECLARES a member function already declared AND implemented by a base class.
The redundant derived declaration hides (C++ name-hiding) the inherited one, so
an unqualified call inside the derived class resolves to `Derived::method`, which
has no body -> undefined symbol. On the Wii decomp it fails to link; on the
native/web port it silently resolves to a no-op stub returning garbage.

Reference bug: BandCharacter::NameToDrumVenue (rb3 727cf01e) -> drum-gear 404s.
Sibling (already documented): the VIRTUAL variant, where the redundant override
makes the derived vtable reference Derived::method — see
docs/decomp/patterns/fixable-struct-layout.md.

Detection (precise — inheritance-aware):
  A class D is a phantom candidate for method M iff
    (1) D's header DECLARES M (a member declaration, not a definition), AND
    (2) some transitive base B of D DEFINES M out-of-line (B::M in a .cpp), AND
    (3) D does NOT define M itself (no D::M in any .cpp), AND
    (4) [RB3 only, confidence] the MWCC symbol M__<mangled D> is ABSENT from the
        Wii map (the target never had a D-specific M) while M__<mangled B> IS
        present (the base's real implementation).

Modes:
  --mode headers  scan ALL class headers under --src (works for RB3 and DC3).
  --mode stubs    RB3 only: additionally require D::M to appear in a native
                  weak-stub .s file (ranks the ACTIVE phantoms first).

Usage:
  scripts/analysis/phantom_stub_scan.py --mode headers --json
  scripts/analysis/phantom_stub_scan.py --mode stubs --json
  scripts/analysis/phantom_stub_scan.py --mode headers \
      --src /home/free/code/milohax/dc3-decomp/src \
      --map /home/free/code/milohax/dc3-decomp/orig/373307D9/ham_xbox_r.map --json
"""
import argparse
import json
import os
import re
import subprocess
import sys
from collections import defaultdict

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RB3_MAP = os.path.join(REPO, "orig/SZBE69_B8/files/band_r_wii.map")
RB3_STUB_FILES = [
    "native/src/band3_link_stubs.s",
    "native/src/dta_link_stubs.s",
    "native/src/rndobj_synth_link_stubs.s",
]

# common method names that are almost always virtual overrides (each class has
# its OWN body) — a derived redeclaring these is normal polymorphism, not a
# name-hiding phantom. We still REPORT them but flag low-confidence.
VIRTUAL_NOISE = {
    "Draw", "Poll", "Load", "Save", "Copy", "Handle", "SyncProperty", "Init",
    "Terminate", "Enter", "Exit", "Print", "Replace", "PreLoad", "PostLoad",
    "Export", "Mats", "DrawShowing", "CollideShowing", "Highlight",
}

CLASS_DECL_RE = re.compile(
    r"\bclass\s+([A-Za-z_]\w*)\s*(?::\s*([^{]+?))?\s*\{")
# a member declaration line: `RetType Method(args);` (no body, ends with ;)
# captured loosely; the agent confirms.
DEF_RE_CACHE = {}


def sh(args):
    return subprocess.run(args, capture_output=True, text=True).stdout


def build_class_index(src_root):
    """class name -> (header_path, [direct base names], set(declared method names))."""
    index = {}
    for dirpath, _, files in os.walk(src_root):
        for fn in files:
            if not fn.endswith((".h", ".hpp")):
                continue
            path = os.path.join(dirpath, fn)
            try:
                text = open(path, errors="ignore").read()
            except Exception:
                continue
            for m in CLASS_DECL_RE.finditer(text):
                name = m.group(1)
                bases_raw = m.group(2) or ""
                bases = []
                for b in bases_raw.split(","):
                    b = b.strip()
                    b = re.sub(r"\b(public|protected|private|virtual)\b", "", b)
                    b = b.strip().split("<")[0].split("::")[-1].strip()
                    if b:
                        bases.append(b)
                # body up to the MATCHING close brace (brace-counting) — a fixed
                # char window leaks methods across class boundaries and produces
                # false attributions (e.g. a sibling class's non-virtual decl).
                body_start = m.end()
                depth = 1
                j = body_start
                n = len(text)
                while j < n and depth > 0:
                    ch = text[j]
                    if ch == '{':
                        depth += 1
                    elif ch == '}':
                        depth -= 1
                    j += 1
                body = text[body_start:j - 1]
                # method name -> is it declared `virtual`? (name-hiding phantoms
                # are the NON-virtual ones; virtual ones are the vtable variant
                # already caught by bin/lint-link-issues ORPHAN_VIRTUAL).
                # A member DECLARATION requires a return type before the name
                # (`Ret Name(args);`); demanding the ret-type excludes call sites
                # like `da->Foo();` / `obj.Bar();` that a bare `Name(...)` match
                # would otherwise pick up as false declarations.
                methods = {}
                for decl in re.finditer(
                        r"(?P<virt>virtual\s+)?(?:static\s+|inline\s+)*"
                        r"(?P<ret>[A-Za-z_][\w:<>,*&\s]*?)\s+"
                        r"(?P<name>[A-Za-z_]\w*)\s*\([^;{]*\)\s*(?:const\s*)?;",
                        body):
                    ret = decl.group("ret").strip()
                    if ret in ("return", "else", "case", "for", "while", "if",
                               "switch", "do", "sizeof"):
                        continue
                    methods[decl.group("name")] = bool(decl.group("virt"))
                # first declaration wins (header that introduces the class)
                if name not in index or bases:
                    index[name] = (path, bases, methods)
    return index


def transitive_bases(cls, index, seen=None):
    if seen is None:
        seen = set()
    entry = index.get(cls)
    if not entry:
        return seen
    for b in entry[1]:
        if b not in seen:
            seen.add(b)
            transitive_bases(b, index, seen)
    return seen


_IMPL_INDEX = None  # {class: set(method)} built once over the whole tree
_IMPL_RE = re.compile(r"([A-Za-z_]\w*)\s*::\s*(~?[A-Za-z_]\w*)\s*\(")


def _build_impl_index(src_root):
    """One pass over all .cpp: {class: {method,...}} for `Class::method(` defs.
    Replaces the per-(class,method) grep — O(files) instead of O(candidates*tree)."""
    impls = {}
    for dirpath, _, files in os.walk(src_root):
        for fn in files:
            if not fn.endswith((".cpp", ".cc", ".cxx", ".c")):
                continue
            try:
                text = open(os.path.join(dirpath, fn), errors="ignore").read()
            except Exception:
                continue
            for m in _IMPL_RE.finditer(text):
                impls.setdefault(m.group(1), set()).add(m.group(2))
    return impls


def defines_method(cls, method, src_root):
    """does `cls::method(` appear as an out-of-line definition in any .cpp?"""
    global _IMPL_INDEX
    if _IMPL_INDEX is None:
        _IMPL_INDEX = _build_impl_index(src_root)
    return method in _IMPL_INDEX.get(cls, ())


def mwcc_mangle_prefix(cls, method):
    """MWCC member prefix: Method__<len><Class>F  (no namespace handling)."""
    return f"{method}__{len(cls)}{cls}F"


def map_has(prefix, map_text):
    return prefix in map_text


def read_stub_methods():
    """set of (Class, method) demangled from the RB3 weak-stub .s files."""
    syms = []
    for rel in RB3_STUB_FILES:
        path = os.path.join(REPO, rel)
        if not os.path.exists(path):
            continue
        for line in open(path):
            m = re.match(r"\s*\.weak\s+(\S+)", line)
            if m:
                syms.append(m.group(1))
    dem = sh(["c++filt"] + []) if False else None
    p = subprocess.run(["c++filt"], input="\n".join(syms),
                       capture_output=True, text=True)
    pairs = set()
    for d in p.stdout.splitlines():
        if any(k in d for k in ("thunk", "vtable", "typeinfo", "guard")):
            continue
        # strip args
        depth = 0
        cut = None
        for i in range(len(d) - 1, -1, -1):
            if d[i] == ')':
                depth += 1
            elif d[i] == '(':
                depth -= 1
                if depth == 0:
                    cut = i
                    break
        name = (d[:cut] if cut is not None else d).strip()
        if "::" not in name:
            continue
        cls, _, meth = name.rpartition("::")
        cls = cls.split("::")[-1].split("<")[0]
        if meth.startswith("~") or meth.startswith("operator") or meth == cls:
            continue
        if re.match(r"^[A-Za-z_]\w*$", meth):
            pairs.add((cls, meth))
    return pairs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=["headers", "stubs"], default="headers")
    ap.add_argument("--src", default=os.path.join(REPO, "src"))
    ap.add_argument("--map", default=RB3_MAP)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    index = build_class_index(args.src)
    map_text = ""
    if args.map and os.path.exists(args.map):
        map_text = open(args.map, errors="ignore").read()

    stub_pairs = read_stub_methods() if args.mode == "stubs" else None

    candidates = []
    for cls, (header, bases, methods) in index.items():
        tbases = transitive_bases(cls, index)
        for meth, is_virtual in methods.items():
            if args.mode == "stubs" and (cls, meth) not in stub_pairs:
                continue
            # base that DEFINES the method out-of-line
            definers = [b for b in tbases if defines_method(b, meth, args.src)]
            if not definers:
                continue
            # derived must NOT define it itself
            if defines_method(cls, meth, args.src):
                continue
            conf = "low" if meth in VIRTUAL_NOISE else "high"
            entry = {
                "class": cls,
                "method": meth,
                "header": os.path.relpath(header, args.src),
                "base_definers": definers,
                "is_virtual": is_virtual,
                "variant": "virtual (vtable)" if is_virtual
                           else "non-virtual (name-hiding)",
                "confidence": conf,
            }
            if map_text:
                d_prefix = mwcc_mangle_prefix(cls, meth)
                entry["derived_sym_in_map"] = map_has(d_prefix, map_text)
                entry["base_sym_in_map"] = any(
                    map_has(mwcc_mangle_prefix(b, meth), map_text)
                    for b in definers)
                # strongest signal: derived absent, base present
                if not entry["derived_sym_in_map"] and entry["base_sym_in_map"] \
                        and conf == "high":
                    entry["confidence"] = "very_high"
            if args.mode == "stubs":
                entry["actively_stubbed"] = True
            candidates.append(entry)

    order = {"very_high": 0, "high": 1, "low": 2}
    candidates.sort(key=lambda c: (order.get(c["confidence"], 3),
                                   c["class"], c["method"]))
    if args.json:
        print(json.dumps(candidates, indent=1))
    else:
        print(f"# {len(candidates)} phantom candidates (mode={args.mode}, "
              f"src={args.src})\n")
        for c in candidates:
            mapinfo = ""
            if "derived_sym_in_map" in c:
                mapinfo = (f"  map[D={c['derived_sym_in_map']} "
                           f"B={c['base_sym_in_map']}]")
            v = "V" if c["is_virtual"] else "N"
            print(f"  [{c['confidence']:9}][{v}] {c['class']}::{c['method']}  "
                  f"<- {', '.join(c['base_definers'])}  ({c['header']}){mapinfo}")


if __name__ == "__main__":
    main()
