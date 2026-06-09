#!/usr/bin/env python3
"""Find decomp STUBS — functions whose source body is empty/trivial yet the
target has real code, so they compile + link and masquerade as "done".

A stub is NOT the same as "not started":
  - MISSING : target symbol has NO body in our build (base_size==0 / absent).
              The normal "not decompiled yet" backlog (mostly Wii/SDK).
  - STUB    : a body EXISTS and links, but it's trivially short vs the target
              and matches poorly. These hide because they compile cleanly.
              (e.g. `void f() {}` placeholders that "unblock the link".)

The discriminator is base_size (what OUR build emitted), read from the base
object files — report.json only carries the target size + match%, which can't
tell a stub from a hard partial.

Usage:
  python3 scripts/analysis/find_stubs.py                 # in-scope STUBS, ranked
  python3 scripts/analysis/find_stubs.py --all-scope      # include sdk/network/wii
  python3 scripts/analysis/find_stubs.py --missing        # also list MISSING (0-body)
  python3 scripts/analysis/find_stubs.py --md OUT.md       # write a markdown report
  python3 scripts/analysis/find_stubs.py --update-db       # refresh decomp.db: ingest
                                                           # report.json + set is_stub
"""
import argparse, json, os, sys
from elftools.elf.elffile import ELFFile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REPORT = os.path.join(REPO, "build/SZBE69_B8/report.json")
OBJ_ROOT = os.path.join(REPO, "build/SZBE69_B8/src")

# A unit "main/system/rndwii/Mesh" -> base object build/SZBE69_B8/src/system/rndwii/Mesh.o
def unit_to_obj(unit):
    rel = unit[len("main/"):] if unit.startswith("main/") else unit
    return os.path.join(OBJ_ROOT, rel + ".o")

# Wii/SDK/network = replaced by the native port; not the stub backlog we care about.
def in_scope(unit):
    bad = ("main/sdk", "/rndwii", "/os/", "network", "DWC", "RVL_SDK", "/MSL",
           "/NW4R", "_Wii", "binkwii", "/lib/")
    return not any(b in unit for b in bad)

def func_sizes(opath):
    out = {}
    try:
        with open(opath, "rb") as f:
            st = ELFFile(f).get_section_by_name(".symtab")
            if st:
                for s in st.iter_symbols():
                    if s["st_info"]["type"] == "STT_FUNC" and s.name:
                        out[s.name] = s["st_size"]
    except FileNotFoundError:
        pass
    return out

def classify(target, base, pct):
    if base is None or base == 0:
        return "MISSING" if target > 0 else "EMPTY"
    if pct < 40 and target >= 40 and base <= max(20, 0.20 * target):
        return "STUB"
    if pct < 95:
        return "PARTIAL"
    return "DONE"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--all-scope", action="store_true", help="include sdk/network/wii")
    ap.add_argument("--missing", action="store_true", help="also list MISSING (0-body)")
    ap.add_argument("--md", help="write a markdown report to this path")
    ap.add_argument("--update-db", action="store_true",
                    help="refresh decomp.db: ingest report.json + set is_stub flags")
    args = ap.parse_args()

    rep = json.load(open(REPORT))
    rows = []
    cache = {}
    for u in rep.get("units", []):
        unit = u.get("name", "")
        obj = unit_to_obj(unit)
        if obj not in cache:
            cache[obj] = func_sizes(obj)
        sizes = cache[obj]
        for fn in u.get("functions", []):
            name = fn["name"]
            tgt = int(fn.get("size", 0))
            pct = fn.get("fuzzy_match_percent", 0.0)
            base = sizes.get(name)  # None == absent from our object
            kind = classify(tgt, base, pct)
            rows.append((kind, unit, name, tgt, base, pct,
                         fn.get("metadata", {}).get("demangled_name", name)))

    scoped = [r for r in rows if args.all_scope or in_scope(r[1])]
    stubs = sorted([r for r in scoped if r[0] == "STUB"], key=lambda r: -r[3])
    missing = sorted([r for r in scoped if r[0] == "MISSING"], key=lambda r: -r[3])

    n = {k: sum(1 for r in rows if r[0] == k) for k in
         ("STUB", "MISSING", "PARTIAL", "DONE", "EMPTY")}
    ns = {k: sum(1 for r in scoped if r[0] == k) for k in n}
    print(f"PROJECT total={len(rows)}  STUB={n['STUB']} MISSING={n['MISSING']} "
          f"PARTIAL={n['PARTIAL']} DONE={n['DONE']}")
    print(f"IN-SCOPE      STUB={ns['STUB']} MISSING={ns['MISSING']} "
          f"PARTIAL={ns['PARTIAL']} DONE={ns['DONE']}")
    scope_lbl = "ALL-SCOPE" if args.all_scope else "IN-SCOPE"
    print(f"\n=== {scope_lbl} STUBS (body exists + links, but empty/trivial vs target) ===")
    print(f"{'tgtB':>6} {'baseB':>6} {'pct':>6}  unit / symbol")
    for kind, unit, name, tgt, base, pct, dem in stubs:
        print(f"{tgt:6d} {base:6d} {pct:6.1f}  {unit}  {dem}")
    if not stubs:
        print("  (none)")

    if args.missing:
        print(f"\n=== {scope_lbl} MISSING (no body in our build — top 30 by size) ===")
        for kind, unit, name, tgt, base, pct, dem in missing[:30]:
            print(f"{tgt:6d}      -   0.0  {unit}  {dem}")
        print(f"  ... {len(missing)} total")

    if args.update_db:
        # A function "is a stub" for tracking = it has no real body in our build:
        # MISSING (absent / base_size 0) or STUB (trivial placeholder that links).
        # Both are the categories that masquerade as done or need writing.
        import sqlite3
        db = os.path.join(REPO, "decomp.db")
        # 1) refresh percentages/verdict from the freshly-built report.json
        sys.path.insert(0, os.path.join(REPO, "scripts"))
        from orchestrator.database import ingest_report
        stats = ingest_report(REPORT, db)
        print(f"\n[update-db] ingest_report: {stats}")
        # 2) recompute is_stub for every function (clear stale, set fresh)
        stub_syms = [r[2] for r in rows if r[0] in ("MISSING", "STUB")]
        conn = sqlite3.connect(db)
        conn.execute("UPDATE functions SET is_stub = 0")
        conn.executemany("UPDATE functions SET is_stub = 1 WHERE symbol = ?",
                         [(s,) for s in stub_syms])
        conn.commit()
        flagged = conn.execute("SELECT count(*) FROM functions WHERE is_stub=1").fetchone()[0]
        conn.close()
        print(f"[update-db] is_stub set on {flagged} functions "
              f"(MISSING {n['MISSING']} + STUB {n['STUB']})")

    if args.md:
        with open(args.md, "w") as f:
            f.write("# Decomp stub tracker\n\n")
            f.write("Auto-generated by `scripts/analysis/find_stubs.py`. A STUB is a "
                    "function whose body exists and links but is empty/trivial vs the "
                    "target (masquerades as done); MISSING has no body at all.\n\n")
            f.write(f"- project: STUB **{n['STUB']}**, MISSING **{n['MISSING']}**, "
                    f"PARTIAL {n['PARTIAL']}, DONE {n['DONE']}\n")
            f.write(f"- {scope_lbl}: STUB **{ns['STUB']}**, MISSING **{ns['MISSING']}**, "
                    f"PARTIAL {ns['PARTIAL']}, DONE {ns['DONE']}\n\n")
            f.write(f"## {scope_lbl} STUBS\n\n| target B | base B | match% | unit | symbol |\n")
            f.write("|--:|--:|--:|---|---|\n")
            for kind, unit, name, tgt, base, pct, dem in stubs:
                f.write(f"| {tgt} | {base} | {pct:.1f} | {unit} | `{dem}` |\n")
        print(f"\nwrote {args.md}")

if __name__ == "__main__":
    main()
