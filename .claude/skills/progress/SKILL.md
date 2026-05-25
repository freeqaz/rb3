---
name: progress
description: Get decomp progress summary. Shows match statistics from the build report, per-category breakdowns, and identifies areas with the most remaining work. Use to check overall project status or find targets for decomp work.
argument-hint: ""
allowed-tools: Bash(tools/ninja-locked *), Bash(ninja *), Bash(python3 *), Bash(jq *), Read, Grep, Glob
---

# Progress Skill

Show RB3 decomp progress. Categories come from `report.json` itself (driven by `configure.py`'s per-object `progress_category` — see `configure.py`'s Wii-only override).

## Steps

1. **Refresh the report** (serialized via flock):
   ```bash
   tools/ninja-locked build/SZBE69_B8/report.json
   ```

2. **Show per-category summary, then port-scope rollup:**
   ```bash
   python3 -c "
   import json
   from pathlib import Path

   data = json.loads(Path('build/SZBE69_B8/report.json').read_text())
   cats = data.get('categories', [])

   print(f'{\"Cat ID\":<12} {\"Name\":<32} {\"Fn done/total\":<15} {\"Fn%\":<7} {\"Fuzzy%\":<8} {\"Linked%\":<8} {\"Remain B\":>10}')
   print('-' * 100)
   for c in cats:
       m = c.get('measures', {})
       f_done = m.get('matched_functions', 0)
       f_tot  = m.get('total_functions', 0)
       fpct   = m.get('matched_functions_percent', 0)
       fuzzy  = m.get('fuzzy_match_percent', 0)
       linked = m.get('matched_code_percent', 0)
       b_tot  = int(m.get('total_code', 0))
       b_done = int(m.get('matched_code', 0))
       remain = b_tot - b_done
       print(f'  {c[\"id\"]:<10} {c[\"name\"]:<32} {f_done:>5}/{f_tot:<5}   {fpct:>5.1f}% {fuzzy:>6.2f}%  {linked:>5.1f}%  {remain:>10,}')

   # In-scope rollup: game + engine (the port surface). Wii-only is excluded
   # because it gets replaced in the native port.
   IN_SCOPE = ('game', 'engine')
   ic = {'fn_done':0,'fn_tot':0,'b_done':0,'b_tot':0,'fuzzy_b':0.0}
   for c in cats:
       if c['id'] not in IN_SCOPE: continue
       m = c['measures']
       ic['fn_done'] += m['matched_functions']
       ic['fn_tot']  += m['total_functions']
       ic['b_done']  += int(m['matched_code'])
       ic['b_tot']   += int(m['total_code'])
       ic['fuzzy_b'] += int(m['total_code']) * m['fuzzy_match_percent'] / 100.0
   print()
   print(f'IN-SCOPE TOTAL (game + engine, port surface):')
   print(f'  Fns matched: {ic[\"fn_done\"]:,} / {ic[\"fn_tot\"]:,} ({100*ic[\"fn_done\"]/ic[\"fn_tot\"]:.2f}%)')
   print(f'  Bytes fuzzy: {100*ic[\"fuzzy_b\"]/ic[\"b_tot\"]:.2f}%   linked-100%: {100*ic[\"b_done\"]/ic[\"b_tot\"]:.2f}%   remain: {ic[\"b_tot\"]-ic[\"b_done\"]:,} B')
   "
   ```

3. **Present the results.** Categories of interest:
   - `game` / `engine` — the port surface (what we care about reaching 100%).
   - `game_wii` / `engine_wii` — Wii-only platform glue (rndwii, os, synthwii, usbwii, `*_Wii.cpp`, `band3/meta_band/Wii*`). Replaced in the native port; lower priority.
   - `network` / `sdk` / `lib` — out of scope per CLAUDE.md (replaced by host platform / modern netcode).

## Finding Work Targets

```bash
# Units with the most remaining work in the engine
python3 scripts/dc3_compare.py --units --filter system/

# Functions where DC3 is at 100% but RB3 is not (porting opportunities)
python3 scripts/dc3_compare.py --filter system/char/ --sort rb3
```

## Tips

- Use `tools/ninja-locked`, not bare `ninja` — concurrent agents share the build dir.
- The report.json is regenerated as part of the build via objdiff-cli.
- If categories look stale, delete `build/SZBE69_B8/report.cache` and rebuild.
