#!/usr/bin/env python3
"""Make ninja depfiles (.d) location-independent, and VERIFY that they are.

Why this exists
---------------
`tools/setup-worktree.sh` reflink-copies the main repo's `build/` into a new
worktree so the object cache starts warm. Historically the copied `.d` files
listed every header dependency by ABSOLUTE path into the MAIN repo. Because
`deps="gcc"` is deliberately disabled (ninja reads `.d` files directly), a
header edit inside the worktree then invalidated NOTHING: ninja checked each
dependency against main's copy of the header, found it unchanged, printed
"ninja: no work to do", and objdiff returned a STALE, IDENTICAL result.

That failure is silent. It does not error — it reports NO CHANGE, which reads
exactly like a genuine negative result and gets recorded as one. rb3 lane
`x24-rotatez` lost a build cycle to it.

`tools/transform_dep.py` now emits repo-relative prerequisites, so newly
compiled depfiles are correct by construction. This script does the other two
jobs:

  1. MIGRATE an existing build cache whose `.d` files predate that fix
     (rewrite in-root absolute prerequisites to relative). Idempotent.
  2. VERIFY (`--check`) that no depfile references a foreign checkout. This is
     the loud-failure guard: a depfile that points at another tree is a
     silent-stale-read waiting to happen, so we fail the build instead.

Rewriting only changes path SPELLING, never which file is named, so it cannot
make ninja rebuild: ninja compares the mtimes of the named files, and a
relative path resolved from the build root names the same inode.

Usage
-----
  python3 tools/normalize-depfiles.py                 # migrate ./build in cwd
  python3 tools/normalize-depfiles.py --check         # verify only, exit 1 on bad
  python3 tools/normalize-depfiles.py --root /path/to/worktree
"""

import argparse
import os
import sys

# Scratch depfiles written by the source permuter, NOT by ninja. They are raw
# mwcc output (CRLF, unconverted `Z:\` paths) for throwaway `.permuter_work_*`
# translation units that no ninja edge depends on. Normalizing them would be
# harmless but pointless churn, and their absolute paths are not a staleness
# risk because nothing reads them as a dependency.
SKIP_PREFIX = ".permuter_work_"


# Top-level directories that mark the start of a repo-relative path. Used to
# recognise the tail of a FOREIGN checkout's absolute path.
REPO_TOPS = ("src/", "include/", "config/", "build/")


class Relativizer:
    """Turn absolute depfile prerequisites into root-relative ones.

    Two distinct cases both need rewriting, and conflating them was a real bug:

      1. Path is under THIS root (`<root>/src/Foo.h`). Strip the prefix.
      2. Path is under ANOTHER checkout (`/…/rb3/src/Foo.h` while we are a
         worktree at `/…/rb3/.claude/worktrees/x`). This is the actual reflinked
         -cache case, and it IS migratable: the repo-relative tail `src/Foo.h`
         names our own copy. Rewriting it to that tail is exactly the fix.

    A path with no local counterpart (a genuine out-of-tree file) is left alone
    and reported — unportable, but not a staleness trap.

    Foreign roots are cached after first discovery, so the filesystem probing is
    O(number of distinct checkouts), not O(number of prerequisite paths) —
    which matters at ~308k paths.
    """

    def __init__(self, root):
        self.root = root.rstrip("/")
        self.prefix = self.root + "/"
        self.foreign_prefixes = []

    def relativize(self, path):
        """Return (new_path, unfixable)."""
        p = os.path.normpath(path)
        if not p.startswith("/"):
            return p, False
        if p.startswith(self.prefix):
            return p[len(self.prefix) :], False
        for fp in self.foreign_prefixes:
            if p.startswith(fp):
                return p[len(fp) :], False
        parts = p.strip("/").split("/")
        for i in range(len(parts)):
            tail = "/".join(parts[i:])
            if not tail.startswith(REPO_TOPS):
                continue
            if os.path.exists(os.path.join(self.root, tail)):
                self.foreign_prefixes.append(p[: len(p) - len(tail)])
                return tail, False
        return p, True


def normalize_text(text, rel):
    """Rewrite absolute prerequisites to root-relative form.

    Returns (new_text, unfixable_absolute_paths, shadowing_examples).
    """
    out_lines = []
    unfixable = []
    shadowing = []
    for idx, raw in enumerate(text.split("\n")):
        line = raw.rstrip("\r")
        if idx == 0 or not line.strip():
            out_lines.append(line)
            continue
        suffix = ""
        body = line
        if body.endswith(" \\"):
            suffix = " \\"
            body = body[:-2]
        path = body.strip()
        if not path:
            out_lines.append(line)
            continue
        if path.startswith("/"):
            new_path, bad = rel.relativize(path)
            if bad:
                unfixable.append(new_path)
            elif not new_path.startswith("/"):
                shadowing.append((path, new_path))
            path = new_path
        out_lines.append("\t" + path + suffix)
    return "\n".join(out_lines), unfixable, shadowing


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=None,
                    help="Build root (default: cwd). Prerequisites are made relative to this.")
    ap.add_argument("--build-dir", default=None,
                    help="Directory to scan for .d files (default: <root>/build).")
    ap.add_argument("--check", action="store_true",
                    help="Verify only; write nothing. Exit 1 if any depfile is "
                         "non-relative or references a foreign checkout.")
    ap.add_argument("--quiet", action="store_true", help="Only print problems.")
    args = ap.parse_args()

    root = os.path.abspath(args.root) if args.root else os.getcwd()
    build_dir = os.path.abspath(args.build_dir) if args.build_dir else os.path.join(root, "build")

    if not os.path.isdir(build_dir):
        if not args.quiet:
            print(f"normalize-depfiles: no build dir at {build_dir}; nothing to do")
        return 0

    rel = Relativizer(root)
    scanned = changed = 0
    shadow_examples = []
    unfixable_examples = []
    n_shadow = n_unfixable = 0

    for dirpath, _dirnames, filenames in os.walk(build_dir):
        for name in filenames:
            if not name.endswith(".d") or name.startswith(SKIP_PREFIX):
                continue
            path = os.path.join(dirpath, name)
            scanned += 1
            try:
                with open(path, encoding="UTF-8", errors="surrogateescape") as fh:
                    text = fh.read()
            except OSError as exc:
                print(f"normalize-depfiles: WARN cannot read {path}: {exc}", file=sys.stderr)
                continue

            new_text, unfixable, shadowing = normalize_text(text, rel)
            n_shadow += len(shadowing)
            n_unfixable += len(unfixable)
            if shadowing and len(shadow_examples) < 5:
                shadow_examples.append((path, shadowing[0][0], shadowing[0][1]))
            if unfixable and len(unfixable_examples) < 5:
                unfixable_examples.append((path, unfixable[0]))

            if new_text != text:
                changed += 1
                if not args.check:
                    tmp = path + ".tmp"
                    with open(tmp, "w", encoding="UTF-8", errors="surrogateescape") as fh:
                        fh.write(new_text)
                    os.replace(tmp, path)

    if not args.quiet:
        verb = "would rewrite" if args.check else "rewrote"
        print(f"normalize-depfiles: scanned {scanned} depfile(s), {verb} {changed}")
        if n_shadow:
            print(f"  {n_shadow} prerequisite path(s) named a FOREIGN checkout "
                  f"and were re-pointed at this tree")

    if n_unfixable and not args.quiet:
        print(f"normalize-depfiles: NOTE {n_unfixable} out-of-tree absolute path(s) "
              f"left as-is (no local counterpart, not a staleness risk)")
        for dfile, p in unfixable_examples:
            print(f"    {dfile}: {p}")

    if args.check and changed:
        print("", file=sys.stderr)
        print(f"normalize-depfiles: FAIL — {changed} depfile(s) do not use "
              f"repo-relative paths.", file=sys.stderr)
        if n_shadow:
            print("", file=sys.stderr)
            print(f"  {n_shadow} of those prerequisites name ANOTHER checkout's copy of a", file=sys.stderr)
            print("  file that also exists here. Editing the local copy would invalidate", file=sys.stderr)
            print("  NOTHING — ninja would print \"no work to do\" and the build would", file=sys.stderr)
            print("  silently report a STALE result. Examples:", file=sys.stderr)
            print("", file=sys.stderr)
            for dfile, foreign, local in shadow_examples:
                print(f"    {dfile}\n        names  {foreign}\n        ours   {local}", file=sys.stderr)
        print("", file=sys.stderr)
        print("  Fix with:  python3 tools/normalize-depfiles.py", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
