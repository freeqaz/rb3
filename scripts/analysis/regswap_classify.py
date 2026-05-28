#!/usr/bin/env python3
"""Classify callee-saved register swaps from cached objdiff JSON diffs.

Scans /tmp/claude/diff_*.json files (produced by run_objdiff) and:
1. Finds functions with callee-saved register swaps
2. Classifies each swap pair by variable type (param, addr, member load, etc.)
3. Estimates fixability (both user-controllable = maybe fixable)
4. Applies an IPA penalty: at ≥99% match, callee-saved register swaps in
   units compiled with ``-ipa file`` are TU-level scheduling decisions that
   intra-function declaration reorders cannot move. Such cases are demoted
   to ``fixable=False`` while preserving the raw verdict in ``fixable_raw``.
5. Reports most common swap pairs

Usage:
    python3 scripts/analysis/regswap_classify.py [--min-pct 90] [--verbose]
"""
import json
import glob
import os
import re
import argparse
from collections import Counter
from pathlib import Path


CALLEE_SAVED_GPR = set(f'r{i}' for i in range(14, 32))
CALLEE_SAVED_FPR = set(f'f{i}' for i in range(14, 32))

WRITE_POS0_OPS = {
    'mr', 'addi', 'subi', 'lwz', 'lbz', 'lhz', 'li', 'lis',
    'fmr', 'fmuls', 'fadds', 'fsubs', 'fdivs', 'frsp', 'fcfid',
    'lfs', 'lfd', 'extsw', 'slwi', 'clrlwi', 'rlwinm', 'add',
    'subf', 'mullw', 'neg', 'ori', 'oris', 'xori', 'andi.',
    'srwi',
    # Wii paired-single opcodes (Gekko/Broadway)
    'psq_l', 'psq_st',
    'ps_add', 'ps_sub', 'ps_mul', 'ps_div',
    'ps_madd', 'ps_msub', 'ps_nmadd', 'ps_nmsub',
    'ps_neg', 'ps_abs', 'ps_mr', 'ps_sel',
    'ps_merge00', 'ps_merge01', 'ps_merge10', 'ps_merge11',
}

USER_TYPES = {'param_save', 'fparam_save'}
TEMP_TYPES = {'member_load', 'addr_compute', 'global_addr', 'const', 'fcompute'}

# IPA penalty thresholds. At >=99% the swap is whole-TU IPA-locked; declaration
# reorders inside the function won't move it. 95-99% is a middle band where
# the swap MIGHT still be reachable from source but odds are low.
_IPA_LOCKED_PCT = 99.0
_IPA_PARTIAL_PCT = 95.0

# Repo paths — scripts/analysis/regswap_classify.py → repo root is two levels up.
_REPO_ROOT = Path(__file__).resolve().parent.parent.parent
_OBJDIFF_JSON = _REPO_ROOT / "objdiff.json"


def classify_def(opcode):
    if opcode == 'mr':
        return 'param_save'
    elif opcode in ('lwz', 'lbz', 'lhz', 'lfs', 'lfd'):
        return 'member_load'
    elif opcode in ('addi', 'subi'):
        return 'addr_compute'
    elif opcode == 'fmr':
        return 'fparam_save'
    elif opcode in ('fmuls', 'fadds', 'fsubs', 'fdivs', 'frsp', 'fcfid'):
        return 'fcompute'
    elif opcode == 'psq_l':
        return 'member_load'
    elif opcode in ('ps_add', 'ps_sub', 'ps_mul', 'ps_div',
                    'ps_madd', 'ps_msub', 'ps_nmadd', 'ps_nmsub',
                    'ps_neg', 'ps_abs', 'ps_mr', 'ps_sel',
                    'ps_merge00', 'ps_merge01', 'ps_merge10', 'ps_merge11'):
        return 'fcompute'
    elif opcode == 'li':
        return 'const'
    elif opcode == 'lis':
        return 'global_addr'
    else:
        return opcode


def find_swap_pairs(instrs):
    """Find all callee-saved register swap pairs."""
    pairs = set()
    for instr in instrs:
        if instr.get('match_type') != 'diff_arg':
            continue
        t_args = instr.get('target', {}).get('typed_args', [])
        b_args = instr.get('base', {}).get('typed_args', [])
        for ta, ba in zip(t_args, b_args):
            if ta.get('type') != 'Register' or ba.get('type') != 'Register':
                continue
            tr, br = ta['value'], ba['value']
            if tr == br:
                continue
            pair = tuple(sorted([tr, br]))
            both_cs = (pair[0] in CALLEE_SAVED_GPR and pair[1] in CALLEE_SAVED_GPR) or \
                      (pair[0] in CALLEE_SAVED_FPR and pair[1] in CALLEE_SAVED_FPR)
            if both_cs:
                pairs.add(pair)
    return pairs


def classify_pair(instrs, lo_reg, hi_reg):
    """Classify the defining opcode for each register in the target."""
    lo_type = hi_type = None
    lo_idx = hi_idx = None

    for i, instr in enumerate(instrs):
        t_args = instr.get('target', {}).get('typed_args', [])
        opcode = instr.get('target', {}).get('opcode', '')
        if t_args and t_args[0].get('type') == 'Register' and opcode in WRITE_POS0_OPS:
            r = t_args[0]['value']
            if r == lo_reg and lo_type is None:
                lo_type = classify_def(opcode)
                lo_idx = i
            if r == hi_reg and hi_type is None:
                hi_type = classify_def(opcode)
                hi_idx = i

    return lo_type, hi_type, lo_idx, hi_idx


def _load_unit_ipa_map():
    """Return (ipa_map, all_units) from ``objdiff.json``.

    ``ipa_map`` maps each unit name to a bool — True iff its ``c_flags``
    include ``-ipa file``. ``all_units`` is the full set of known unit
    names. Returns (empty, empty) if objdiff.json is missing.

    The diff-JSON ``unit`` field often uses a shorter form (e.g.
    ``meta_band/CalibrationPanel``) than objdiff's unit names (e.g.
    ``main/band3/meta_band/CalibrationPanel``); callers should use
    :func:`_resolve_unit` for matching.
    """
    ipa_map: dict[str, bool] = {}
    all_units: set[str] = set()
    if not _OBJDIFF_JSON.exists():
        return ipa_map, all_units
    try:
        with open(_OBJDIFF_JSON) as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError):
        return ipa_map, all_units
    for unit in data.get('units', []):
        name = unit.get('name', '')
        if not name:
            continue
        all_units.add(name)
        cflags = (unit.get('scratch') or {}).get('c_flags', '') or ''
        ipa_map[name] = '-ipa file' in cflags
    return ipa_map, all_units


def _resolve_unit(diff_unit: str, ipa_map: dict[str, bool]) -> str:
    """Resolve a diff-JSON ``unit`` field to a canonical objdiff unit name.

    The diff JSON's ``unit`` may be a suffix of the canonical name (the
    canonical form has a category prefix like ``main/band3/`` or
    ``default/system/``). Returns:
      - ``diff_unit`` if it is already a canonical name;
      - the unique canonical name that ends with ``/<diff_unit>`` otherwise;
      - ``""`` if no canonical match (treat as unknown).
    """
    if not diff_unit:
        return ""
    if diff_unit in ipa_map:
        return diff_unit
    needle = '/' + diff_unit
    matches = [n for n in ipa_map if n.endswith(needle)]
    if len(matches) == 1:
        return matches[0]
    # Ambiguous (multiple match) or no match — fall through to unknown.
    return ""


def _ipa_penalty_class(resolved_unit: str, pct: float,
                       ipa_map: dict[str, bool], ipa_known: bool) -> str:
    """Classify a function for IPA-locked register swap fixability.

    Returns one of:
      - ``none`` — unit has no ``-ipa file`` flag.
      - ``intra_function`` — pct < 95 in an ``-ipa file`` unit: structural
        fixes still plausible.
      - ``ipa_partial`` — 95 ≤ pct < 99 in an ``-ipa file`` unit: borderline.
      - ``ipa_locked`` — pct ≥ 99 in an ``-ipa file`` unit: whole-TU IPA
        decision; intra-function reorders cannot move callee-saved swaps.
      - ``unknown`` — objdiff.json missing, or diff's unit didn't resolve.
    """
    if not ipa_known:
        return 'unknown'
    if not resolved_unit:
        return 'unknown'
    if not ipa_map.get(resolved_unit, False):
        return 'none'
    if pct >= _IPA_LOCKED_PCT:
        return 'ipa_locked'
    if pct >= _IPA_PARTIAL_PCT:
        return 'ipa_partial'
    return 'intra_function'


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--min-pct', type=float, default=90.0,
                        help='Minimum match percentage (default: 90)')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Show per-function details')
    parser.add_argument('--diff-dir', default='/tmp/claude',
                        help='Directory containing diff_*.json files')
    parser.add_argument('--ipa-aware', dest='ipa_aware', action='store_true',
                        default=True,
                        help='Apply IPA penalty (demote fixable=False at >=99%% in -ipa file units). Default: ON.')
    parser.add_argument('--no-ipa-aware', dest='ipa_aware', action='store_false',
                        help='Disable IPA penalty (pre-tightening behavior).')
    parser.add_argument('--show-ipa-penalty', action='store_true',
                        help='Print the ipa_penalty_class field in verbose output.')
    parser.add_argument('--json', dest='json_output', action='store_true',
                        help='Emit per-function records as JSON to stdout.')
    args = parser.parse_args()

    diff_files = glob.glob(f'{args.diff_dir}/diff_*.json')
    if not diff_files:
        print(f"No diff files found in {args.diff_dir}/")
        return

    ipa_map, _all_units = _load_unit_ipa_map()
    ipa_known = _OBJDIFF_JSON.exists()
    ipa_units = {u for u, on in ipa_map.items() if on}

    all_pairs_counter = Counter()
    single_pair_results = []   # tuples preserved for legacy verbose output
    single_pair_records = []   # rich dicts (JSON + ipa-aware verbose)
    multi_pair_count = 0
    total_cs_swap_functions = 0
    ipa_penalty_counter = Counter()

    for df in diff_files:
        try:
            with open(df) as f:
                data = json.load(f)
        except Exception:
            continue

        instrs = data.get('instructions', [])
        pct = data.get('normalized_match_percent', 0)
        if not instrs or pct < args.min_pct:
            continue

        pairs = find_swap_pairs(instrs)
        if not pairs:
            continue

        total_cs_swap_functions += 1
        for p in pairs:
            all_pairs_counter[p] += 1

        sym = data.get('symbol', '')
        unit = data.get('unit', '') or ''
        resolved_unit = _resolve_unit(unit, ipa_map)
        penalty = _ipa_penalty_class(resolved_unit, pct, ipa_map, ipa_known)
        ipa_penalty_counter[penalty] += 1

        if len(pairs) == 1:
            pair = list(pairs)[0]
            lo_type, hi_type, lo_idx, hi_idx = classify_pair(instrs, pair[0], pair[1])

            lo_user = lo_type in USER_TYPES if lo_type else False
            hi_user = hi_type in USER_TYPES if hi_type else False
            fixable_raw = bool(lo_type and hi_type and lo_user and hi_user)
            # IPA modulation: at ipa_locked, force fixable=False.
            if args.ipa_aware and penalty == 'ipa_locked':
                fixable = False
                fixable_uncertain = False
            elif args.ipa_aware and penalty == 'ipa_partial' and fixable_raw:
                # Middle band: keep raw verdict but flag uncertainty.
                fixable = fixable_raw
                fixable_uncertain = True
            else:
                fixable = fixable_raw
                fixable_uncertain = False

            single_pair_results.append((pct, pair, lo_type, hi_type, lo_idx, hi_idx, sym))
            single_pair_records.append({
                'symbol': sym,
                'unit': unit,
                'resolved_unit': resolved_unit,
                'match_percent': pct,
                'pair': list(pair),
                'lo_type': lo_type,
                'hi_type': hi_type,
                'lo_idx': lo_idx,
                'hi_idx': hi_idx,
                'ipa_penalty_class': penalty,
                'fixable_raw': fixable_raw,
                'fixable': fixable,
                'fixable_uncertain': fixable_uncertain,
            })
        else:
            multi_pair_count += 1

    single_pair_results.sort(key=lambda x: -x[0])
    single_pair_records.sort(key=lambda r: -r['match_percent'])

    if args.json_output:
        out = {
            'metadata': {
                'diff_files': len(diff_files),
                'min_pct': args.min_pct,
                'ipa_aware': args.ipa_aware,
                'ipa_known': ipa_known,
                'ipa_units_count': len(ipa_units),
            },
            'totals': {
                'functions_with_cs_swap': total_cs_swap_functions,
                'single_pair': len(single_pair_records),
                'multi_pair': multi_pair_count,
                'by_ipa_penalty': dict(ipa_penalty_counter),
                'fixable_count': sum(1 for r in single_pair_records if r['fixable']),
                'fixable_raw_count': sum(1 for r in single_pair_records if r['fixable_raw']),
            },
            'pairs': [
                {'pair': list(p), 'count': c}
                for p, c in all_pairs_counter.most_common(20)
            ],
            'single_pair_records': single_pair_records,
        }
        print(json.dumps(out, indent=2))
        return

    print(f"Callee-Saved Register Swap Analysis")
    print(f"{'=' * 60}")
    print(f"Diff files scanned:     {len(diff_files)}")
    print(f"Functions with CS swap:  {total_cs_swap_functions}")
    print(f"  Single pair:           {len(single_pair_results)}")
    print(f"  Multi pair:            {multi_pair_count}")
    if args.ipa_aware:
        print(f"IPA-aware:               ON (--no-ipa-aware to disable)")
        print(f"  IPA units known:       {len(ipa_units)} from objdiff.json")
        if ipa_penalty_counter:
            classes = ['ipa_locked', 'ipa_partial', 'intra_function', 'none', 'unknown']
            for cls in classes:
                n = ipa_penalty_counter.get(cls, 0)
                if n:
                    print(f"  {cls:18}: {n:3d}")
    else:
        print(f"IPA-aware:               OFF (pre-tightening behavior)")
    print()

    print("Most common callee-saved swap pairs:")
    for pair, cnt in all_pairs_counter.most_common(15):
        print(f"  {pair[0]:>4} <-> {pair[1]:<4}: {cnt:3d} functions")
    print()

    # Classification
    type_combos = Counter()
    user_fixable_raw = both_temp = one_temp = 0
    user_fixable_adjusted = 0
    ipa_demoted = 0
    for rec in single_pair_records:
        lo_t = rec['lo_type']
        hi_t = rec['hi_type']
        if not (lo_t and hi_t):
            continue
        type_combos[(lo_t, hi_t)] += 1
        lo_user = lo_t in USER_TYPES
        hi_user = hi_t in USER_TYPES
        if lo_user and hi_user:
            user_fixable_raw += 1
            if rec['fixable']:
                user_fixable_adjusted += 1
            else:
                ipa_demoted += 1
        elif not lo_user and not hi_user:
            both_temp += 1
        else:
            one_temp += 1

    classified = user_fixable_raw + both_temp + one_temp
    if classified > 0:
        print("Variable type classification (single-pair swaps):")
        for (lo_t, hi_t), cnt in type_combos.most_common(15):
            print(f"  {lo_t:15} x {hi_t:15} = {cnt:3d}")
        print()

        print("Fixability estimate:")
        if args.ipa_aware:
            print(f"  Both user-controllable (raw):    {user_fixable_raw:3d}"
                  f" ({100*user_fixable_raw/classified:.0f}%)")
            print(f"    → fixable after IPA penalty:   {user_fixable_adjusted:3d}"
                  f" ({100*user_fixable_adjusted/classified:.0f}%) - reorder candidates")
            print(f"    → IPA-locked (demoted):        {ipa_demoted:3d}"
                  f" ({100*ipa_demoted/classified:.0f}%) - whole-TU IPA decisions")
        else:
            print(f"  Both user-controllable:    {user_fixable_raw:3d}"
                  f" ({100*user_fixable_raw/classified:.0f}%) - maybe fixable via decl reorder")
        print(f"  One user + one temp:       {one_temp:3d}"
              f" ({100*one_temp/classified:.0f}%) - unlikely fixable")
        print(f"  Both compiler-internal:    {both_temp:3d}"
              f" ({100*both_temp/classified:.0f}%) - unfixable")
    print()

    if args.verbose:
        ipa_col = ' IPA class       ' if args.show_ipa_penalty else ''
        fix_col = ' Fix' if args.show_ipa_penalty else ''
        print(f"{'%':>6} {'Pair':10} {'Lo@':>4} {'Hi@':>4} {'Lo type':15} {'Hi type':15}{ipa_col}{fix_col} Symbol")
        sep_ipa = ' ' + '─' * 15 if args.show_ipa_penalty else ''
        sep_fix = ' ' + '─' * 3 if args.show_ipa_penalty else ''
        print(f"{'─'*6} {'─'*10} {'─'*4} {'─'*4} {'─'*15} {'─'*15}{sep_ipa}{sep_fix} {'─'*50}")
        for rec in single_pair_records[:50]:
            pct = rec['match_percent']
            pair = rec['pair']
            lt = rec['lo_type']
            ht = rec['hi_type']
            li = rec['lo_idx']
            hi = rec['hi_idx']
            sym = rec['symbol']
            lt_s = lt or '?'
            ht_s = ht or '?'
            li_s = f'{li:4d}' if li is not None else '   ?'
            hi_s = f'{hi:4d}' if hi is not None else '   ?'
            extra = ''
            if args.show_ipa_penalty:
                fix_flag = 'Y' if rec['fixable'] else ('?' if rec['fixable_uncertain'] else 'N')
                extra = f" {rec['ipa_penalty_class']:15} {fix_flag:>3}"
            print(f"{pct:5.1f}% {pair[0]}<->{pair[1]:4} {li_s} {hi_s} {lt_s:15} {ht_s:15}{extra} {sym[:50]}")


if __name__ == '__main__':
    main()
