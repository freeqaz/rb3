#!/usr/bin/env python3

###
# Transforms .d files, converting Windows paths to Unix paths.
# Allows usage of the mwcc -MMD flag on platforms other than Windows.
#
# Usage:
#   python3 tools/transform_dep.py build/src/file.d build/src/file.d
#
# Upstream: https://github.com/encounter/dtk-template
#
# LOCAL DIVERGENCE FROM UPSTREAM (2026-08-04) — repo-relative prerequisites.
# ---------------------------------------------------------------------------
# mwcceppc under wibo reports every #include'd header by ABSOLUTE path
# (`Z:\home\...\src\system\math\Mtx.h`). Upstream simply un-Windows-ifies that,
# leaving an absolute path that names ONE specific checkout of the repo.
#
# That is fine for a single tree, but `tools/setup-worktree.sh` reflink-copies
# the main repo's build/ into a new worktree for a warm object cache. Those
# copied .d files then point every header dependency back at the MAIN repo, so
# editing a header INSIDE the worktree invalidates nothing: ninja finds all
# dependencies satisfied against main's copy, prints "no work to do", and the
# subsequent objdiff returns a STALE, IDENTICAL result. The failure is silent —
# it reports NO CHANGE, which is indistinguishable from a real negative result.
#
# Fix: emit prerequisites RELATIVE to the build root (ninja's cwd) whenever they
# live inside it. Ninja resolves depfile paths against its own working
# directory, so a relative prerequisite automatically means "this tree's copy"
# in whichever tree the file is read from. Depfiles become location-independent,
# and a reflinked build cache stays correct in every worktree at zero cost.
#
# Paths genuinely OUTSIDE the build root are left absolute (there are none in
# this repo today; see tools/normalize-depfiles.py --check, which enforces it).
###

import argparse
import os
from platform import uname

wineprefix = os.path.join(os.environ["HOME"], ".wine")
if "WINEPREFIX" in os.environ:
    wineprefix = os.environ["WINEPREFIX"]
winedevices = os.path.join(wineprefix, "dosdevices")


def in_wsl() -> bool:
    return "microsoft-standard" in uname().release


def make_relative(path: str, root: str) -> str:
    """Return `path` relative to `root` if it lives inside it, else unchanged.

    Only ever strips a prefix — never emits a `../` escape — so a path outside
    the build root stays absolute rather than turning into a fragile traversal.
    """
    if not path.startswith("/"):
        return path  # already relative
    normalized = os.path.normpath(path)
    prefix = root.rstrip("/") + "/"
    if normalized.startswith(prefix):
        return normalized[len(prefix) :]
    return normalized


def convert_path(path: str) -> str:
    """Convert one mwcc/wine-emitted path to a Unix path."""
    # lowercase drive letter
    path = path[0].lower() + path[1:]
    if path[0] == "z":
        # shortcut for z:
        return path[2:].replace("\\", "/")
    if in_wsl():
        path = path[0:1] + path[2:]
        return os.path.join("/mnt", path.replace("\\", "/"))
    # use $WINEPREFIX/dosdevices to resolve path
    return os.path.realpath(os.path.join(winedevices, path.replace("\\", "/")))


def import_d_file(in_file: str, root: str) -> str:
    out_text = ""

    with open(in_file) as file:
        for idx, line in enumerate(file):
            # Tolerate CRLF: mwcc emits \r\n, and the upstream ` \\\n` suffix
            # test silently fails to match a `\r`-terminated line.
            line = line.rstrip("\r\n")
            if idx == 0:
                # "build/x.o: src/x.cpp \" — target and first prerequisite are
                # already emitted relative by ninja; just un-Windows-ify them.
                if line.endswith(" \\"):
                    out_text += line[:-2].replace("\\", "/") + " \\\n"
                else:
                    out_text += line.replace("\\", "/") + "\n"
            else:
                suffix = ""
                if line.endswith(" \\"):
                    suffix = " \\"
                    path = line.lstrip()[:-2]
                else:
                    path = line.strip()
                if not path:
                    continue
                path = make_relative(convert_path(path), root)
                out_text += "\t" + path + suffix + "\n"

    return out_text


def main() -> None:
    parser = argparse.ArgumentParser(
        description="""Transform a .d file from Wine paths to normal paths"""
    )
    parser.add_argument(
        "d_file",
        help="""Dependency file in""",
    )
    parser.add_argument(
        "d_file_out",
        help="""Dependency file out""",
    )
    parser.add_argument(
        "--root",
        default=None,
        help="""Build root that prerequisites are made relative to.
                Defaults to the current working directory, which is the
                directory ninja runs build commands from.""",
    )
    args = parser.parse_args()

    root = os.path.abspath(args.root) if args.root else os.getcwd()
    output = import_d_file(args.d_file, root)

    with open(args.d_file_out, "w", encoding="UTF-8") as f:
        f.write(output)


if __name__ == "__main__":
    main()
