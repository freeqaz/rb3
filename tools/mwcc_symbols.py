"""MWCC symbol query helper.

Parse the band_r_wii.map symbol table once and answer "does the target binary
contain an override of <Class>::<Method>?" robustly across MWCC mangling
variants (const, namespace, template).

Used by `bin/lint-link-issues` to detect redundant virtual declarations that
link-error after promoting a TU to source.

Public API:
    load_map(map_path=None)             -> MapIndex     (per-process cached)
    find_override(klass, method, idx=None) -> list[Symbol]
    demangle(mangled)                   -> str | None

A Symbol is `(mangled, demangled, file)`.

Implementation notes:
- The map columns are `addr size virt fileoff align symbol\t lib obj`. We
  detect the `.text section layout` block and parse until the next section.
- We index the mangled symbol table by the *content* of the class encoding
  (length-prefix and namespace-prefix variants both yield the same class).
- Demangling shells out to `tools/upstream/batch-demangle` once per call; we cache.
"""

from __future__ import annotations

import os
import pickle
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


# Mangling primitives:
#   - "F" introduces argument list (after class encoding + optional const "C")
#   - Free function: "<Method>__F..."
#   - Member: "<Method>__<n><Class>[C]F..."  where <n> is class-name length
#   - Namespaced member: "<Method>__Q2<NSlen><NS><n><Class>[C]F..."
#       (Q2 = 2-component qualified name; Q3 etc. exist for deeper nesting)
#   - Template-instantiated method:
#       "<Method><TArgs>__<n><Class>[C]F..." or
#       "<Method>__<n><Class><TArgs>[C]F..."
#       where <TArgs> looks like "<...>" with balanced angle brackets.

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_MAP = REPO_ROOT / "orig" / "SZBE69_B8" / "files" / "band_r_wii.map"
DEFAULT_CACHE = REPO_ROOT / "build" / "SZBE69_B8" / ".mwcc_symbols.pkl"
BATCH_DEMANGLE = REPO_ROOT / "tools" / "upstream" / "batch-demangle"


@dataclass
class Symbol:
    mangled: str
    file: str  # source object path from the map (e.g. .../Gathering.o)

    @property
    def is_quazal(self) -> bool:
        # Mangled namespace prefix uses Q<N>... encoding.
        return "__Q" in self.mangled


class MapIndex:
    """Indexed view of the .text section of the map file.

    `by_class[class_name]` -> list of Symbol entries whose mangled name
    encodes that class as the immediate owner. Namespace-qualified classes
    appear under both the bare name (e.g. "Gathering") and the qualified
    form ("Quazal::Gathering").
    """

    def __init__(self) -> None:
        self.by_class: dict[str, list[Symbol]] = {}
        self.by_mangled: dict[str, Symbol] = {}

    def add(self, klass: str, sym: Symbol) -> None:
        self.by_class.setdefault(klass, []).append(sym)
        self.by_mangled[sym.mangled] = sym


_MAP_CACHE: Optional[MapIndex] = None


# ---------------------------------------------------------------------------
# Map parsing
# ---------------------------------------------------------------------------

# A `.text` symbol line. Format variants:
#   `  <off> <size> <virt> <fileoff> <align> <sym> \t<lib.a> <path>`  (lib member)
#   `  <off> <size> <virt> <fileoff> <align> <sym> \t<path>`          (loose .o)
# We capture the symbol; the source-file path is whatever follows the tab.
_TEXT_LINE_RE = re.compile(
    r"^\s+([0-9a-f]{8})\s+([0-9a-f]{6})\s+[0-9a-f]{8}\s+[0-9a-f]{8}\s+\d+\s+(\S+)(?:\s*\t(.+))?$"
)


def _parse_map_text_section(map_path: Path) -> list[tuple[str, str]]:
    """Return [(mangled_symbol, source_file)] for every `.text` symbol."""
    out: list[tuple[str, str]] = []
    in_text = False
    with map_path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            stripped = line.rstrip("\n")
            if stripped.endswith("section layout"):
                in_text = stripped.startswith(".text")
                continue
            if not in_text:
                continue
            m = _TEXT_LINE_RE.match(stripped)
            if not m:
                continue
            mangled = m.group(3)
            if mangled.startswith("@") or mangled.startswith("*"):
                continue
            src = (m.group(4) or "").strip()
            out.append((mangled, src))
    return out


# ---------------------------------------------------------------------------
# Class-name extraction from a mangled symbol
# ---------------------------------------------------------------------------

def _consume_length_name(s: str, pos: int) -> tuple[str, int] | None:
    """If s[pos:] starts with `<n><n chars>`, return (name, new_pos)."""
    n = 0
    p = pos
    while p < len(s) and s[p].isdigit():
        n = n * 10 + int(s[p])
        p += 1
    if p == pos or n == 0:
        return None
    end = p + n
    if end > len(s):
        return None
    name = s[p:end]
    return name, end


def _consume_balanced_template(s: str, pos: int) -> int | None:
    """If s[pos] == '<', skip to matching '>' and return new pos. Else None."""
    if pos >= len(s) or s[pos] != "<":
        return None
    depth = 0
    for i in range(pos, len(s)):
        c = s[i]
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
            if depth == 0:
                return i + 1
    return None


def _extract_class(mangled: str) -> tuple[str, str] | None:
    """Return (bare_class, qualified_class) or None for non-member symbols.

    qualified_class joins namespace components with '::'.
    """
    # Method name and (possibly) template args come before the "__".
    if "__" not in mangled:
        return None
    # Find the *last* "__" that separates the name from the class-encoding,
    # because operator mangles begin with "__op", "__rs", "__ct", etc.
    sep = mangled.rfind("__")
    if sep == -1:
        return None
    tail = mangled[sep + 2 :]
    if not tail:
        return None

    # Tail starts with the class-encoding, then optional "C", then "F<args>".
    # Free function: tail begins with "F".
    if tail.startswith("F"):
        return None

    pos = 0
    components: list[str] = []

    if tail.startswith("Q"):
        # Qualified: Q<N>... where N is the number of components (single
        # digit in MWCC). The following digits are length-prefixes for each
        # component.
        if len(tail) < 2 or not tail[1].isdigit():
            return None
        ncomp = int(tail[1])
        pos = 2
        for _ in range(ncomp):
            r = _consume_length_name(tail, pos)
            if r is None:
                return None
            name, pos = r
            # Optional template instantiation immediately after the class name
            tend = _consume_balanced_template(tail, pos)
            if tend is not None:
                name = name + tail[pos:tend]
                pos = tend
            components.append(name)
    else:
        # Single class name (possibly templated).
        r = _consume_length_name(tail, pos)
        if r is None:
            return None
        name, pos = r
        tend = _consume_balanced_template(tail, pos)
        if tend is not None:
            name = name + tail[pos:tend]
            pos = tend
        components.append(name)

    # The remainder must start with "C" (const) and/or "F" (args). If not,
    # we mis-parsed — bail.
    rest = tail[pos:]
    if not (rest.startswith("F") or rest.startswith("C")):
        return None

    bare = components[-1]
    qualified = "::".join(components)
    return bare, qualified


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def load_map(map_path: Optional[Path] = None, *, use_cache: bool = True) -> MapIndex:
    """Parse the map file once and return an indexed MapIndex.

    The result is cached in-process and on disk (under build/) so repeated
    invocations across the linter / sweep tooling stay fast.
    """
    global _MAP_CACHE
    if _MAP_CACHE is not None and map_path is None:
        return _MAP_CACHE

    path = map_path or DEFAULT_MAP
    if not path.exists():
        raise FileNotFoundError(f"map file not found: {path}")

    cache_path = DEFAULT_CACHE
    if use_cache and cache_path.exists():
        try:
            if cache_path.stat().st_mtime >= path.stat().st_mtime:
                with cache_path.open("rb") as f:
                    idx = pickle.load(f)
                if isinstance(idx, MapIndex):
                    if map_path is None:
                        _MAP_CACHE = idx
                    return idx
        except (OSError, pickle.PickleError, AttributeError, ModuleNotFoundError):
            # Stale cache (e.g. pickled from `__main__` instead of module
            # import) — drop and rebuild.
            try:
                cache_path.unlink()
            except OSError:
                pass

    idx = MapIndex()
    for mangled, src in _parse_map_text_section(path):
        ce = _extract_class(mangled)
        sym = Symbol(mangled=mangled, file=src)
        if ce is None:
            continue
        bare, qualified = ce
        # Strip template args from the class lookup key — clients ask by
        # plain class name. Keep both forms when they differ.
        bare_plain = bare.split("<", 1)[0]
        idx.add(bare_plain, sym)
        if bare_plain != bare:
            idx.add(bare, sym)
        if qualified != bare and "::" in qualified:
            qualified_plain = "::".join(p.split("<", 1)[0] for p in qualified.split("::"))
            idx.add(qualified, sym)
            if qualified_plain != qualified:
                idx.add(qualified_plain, sym)

    if use_cache:
        try:
            cache_path.parent.mkdir(parents=True, exist_ok=True)
            with cache_path.open("wb") as f:
                pickle.dump(idx, f)
        except OSError:
            pass

    if map_path is None:
        _MAP_CACHE = idx
    return idx


_METHOD_RE_CACHE: dict[str, re.Pattern[str]] = {}


def _method_anchor_regex(method: str) -> re.Pattern[str]:
    """A regex anchored to start that matches '<method>[<template>]__'."""
    if method in _METHOD_RE_CACHE:
        return _METHOD_RE_CACHE[method]
    # Escape method for regex; allow optional balanced <...> directly after.
    pat = re.compile(r"^" + re.escape(method) + r"(?:<[^_]*>)?__")
    _METHOD_RE_CACHE[method] = pat
    return pat


def find_override(
    klass: str,
    method: str,
    idx: Optional[MapIndex] = None,
) -> list[Symbol]:
    """Symbols implementing `<klass>::<method>` in the target binary.

    `klass` may be a bare name ("Gathering") or namespace-qualified
    ("Quazal::Gathering"). Const-qualified, non-const, and template
    instantiations are all returned.
    """
    if idx is None:
        idx = load_map()
    candidates = idx.by_class.get(klass, [])
    pat = _method_anchor_regex(method)
    out: list[Symbol] = []
    for sym in candidates:
        if pat.match(sym.mangled):
            out.append(sym)
    return out


_DEMANGLE_CACHE: dict[str, Optional[str]] = {}


def _ensure_batch_demangle_built() -> Optional[Path]:
    bin_path = BATCH_DEMANGLE / "target" / "release" / "rb3-batch-demangle"
    if bin_path.exists():
        return bin_path
    try:
        subprocess.run(
            ["cargo", "build", "--release"],
            cwd=BATCH_DEMANGLE,
            check=True,
            capture_output=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    return bin_path if bin_path.exists() else None


def demangle(mangled: str) -> Optional[str]:
    """Return the demangled form, or None on failure.

    Uses the batch-demangle Rust tool but feeds it a single fabricated
    map-line so it doesn't require a file. For real performance, the linter
    should batch by writing many lines to a temp file.
    """
    if mangled in _DEMANGLE_CACHE:
        return _DEMANGLE_CACHE[mangled]
    bin_path = _ensure_batch_demangle_built()
    if bin_path is None:
        _DEMANGLE_CACHE[mangled] = None
        return None
    # Use a tempfile with one line; batch-demangle reads from a file path.
    import tempfile

    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as tf:
        tf.write(f"{mangled} = .text:0x80000000; // type:function size:0x4\n")
        tmp = tf.name
    try:
        r = subprocess.run(
            [str(bin_path), tmp],
            capture_output=True,
            text=True,
            check=False,
        )
    finally:
        try:
            os.unlink(tmp)
        except OSError:
            pass
    if r.returncode != 0:
        _DEMANGLE_CACHE[mangled] = None
        return None
    line = r.stdout.strip()
    parts = line.split("|||")
    result = parts[2] if len(parts) >= 3 else None
    _DEMANGLE_CACHE[mangled] = result
    return result


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _main(argv: list[str]) -> int:
    import argparse

    p = argparse.ArgumentParser(description="Query the band_r_wii.map symbol table.")
    sub = p.add_subparsers(dest="cmd", required=True)

    f = sub.add_parser("find", help="Find override symbols for <Class>::<Method>")
    f.add_argument("klass")
    f.add_argument("method")

    d = sub.add_parser("demangle", help="Demangle one mangled symbol")
    d.add_argument("mangled")

    sub.add_parser("rebuild", help="Force-rebuild the on-disk cache")

    args = p.parse_args(argv)

    if args.cmd == "rebuild":
        if DEFAULT_CACHE.exists():
            DEFAULT_CACHE.unlink()
        idx = load_map(use_cache=False)
        print(f"indexed {len(idx.by_mangled)} symbols across {len(idx.by_class)} classes")
        return 0

    if args.cmd == "find":
        hits = find_override(args.klass, args.method)
        for sym in hits:
            print(sym.mangled)
        if not hits:
            print(f"(no symbols matching {args.klass}::{args.method})", file=sys.stderr)
            return 1
        return 0

    if args.cmd == "demangle":
        r = demangle(args.mangled)
        if r is None:
            print(f"(failed to demangle {args.mangled})", file=sys.stderr)
            return 1
        print(r)
        return 0

    return 2


if __name__ == "__main__":
    # Re-import via the package path so pickled MapIndex instances refer to
    # `mwcc_symbols.MapIndex` rather than `__main__.MapIndex`, which would
    # be unloadable from any other importing module.
    if __package__ is None and "mwcc_symbols" not in sys.modules:
        import importlib.util
        spec = importlib.util.spec_from_file_location("mwcc_symbols", __file__)
        mod = importlib.util.module_from_spec(spec)  # type: ignore[arg-type]
        sys.modules["mwcc_symbols"] = mod
        assert spec and spec.loader
        spec.loader.exec_module(mod)
        raise SystemExit(mod._main(sys.argv[1:]))
    raise SystemExit(_main(sys.argv[1:]))
