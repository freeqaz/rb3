#!/bin/bash
# ninja-dry-matrix.sh — reproduce the ninja -n false-negative control matrix
# against the REAL RB3 build (as opposed to test-ninja-dry.sh, which uses a
# hermetic scratch project).
#
# Each row edits one thing, asks both `ninja -n` and `tools/ninja-dry`, then
# restores the original mtimes. Nothing is ever compiled — only mtimes move —
# so the tree ends exactly where it started.
#
# PRECONDITION: the tree must be CONVERGED (`tools/ninja-locked` to completion)
# or row A is not a baseline and every row is offset by the leftover edges.
# The script checks this and refuses to run otherwise, because a matrix taken
# from a dirty tree reads like a tool bug when it is actually correct output.
# (That happened on the first re-run of this matrix: rows A/D/E came back
# "PENDING 3" — REPORT + PROGRESS + SYNC decomp.db, dragged in by an earlier
# `dtk dol split` — and ninja-dry was right.)
#
# Usage: tools/tests/ninja-dry-matrix.sh
# Expected output: see docs/decomp/ninja-dry-run-false-negative.md

set -u
cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)" || exit 2

H=src/system/obj/Object.h
C=src/system/ui/UILabel.cpp
O=config/SZBE69_B8/objects.json
P=configure.py

for f in "$H" "$C" "$O" "$P" tools/ninja-dry tools/ninja-locked; do
    [ -e "$f" ] || { echo "FATAL: missing $f"; exit 2; }
done

if ! ./tools/ninja-dry -q | grep -q 'CLEAN'; then
    echo "REFUSING: tree is not converged. Run \`tools/ninja-locked\` to completion first."
    echo "Current state: $(./tools/ninja-dry -q)"
    exit 2
fi

OH=$(stat -c %y "$H"); OC=$(stat -c %y "$C")
OO=$(stat -c %y "$O"); OP=$(stat -c %y "$P")
restore() {
    touch -d "$OH" "$H"; touch -d "$OC" "$C"
    touch -d "$OO" "$O"; touch -d "$OP" "$P"
    ./tools/ninja-dry -q >/dev/null 2>&1
}
trap restore EXIT

row() { # row <label> <setup-cmd>
    eval "$2"
    raw=$(./tools/ninja-locked -n 2>&1); rawrc=$?
    rawn=$(printf '%s\n' "$raw" | grep -cE '^\[[0-9]+/[0-9]+\] ')
    rawtail=$(printf '%s\n' "$raw" | grep -E '^\[[0-9]+/[0-9]+\] ' | tail -1)
    dry=$(./tools/ninja-dry -q 2>&1); dryrc=$?
    printf '%-46s | raw ninja -n: %5s edge(s), exit %d  (last: %-28s) | %s (exit %d)\n' \
        "$1" "$rawn" "$rawrc" "${rawtail:-<none>}" "$dry" "$dryrc"
    restore
}

echo "denominator: $(ninja -t commands 2>/dev/null | grep -c .) edges in the default-target closure"
echo "$H is named by $(grep -rlF "$H" build/SZBE69_B8 --include='*.d' 2>/dev/null | wc -l) of $(find build/SZBE69_B8 -name '*.d' | wc -l) depfiles"
echo

row "A  nothing edited"                           "true"
row "B  header (Object.h)"                        "touch $H"
row "C  single .cpp (UILabel.cpp)"                "touch $C"
row "D  configure.py only"                        "touch $P"
row "E  objects.json only (manifest stale)"       "touch $O"
row "F  objects.json + header  <== FALSE NEG"     "touch $O $H"
row "G  objects.json + single .cpp <== FALSE NEG" "touch $O $C"

echo
echo "Rows E, F and G print IDENTICAL raw \`ninja -n\` output and exit 0, yet E has"
echo "no work and F/G have 727 and 4 edges of real work. That is the defect."
