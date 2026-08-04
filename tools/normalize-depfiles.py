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


def split_depfile(text):
    """Yield (leading_whitespace, path, trailing) for each prerequisite line.

    Line 0 is `target: firstprereq \\` and is emitted relative by ninja already;
    later lines are one prerequisite each. We only rewrite the path token.
    """
    for idx, raw in enumerate(text.split("\n")):
        line = raw.rstrip("\r")
        yield idx, line


def normalize_text(text, root):
    """Rewrite absolute prerequisites under `root` to root-relative form.

    Returns (new_text, remaining_absolute_paths).
    """
    prefix = root.rstrip("/") + "/"
    out_lines = []
    remaining = []
    for idx, line in split_depfile(text):
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
            normalized = os.path.normpath(path)
            if normalized.startswith(prefix):
                path = normalized[len(prefix) :]
            else:
                remaining.append(normalized)
                path = normalized
        out_lines.append("\t" + path + suffix)
    return "\n".join(out_lines), remaining


def classify(offenders, root):
    """Split remaining absolute paths into FATAL (shadowing) and benign.

    An absolute path is FATAL when the same repo-relative tail ALSO exists
    inside this build root — that is precisely the silent-staleness defect: the
    depfile names another checkout's copy of a file we have locally, so local
    edits to it invalidate nothing. A path with no local counterpart (a real
    system header, say) is merely unportable, not a correctness trap.
    """
    fatal, benign = [], []
    for path in offenders:
        tail = None
        # Find a plausible repo-relative tail: the longest suffix of the path
        # that exists inside our root.
        parts = path.strip("/").split("/")
        for i in range(len(parts)):
            candidate = "/".join(parts[i:])
            if os.path.exists(os.path.join(root, candidate)):
                tail = candidate
                break
        if tail and tail.startswith(("src/", "include/", "config/", "build/")):
            fatal.append((path, tail))
        else:
            benign.append(path)
    return fatal, benign


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

    scanned = changed = 0
    all_fatal = []
    all_benign = []

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

            new_text, remaining = normalize_text(text, root)
            fatal, benign = classify(remaining, root)
            all_fatal.extend((path, p, t) for p, t in fatal)
            all_benign.extend((path, p) for p in benign)

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

    if all_benign and not args.quiet:
        print(f"normalize-depfiles: NOTE {len(all_benign)} out-of-tree absolute "
              f"path(s) left as-is (no local counterpart, not a staleness risk)")
        for dfile, p in all_benign[:5]:
            print(f"    {dfile}: {p}")

    if all_fatal:
        print("", file=sys.stderr)
        print("normalize-depfiles: FATAL — depfile(s) reference a FOREIGN checkout.", file=sys.stderr)
        print("  These name another tree's copy of a file that also exists here, so", file=sys.stderr)
        print("  editing the local copy would invalidate NOTHING and the build would", file=sys.stderr)
        print("  silently report a stale result. Refusing to continue.", file=sys.stderr)
        print("", file=sys.stderr)
        for dfile, p, tail in all_fatal[:10]:
            print(f"    {dfile}\n        -> {p}\n        (local copy exists at {tail})", file=sys.stderr)
        if len(all_fatal) > 10:
            print(f"    ... and {len(all_fatal) - 10} more", file=sys.stderr)
        print("", file=sys.stderr)
        print("  Recover with:  find <build-dir> -name '*.d' -delete && tools/ninja-locked", file=sys.stderr)
        return 1

    if args.check and changed:
        print("", file=sys.stderr)
        print(f"normalize-depfiles: FAIL — {changed} depfile(s) still use absolute "
              f"in-tree paths.", file=sys.stderr)
        print("  Run: python3 tools/normalize-depfiles.py", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
