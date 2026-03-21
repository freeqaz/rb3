#!/usr/bin/env bash
if [ -z "$1" ]; then
    echo "Usage: $0 <unit_name>"
    echo "  e.g.: $0 system/char/CharBones"
    exit 1
fi

build/tools/objdiff-cli diff -u "main/$1" "" --format json-pretty -o /dev/stdout 2>/dev/null | python3 -c "
import json, sys
d = json.load(sys.stdin)
text = [s for s in d['left']['sections'] if s.get('kind') == 'SECTION_TEXT'][0]
for sym in text['symbols']:
    name = sym['symbol'].get('demangled_name', sym['symbol'].get('name', '?'))
    pct = sym.get('match_percent', 0)
    if isinstance(pct, float):
        print(f'{pct:6.1f}%  {name}')
    else:
        print(f'  ???%  {name}')
"
