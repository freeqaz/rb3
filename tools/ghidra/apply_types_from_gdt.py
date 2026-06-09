#!/usr/bin/env python3
"""Pre-seed Bank 8's DataTypeManager with Bank 5's full type catalog from a ``.gdt``.

Step 2 of the two-step fix for the struct-type degradation (see export_bank5_gdt.py
for step 1 and the full root-cause). This **must run before** the signature port
(``port_dwarf_types.py --all``) so that every later ``ApplyFunctionSignatureCmd``
references already-fully-defined struct types and can never fall back to ``undefined *``.

What this does and WHY it fixes the degradation
-----------------------------------------------
It opens ``build/SZBE69_B8/ghidra/bank5_types.gdt`` (read-only) and resolves every
datatype in it into ``bank8_target.getDataTypeManager()`` using
``DataTypeConflictHandler.REPLACE_EMPTY_STRUCTS_OR_RENAME_AND_ADD_HANDLER``.

The degradation in the original bulk port happened because, under the default
RENAME_AND_ADD handler, a struct that was already present in Bank 8's DTM as an
**empty / forward-declared / not-yet-defined** composite (``Composite.isNotYetDefined()``
— e.g. an earlier ``ObjectDir.conflict`` stub created when a pointer-to-ObjectDir
signature was resolved before ObjectDir's own full body was dragged in) is treated as a
name conflict: the *full* incoming definition is renamed to ``ObjectDir.conflict`` and
the **empty** one keeps the name ``ObjectDir``. The signature's ``ObjectDir *`` param
then points at a 0-length/member-less type → decompiles as ``undefined *``.

``REPLACE_EMPTY_STRUCTS_OR_RENAME_AND_ADD_HANDLER`` resolves that conflict the other way
(``DataTypeConflictHandler.java`` ~line 184): when the existing composite
``isNotYetDefined()`` it returns ``REPLACE_EXISTING`` — the full Bank 5 definition (with
members) overwrites the empty stub in place, **keeping the canonical name**. So after
this pass Bank 8's DTM holds the real, fully-defined ``ObjectDir`` under its proper name.
When ``ApplyFunctionSignatureCmd`` later resolves an ``ObjectDir *`` param, the DTM finds
the existing **equivalent** full type and reuses it (``resolveDataTypeWithSource`` returns
the existing equivalent — no conflict, no ``.conflict`` rename) → the pointer keeps a real
``ObjectDir *``. It also pulls in globals-only / unreferenced types a signature-only port
never touches, giving one consistent catalog for later manual struct work.

Note the apply order within a packed-DB resolve: each composite's own components are
resolved against the same DTM, and ``getSubsequentHandler()`` on this handler returns
*itself* (unlike DEFAULT_HANDLER, whose subsequent handler still RENAME_AND_ADDs), so the
empty-replace policy holds for the whole dependency closure, not just the top-level type.

Run with the pyghidra-mcp service STOPPED (it holds the project lock):
    tools/ghidra/pyghidra-service.sh stop
    cd ../pyghidra-mcp && JAVA_HOME=/usr/lib/jvm/java-17-openjdk \\
        GHIDRA_INSTALL_DIR=/opt/ghidra GHIDRA_USER_HOME=/tmp/claude/ghidra_user_bank8 \\
        uv run --python 3.10 --project . \\
        python ../rb3/tools/ghidra/apply_types_from_gdt.py
    # then run the signature port:
    #   python ../rb3/tools/ghidra/port_dwarf_types.py --all
    tools/ghidra/pyghidra-service.sh start
"""
from __future__ import annotations

import sys
from pathlib import Path

RB3 = Path(__file__).resolve().parents[2]
PROJ_LOC = str(RB3 / "ghidra_projects" / "RB3" / "RB3")
PROJ_NAME = "RB3"
GDT_IN = RB3 / "build" / "SZBE69_B8" / "ghidra" / "bank5_types.gdt"


def seed_bank8_types(b8, monitor) -> int:
    """Resolve every type from bank5_types.gdt into b8's DTM. Returns count resolved.

    Importable by port_dwarf_types.py (its --gdt path) so the pre-seed and the
    signature apply can share one open program + transaction.
    """
    from ghidra.program.model.data import (
        DataTypeConflictHandler,
        FileDataTypeManager,
    )
    from java.io import File

    if not GDT_IN.exists():
        raise SystemExit(
            f"error: {GDT_IN} not found — run export_bank5_gdt.py first"
        )

    # Open the archive read-only (openForUpdate=False ⇒ IMMUTABLE; we only read it).
    archive = FileDataTypeManager.openFileArchive(File(str(GDT_IN)), False)
    try:
        dst = b8.getDataTypeManager()
        # Full-definition-wins handler: a non-empty composite replaces an existing
        # empty/forward stub of the same name; everything else RENAME_AND_ADDs. This
        # is exactly the policy that prevents struct params decoding as undefined*.
        handler = DataTypeConflictHandler.REPLACE_EMPTY_STRUCTS_OR_RENAME_AND_ADD_HANDLER

        before = dst.getDataTypeCount(True)
        n = skipped = 0
        it = archive.getAllDataTypes()
        # Per-type resolve with skip-on-error: a few Bank 5 DWARF .conflict artifacts
        # (e.g. _DOC_RootDO containing the malformed DOClassTemplate.conflict1) throw
        # DataTypeDependencyException; isolate them so the 32k good types still seed.
        while it.hasNext():
            dt = it.next()
            try:
                dst.resolve(dt, handler)
                n += 1
            except Exception:
                skipped += 1
            if monitor is not None and (n + skipped) % 4000 == 0:
                print(f"[gdt-apply] resolved {n} skipped {skipped}...", file=sys.stderr)
        after = dst.getDataTypeCount(True)
        print(
            f"[gdt-apply] resolved {n} (skipped {skipped}) archive types into Bank 8 DTM "
            f"(count {before} -> {after})",
            file=sys.stderr,
        )
        return n
    finally:
        archive.close()


def main() -> int:
    import pyghidra

    pyghidra.start()
    from ghidra.base.project import GhidraProject
    from ghidra.util.task import ConsoleTaskMonitor

    project = GhidraProject.openProject(PROJ_LOC, PROJ_NAME, True)
    b8 = None
    for df in project.getRootFolder().getFiles():
        if "bank8_target" in df.getName():
            b8 = project.openProgram("/", df.getName(), False)  # writable
            break
    if b8 is None:
        print("error: bank8_target (Bank 8) program not found in project", file=sys.stderr)
        project.close()
        return 2

    monitor = ConsoleTaskMonitor()
    tx = b8.startTransaction("pre-seed Bank 5 types from gdt")
    ok = False
    try:
        seed_bank8_types(b8, monitor)
        ok = True
    finally:
        b8.endTransaction(tx, ok)

    if ok:
        project.save(b8)
        print("[gdt-apply] saved bank8_target program", file=sys.stderr)
    project.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
