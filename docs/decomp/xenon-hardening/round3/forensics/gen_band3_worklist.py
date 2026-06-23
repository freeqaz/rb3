#!/usr/bin/env python3
"""Round-3 band3 porting-worklist generator (rb3-side forensics copy).

This is the self-verifying generator the task asked be kept under
`round3/forensics/`. The canonical, committed copy lives in the rb3-xenon repo at
`rb3-xenon/tools/gen_band3_port_worklist.py` (it must sit there to read
`ghidriff_identities.json` + `scripts/target_symbol_map.json` and write the
`band3_port_worklist.json` / `docs/plans/band3-port-worklist.md` outputs into that
repo). This file simply executes that canonical generator so there is one source
of truth.

What it does (see the canonical script for detail):
  1. Regenerates the NET-NEW band3 set = ghidriff ACCEPT identities (category band3)
     whose normalized Xenon `rb3_addr` is NOT a key in `target_symbol_map.json`.
     Self-verifies count == 232 across 93 TUs (exits non-zero otherwise).
  2. Per entry: rb3_addr, wii_addr_bank8, wii_symbol (CW/MWCC-mangled),
     wii_demangled (human-readable, via round2 demangle_cw.py), tu, src_path,
     match_type, confidence_label (high | bsim>=30 | bsim20-30 | bsim15-20), simconf,
     dc3_cannot_provide=True.
  3. Emits the JSON feed + the TU-ranked markdown checklist (rank = #high+#bsim>=30
     desc, then total desc).
  4. VERIFIES every wii_symbol resolves in the Wii CW map
     (orig/SZBE69_B8/files/band_r_wii.map) to its claimed Bank-8 addr, and that 0
     entries are already in target_symbol_map.json.

Run: python3 docs/decomp/xenon-hardening/round3/forensics/gen_band3_worklist.py
(cwd-independent; the canonical generator uses absolute repo paths.)
"""
import runpy

CANONICAL = "/home/free/code/milohax/rb3-xenon/tools/gen_band3_port_worklist.py"

if __name__ == "__main__":
    runpy.run_path(CANONICAL, run_name="__main__")
