tools/upstream — not this fork's work
=====================================

Everything in this directory came from upstream
[DarkRTA/rb3](https://github.com/DarkRTA/rb3) or from third parties. None of
it was written by this fork, and **none of it is covered by the CC0 waiver in
the repository root `LICENSE`**.

The parent `tools/` directory is this fork's own work and is CC0. This
directory is the exception, split out so the boundary is a fact about the
layout rather than a list somebody has to maintain by hand.

See the repository root `NOTICE`, sections 2 and 3.

Known third-party licenses in here
----------------------------------

  ninja_syntax.py    Apache 2.0, Copyright 2011 Google Inc.
  decompctx.py       Borrowed from the ac-decomp project (see its header)

Everything else is upstream-authored: the decomp-toolkit build glue
(`project.py`, `defines_common.py`, `download_tool.py`, `transform_dep.py`,
`splits_*.py`, `split_dwarf_dump.py`, `upload_progress.py`, `diff_sym.sh`),
plus the `batch-demangle`, `lex-tester` and `ghidra-scripts` subdirectories.

If you add a file here
----------------------

Only add code you did not write. Anything this fork authors belongs in the
parent `tools/`, where the CC0 applies. Getting this backwards is how the
NOTICE goes stale.
