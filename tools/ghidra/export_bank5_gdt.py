#!/usr/bin/env python3
"""Export the Bank 5 program's entire DataTypeManager to a packed ``.gdt`` archive.

Step 1 of the two-step fix for the struct-type degradation in the signature port.

Background — the degradation
----------------------------
``port_dwarf_types.py`` applies Bank 5's typed function **signatures** onto Bank 8
via ``ApplyFunctionSignatureCmd(entry, sig, IMPORTED)`` (the 3-arg ctor →
``DEFAULT_HANDLER``, ``applyEmptyComposites=false``). In that path the param/return
DataTypes are *not* pre-resolved by the command (``prepareDataType`` only resolves
when a non-default handler is supplied); instead each gets resolved lazily into
Bank 8's DTM by ``FunctionDB.updateFunction`` → ``getDataTypeManager().resolve(dt, null)``
(``DataTypeManagerDB`` line ~1243, ``null`` ⇒ ``DEFAULT_HANDLER`` = RENAME_AND_ADD).

Why "ObjectDir *" decoded fine in isolation but became "undefined *" in bulk:
``resolve()`` pulls in a referenced struct (e.g. ``ObjectDir``) *transitively*. When a
function is resolved while ``ObjectDir`` is only reachable as a **forward declaration /
not-yet-defined composite** (``Composite.isNotYetDefined()``) — which happens during a
26 k-signature bulk run where a pointer-to-X signature lands before X's own full
definition is dragged in, or where an earlier ``.conflict`` empty stub already sits in
the DTM — ``resolveNoEquivalentFound`` finds a same-name existing type that is **not
equivalent** and, under ``DEFAULT_HANDLER``, returns ``RENAME_AND_ADD``: it keeps the
empty/forward stub as ``ObjectDir`` and adds the full one as ``ObjectDir.conflict``.
The signature's pointer still references the *empty* ``ObjectDir`` (0-length / no
members) → the decompiler prints ``undefined *``. Run a single function in isolation
and the full ``ObjectDir`` is the first (and only) one resolved, so it wins → ``ObjectDir *``.

The fix (this script + apply_types_from_gdt.py)
-----------------------------------------------
Pre-seed Bank 8's DTM with the **complete** Bank 5 type catalog *before* any signature
is applied, using a conflict handler that lets a full definition replace an empty stub
(``REPLACE_EMPTY_STRUCTS_OR_RENAME_AND_ADD_HANDLER``). Then every later
``ApplyFunctionSignatureCmd`` finds an already-fully-defined ``ObjectDir`` and its
``resolve()`` returns that equivalent type (no conflict, no ``.conflict`` stub) — so
pointers decode as ``ObjectDir *``.

This script does the export half: open Bank 5 read-only and serialize its **entire**
``DataTypeManager`` (every category + datatype, including globals-only / unreferenced
types that a signature-only port would never drag across) into a portable ``.gdt`` file.

Run with the pyghidra-mcp service STOPPED (it holds the project lock):
    tools/ghidra/pyghidra-service.sh stop
    cd ../pyghidra-mcp && JAVA_HOME=/usr/lib/jvm/java-17-openjdk \\
        GHIDRA_INSTALL_DIR=/opt/ghidra GHIDRA_USER_HOME=/tmp/claude/ghidra_user_bank8 \\
        uv run --python 3.10 --project . \\
        python ../rb3/tools/ghidra/export_bank5_gdt.py
    tools/ghidra/pyghidra-service.sh start
"""
from __future__ import annotations

import sys
from pathlib import Path

RB3 = Path(__file__).resolve().parents[2]
PROJ_LOC = str(RB3 / "ghidra_projects" / "RB3" / "RB3")
PROJ_NAME = "RB3"
# FileDataTypeManager requires the filename to end with the archive suffix (.gdt).
GDT_OUT = RB3 / "build" / "SZBE69_B8" / "ghidra" / "bank5_types.gdt"


def main() -> int:
    GDT_OUT.parent.mkdir(parents=True, exist_ok=True)
    # createFileArchive throws DuplicateFileException if the .gdt already exists, so
    # clear a stale one first (a re-export should be idempotent).
    if GDT_OUT.exists():
        GDT_OUT.unlink()

    import pyghidra

    pyghidra.start()
    from ghidra.base.project import GhidraProject
    from ghidra.program.model.data import (
        DataTypeConflictHandler,
        FileDataTypeManager,
    )
    from ghidra.util.task import ConsoleTaskMonitor
    from java.io import File
    from java.util import ArrayList

    project = GhidraProject.openProject(PROJ_LOC, PROJ_NAME, True)
    b5 = None
    for df in project.getRootFolder().getFiles():
        n = df.getName()
        # Bank 5 = band_r_wii (DWARF); Bank 8 = bank8_target. Open Bank 5 read-only.
        if "band_r_wii" in n:
            b5 = project.openProgram("/", n, True)
            break
    if b5 is None:
        print("error: band_r_wii (Bank 5) program not found in project", file=sys.stderr)
        project.close()
        return 2

    monitor = ConsoleTaskMonitor()
    src = b5.getDataTypeManager()

    # Match the archive's data organization (pointer size, alignment, endianness) to
    # the program it came from so struct layouts/offsets are preserved exactly. We take
    # language + compiler spec straight from Bank 5 rather than hardcoding
    # PowerPC:BE:32:Gekko_Broadway, so the archive can never drift from the program.
    lang = b5.getLanguage()
    cspec_id = b5.getCompilerSpec().getCompilerSpecID()
    print(
        f"[gdt] exporting Bank 5 DTM '{src.getName()}': "
        f"{src.getDataTypeCount(True)} datatypes, {src.getCategoryCount()} categories "
        f"-> {GDT_OUT}  (lang={lang.getLanguageID()} cspec={cspec_id})",
        file=sys.stderr,
    )

    out = FileDataTypeManager.createFileArchive(File(str(GDT_OUT)), lang.getLanguageID(), cspec_id)

    # Drain every datatype from Bank 5's DTM into a Java collection, then add them in
    # one batch. addDataTypes() caches equivalence across the batch (faster + more
    # consistent than per-type resolve) and, crucially, resolves each type's *component*
    # dependencies against the same growing archive — so a struct and the structs it
    # contains end up as one coherent definition set, not a sea of forward stubs.
    all_types = ArrayList()
    it = src.getAllDataTypes()
    while it.hasNext():
        all_types.add(it.next())
    print(f"[gdt] collected {all_types.size()} top-level datatypes", file=sys.stderr)

    tx = out.startTransaction("export Bank 5 datatypes")
    added = False
    n_add = n_skip = 0
    try:
        # Per-type add with skip-on-error. The bulk addDataTypes() aborts the whole
        # export if ANY type has a broken dependency, and Bank 5's DWARF import carries
        # a few malformed .conflict artifacts (e.g. _DOC_RootDO -> DOClassTemplate.conflict1,
        # "Invalid structure"). Adding one at a time isolates those few bad apples; the
        # 33k good types (incl. ObjectDir etc.) still land. REPLACE so a full definition
        # wins over any forward stub resolved earlier as a dependency.
        handler = DataTypeConflictHandler.REPLACE_HANDLER
        n = all_types.size()
        for idx in range(n):
            try:
                out.addDataType(all_types.get(idx), handler)
                n_add += 1
            except Exception:
                n_skip += 1
            if (idx + 1) % 5000 == 0:
                print(f"[gdt] added {n_add} skipped {n_skip} ({idx + 1}/{n})", file=sys.stderr)
        added = True
    finally:
        out.endTransaction(tx, added)
    print(f"[gdt] add complete: added={n_add} skipped={n_skip}", file=sys.stderr)

    out.save()
    n_out = out.getDataTypeCount(True)
    n_cat = out.getCategoryCount()
    out.close()
    print(
        f"[gdt] wrote {GDT_OUT}  ({n_out} datatypes, {n_cat} categories)",
        file=sys.stderr,
    )
    project.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
