#!/usr/bin/env python3
"""Batched Xenon-side evidence extraction for the 30 sampled ACCEPT pairs.

Opens the analyzed rb3_xenon_default_xex.gzf (12.2-format, PowerPC:BE:64:Xenon)
READ-ONLY under the FORK Ghidra in ONE JVM session and, for each xenon_addr:
  - decompiles to pseudo-C
  - records byte size, callee names (resolved through our matches.json where the
    callee is a matched Wii function), referenced strings.

The gzf is imported (NOT analyzed — it already carries full analysis) into a
THROWAWAY project under /tmp/claude/ so the ghidriff project is never touched.

Env (set by the wrapper below):
  GHIDRA_INSTALL_DIR=/home/free/code/milohax/ghidra/build/ghidra   (fork, 12.2)
  GHIDRA_USER_HOME=/tmp/claude/ghidra_user_round2
  JAVA_HOME -> a JDK >= 21

Output: forensics/xenon_evidence.json  (pair_id -> {pseudo_c, size, callees, strings})
"""
from __future__ import annotations
import json, os, sys
from pathlib import Path

RB3 = Path('/home/free/code/milohax/rb3')
GZF = RB3 / 'build/SZBE69_B8/ghidra/ghidriff-xenon/rb3_xenon_default_xex.gzf'
MATCHES = RB3 / 'build/SZBE69_B8/ghidra/ghidriff-xenon/json/bank8_target.elf-42264e.gzf-rb3_xenon_default_xex.gzf.ghidriff.matches.json'
MANIFEST = RB3 / 'docs/decomp/xenon-hardening/round2/forensics/sample_manifest.json'
OUT = RB3 / 'docs/decomp/xenon-hardening/round2/forensics/xenon_evidence.json'
PROJ_DIR = Path('/tmp/claude/xenon_evidence_proj')
PROJ_NAME = 'xenon_ev'


def build_xenon_to_wii():
    """Map xenon addr (int) -> wii_name from matches.json, for callee resolution."""
    m = json.load(open(MATCHES))
    out = {}
    for e in m['function_matches']:
        p2 = e['p2_addr'].lower().replace('0x', '')
        try:
            a = int(p2, 16)
        except ValueError:
            continue
        out[a] = e.get('p1_name') or e.get('p2_name')
    return out


def main():
    manifest = json.load(open(MANIFEST))
    xen_to_wii = build_xenon_to_wii()

    import pyghidra
    pyghidra.start()

    from ghidra.app.decompiler import DecompInterface
    from ghidra.util.task import ConsoleTaskMonitor
    from ghidra.app.util.importer import AutoImporter, MessageLog
    from ghidra.framework.model import DomainFolder  # noqa: F401
    from java.io import File as JFile

    PROJ_DIR.mkdir(parents=True, exist_ok=True)

    # Use the headless project + importFile machinery to load the packed gzf.
    from ghidra.base.project import GhidraProject
    monitor = ConsoleTaskMonitor()

    # Open or create a throwaway Ghidra project, import the gzf as a packed DB.
    project = GhidraProject.createProject(str(PROJ_DIR), PROJ_NAME, False)
    try:
        gzf_file = JFile(str(GZF))
        # importProgram on a packed .gzf restores the analyzed DB verbatim.
        program = project.importProgram(gzf_file)
        if program is None:
            print('ERROR: importProgram returned None', file=sys.stderr)
            return 2

        af = program.getAddressFactory().getDefaultAddressSpace()
        fm = program.getFunctionManager()
        listing = program.getListing()
        ref_mgr = program.getReferenceManager()

        decomp = DecompInterface()
        decomp.openProgram(program)

        results = {}
        for entry in manifest:
            pid = entry['pair_id']
            xaddr_s = entry['xenon_addr'].lower().replace('0x', '')
            xa = int(xaddr_s, 16)
            addr = af.getAddress(xa)
            func = fm.getFunctionAt(addr)
            rec = {
                'pair_id': pid,
                'xenon_addr': entry['xenon_addr'],
                'wii_symbol': entry['wii_symbol'],
                'found': func is not None,
            }
            if func is None:
                # try containing function
                func = fm.getFunctionContaining(addr)
                rec['found_containing'] = func is not None
            if func is None:
                rec['error'] = 'no function at/containing addr'
                results[pid] = rec
                print(f'{pid} NO FUNC @ {entry["xenon_addr"]}', file=sys.stderr)
                continue

            body = func.getBody()
            rec['size_bytes'] = int(body.getNumAddresses())
            rec['xenon_func_name'] = str(func.getName())
            rec['entry_addr'] = '0x' + str(func.getEntryPoint())

            # Callees: iterate called functions; resolve to Wii name via matches.
            callees = []
            seen = set()
            for callee in func.getCalledFunctions(monitor):
                ca = callee.getEntryPoint().getOffset() & 0xffffffffffffffff
                ca32 = ca & 0xffffffff
                wii = xen_to_wii.get(ca32) or xen_to_wii.get(ca)
                nm = str(callee.getName())
                key = (ca, nm)
                if key in seen:
                    continue
                seen.add(key)
                callees.append({
                    'addr': '0x%08x' % ca32,
                    'xenon_name': nm,
                    'wii_match': wii if (wii and not wii.startswith('Function_')) else None,
                })
            rec['callees'] = callees
            rec['n_callees'] = len(callees)

            # Referenced strings: scan instructions in body for data refs to defined strings.
            strings = []
            seen_s = set()
            ai = listing.getInstructions(body, True)
            while ai.hasNext():
                ins = ai.next()
                for ref in ins.getReferencesFrom():
                    if not ref.getReferenceType().isData():
                        continue
                    to = ref.getToAddress()
                    data = listing.getDataAt(to)
                    if data is None:
                        continue
                    if data.hasStringValue():
                        try:
                            sval = str(data.getValue())
                        except Exception:
                            sval = None
                        if sval and sval not in seen_s:
                            seen_s.add(sval)
                            strings.append(sval)
            rec['strings'] = strings[:40]
            rec['n_strings'] = len(strings)

            # Decompile
            res = decomp.decompileFunction(func, 60, monitor)
            if res.decompileCompleted():
                rec['pseudo_c'] = res.getDecompiledFunction().getC()
            else:
                rec['pseudo_c'] = None
                rec['decomp_error'] = str(res.getErrorMessage())

            results[pid] = rec
            print(f'{pid} OK {entry["xenon_addr"]} size={rec.get("size_bytes")} '
                  f'callees={rec.get("n_callees")} strings={rec.get("n_strings")}',
                  file=sys.stderr)

        json.dump(results, open(OUT, 'w'), indent=2)
        print(f'wrote {OUT} ({len(results)} pairs)', file=sys.stderr)
    finally:
        project.close()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
