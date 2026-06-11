#!/usr/bin/env python3
"""Extract Wii target asm bodies for the 30 sampled pairs.

For each sampled wii_symbol, finds the .s file containing `.fn <symbol>, ...`
and extracts the body up to the matching `.endfn <symbol>`. Writes per-pair
asm to forensics/wii_asm/<pair_id>.s and records the .s path + line span.
"""
import json, os, re, subprocess, sys
from pathlib import Path

ROOT = Path('/home/free/code/milohax/rb3')
ASM = ROOT / 'build/SZBE69_B8/asm'
OUT = ROOT / 'docs/decomp/xenon-hardening/round2/forensics/wii_asm'
OUT.mkdir(parents=True, exist_ok=True)

manifest = json.load(open(ROOT / 'docs/decomp/xenon-hardening/round2/forensics/sample_manifest.json'))

def find_s_file(symbol):
    # rg -l for literal .fn <symbol>,
    # symbol may contain regex-special chars; use fixed string
    needle = f'.fn {symbol},'
    r = subprocess.run(['rg', '-l', '-F', needle, str(ASM)],
                       capture_output=True, text=True)
    files = [l for l in r.stdout.splitlines() if l.strip()]
    return files

def extract(path, symbol):
    lines = Path(path).read_text(errors='replace').splitlines()
    start = None
    for i, l in enumerate(lines):
        if l.startswith(f'.fn {symbol},'):
            start = i
            break
    if start is None:
        return None, None, None
    end = None
    for j in range(start+1, len(lines)):
        if lines[j].startswith(f'.endfn {symbol}'):
            end = j
            break
    if end is None:
        end = len(lines)-1
    # include a preceding comment line if present (the # ClassName::method())
    pre = start
    if start > 0 and lines[start-1].lstrip().startswith('#'):
        pre = start-1
    body = '\n'.join(lines[pre:end+1])
    return body, start+1, end+1

results = []
for m in manifest:
    sym = m['wii_symbol']
    files = find_s_file(sym)
    if not files:
        results.append({**{k:m[k] for k in ('pair_id','wii_symbol')}, 's_file': None, 'lines':0})
        print(m['pair_id'], 'NOT FOUND', sym[:50])
        continue
    path = files[0]
    body, lo, hi = extract(path, sym)
    rel = os.path.relpath(path, ROOT)
    nlines = body.count('\n')+1 if body else 0
    (OUT / f"{m['pair_id']}.s").write_text(body or '')
    results.append({'pair_id': m['pair_id'], 'wii_symbol': sym, 's_file': rel,
                    'line_span': f'{lo}-{hi}', 'lines': nlines, 'multi_files': len(files)})
    print(m['pair_id'], rel, f'{lo}-{hi}', f'{nlines}L', ('MULTI!' if len(files)>1 else ''))

json.dump(results, open(OUT / 'wii_asm_index.json','w'), indent=2)
print('done', len(results))
