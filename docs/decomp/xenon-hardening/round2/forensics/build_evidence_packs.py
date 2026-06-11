#!/usr/bin/env python3
"""Assemble per-pair evidence markdown for the 30 sampled ACCEPT identities.

Combines:
  - sample_manifest.json        (header: symbols, addrs, match_type, simconf, tu)
  - wii_asm/<pid>.s             (Wii Bank-8 target asm, ground truth)
  - wii_m2c/<pid>.c             (m2c pseudo-C, where available)
  - xenon_evidence.json         (Xenon pseudo-C + callees + strings)
Writes ../evidence/pair-<pid>.md  (self-contained for a judge with no context).
"""
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from demangle_cw import demangle

ROOT = Path('/home/free/code/milohax/rb3')
F = ROOT / 'docs/decomp/xenon-hardening/round2/forensics'
EV = ROOT / 'docs/decomp/xenon-hardening/round2/evidence'
EV.mkdir(parents=True, exist_ok=True)

WII_ASM_TRUNC = 150
XEN_C_TRUNC = 240
WII_M2C_TRUNC = 220

manifest = {m['pair_id']: m for m in json.load(open(F / 'sample_manifest.json'))}
wii_idx = {r['pair_id']: r for r in json.load(open(F / 'wii_asm/wii_asm_index.json'))}
m2c_idx = json.load(open(F / 'wii_m2c/m2c_index.json'))
xen = json.load(open(F / 'xenon_evidence.json'))


def truncate(text, n, label):
    lines = text.splitlines()
    if len(lines) <= n:
        return text
    head = '\n'.join(lines[:n])
    return head + f"\n... [truncated {len(lines)-n} of {len(lines)} {label} lines]"


def main():
    rows = []
    for pid in sorted(manifest):
        m = manifest[pid]
        x = xen.get(pid, {})
        wi = wii_idx.get(pid, {})
        readable = demangle(m['wii_symbol'])

        out = []
        out.append(f"# Pair {pid} — verification evidence")
        out.append("")
        out.append(f"**Claimed identity:** Wii `{m['wii_symbol']}`  ==  Xenon `{m['xenon_addr']}`")
        out.append("")
        out.append("| field | value |")
        out.append("|---|---|")
        out.append(f"| pair_id | {pid} |")
        out.append(f"| stratum | {m['stratum']} |")
        out.append(f"| match_type | `{m['match_type']}` |")
        sc = m['simconf']
        out.append(f"| BSim sim×conf | {sc if sc != -1 else 'n/a (non-BSim)'} |")
        if m.get('bsim'):
            b = m['bsim']
            out.append(f"| BSim similarity / confidence | {b.get('similarity')} / {b.get('confidence')} |")
        out.append(f"| TU (Wii) | `{m['tu']}` |")
        out.append(f"| Wii symbol (demangled) | `{readable}` |")
        out.append(f"| Wii addr (Bank 8) | `{m['wii_addr']}` |")
        out.append(f"| Xenon addr | `{m['xenon_addr']}` |")
        out.append(f"| Xenon func name | `{x.get('xenon_func_name','?')}` (stripped binary — name is auto-generated) |")
        if wi.get('s_file'):
            wii_loc = f"lines {wi.get('line_span','?')} in `{wi['s_file']}`"
        else:
            wii_loc = wi.get('line_span', 'llvm-objdump by-addr from bank8_target.elf')
        out.append(f"| Wii body size | {wi.get('lines','?')} asm lines ({wii_loc}) |")
        out.append(f"| Xenon body size | {x.get('size_bytes','?')} bytes |")
        out.append("")

        # --- judge guidance ---
        out.append("## How to read this")
        out.append("")
        out.append("The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon")
        out.append("function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.")
        out.append("The claim is that these two functions are the **same source function** compiled by")
        out.append("two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge")
        out.append("whether the *control-flow shape, constant pool, field offsets, and especially the")
        out.append("resolved callees* are consistent with that claim. The 'resolved callee' column")
        out.append("below maps each Xenon callee to its matched Wii symbol where a match exists —")
        out.append("**agreeing callee names are the strongest cross-compiler signal.**")
        out.append("")

        # --- Xenon callees ---
        callees = x.get('callees', [])
        resolved = [c for c in callees if c.get('wii_match')]
        out.append(f"## Xenon callees ({len(callees)} total, {len(resolved)} resolved to a matched Wii symbol)")
        out.append("")
        if callees:
            out.append("| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |")
            out.append("|---|---|---|")
            for c in callees:
                wm = c.get('wii_match') or ''
                wm_disp = f"`{wm}`" if wm else "_(unmatched / Function_)_"
                out.append(f"| `{c['addr']}` | `{c['xenon_name']}` | {wm_disp} |")
        else:
            out.append("_(no direct callees — leaf function)_")
        out.append("")

        # --- strings ---
        xstr = x.get('strings', [])
        out.append(f"## Referenced strings (Xenon side, {x.get('n_strings',0)})")
        out.append("")
        if xstr:
            for s in xstr:
                out.append(f"- `{s!r}`")
        else:
            out.append("_(none)_")
        out.append("")

        # --- Xenon pseudo-C ---
        out.append("## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)")
        out.append("")
        pc = x.get('pseudo_c')
        if pc:
            out.append("```c")
            out.append(truncate(pc, XEN_C_TRUNC, 'pseudo-C'))
            out.append("```")
        else:
            out.append(f"_(decompile failed: {x.get('decomp_error','?')})_")
        out.append("")

        # --- Wii m2c ---
        mrec = m2c_idx.get(pid, {})
        out.append("## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)")
        out.append("")
        if mrec.get('m2c'):
            m2c_txt = (F / 'wii_m2c' / mrec['m2c']).read_text()
            out.append("```c")
            out.append(truncate(m2c_txt, WII_M2C_TRUNC, 'm2c'))
            out.append("```")
        else:
            out.append(f"_(m2c not available: {mrec.get('reason','?')} — see raw asm below)_")
        out.append("")

        # --- Wii asm ---
        out.append("## Wii target asm (Bank 8, ground-truth body)")
        out.append("")
        asm_path = F / 'wii_asm' / f'{pid}.s'
        asm = asm_path.read_text() if asm_path.exists() else '(missing)'
        out.append("```asm")
        out.append(truncate(asm, WII_ASM_TRUNC, 'asm'))
        out.append("```")
        out.append("")

        (EV / f'pair-{pid}.md').write_text('\n'.join(out))
        rows.append({
            'pair_id': pid, 'wii_symbol': m['wii_symbol'], 'wii_addr': m['wii_addr'],
            'xenon_addr': m['xenon_addr'], 'match_type': m['match_type'],
            'simconf': sc, 'resolved_callees': len(resolved), 'total_callees': len(callees),
            'xenon_strings': x.get('n_strings', 0), 'xenon_size': x.get('size_bytes'),
        })
        print(f'pair-{pid}.md  callees={len(resolved)}/{len(callees)}  strings={x.get("n_strings",0)}')

    json.dump(rows, open(F / 'evidence_summary.json', 'w'), indent=2)
    print('done', len(rows))


if __name__ == '__main__':
    main()
