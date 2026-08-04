#!/bin/bash
# test-ninja-dry.sh — regression test for tools/ninja-dry.
#
# Runs against a SELF-CONTAINED scratch ninja project in a temp dir, not the
# RB3 build, so it is fast (<1s), hermetic, and safe to run while other agents
# are building. It asserts four things:
#
#   T1  clean tree                       -> ninja-dry says CLEAN, exit 0
#   T2  one dirty input                  -> ninja-dry says PENDING, exit 1
#   T3  STALE MANIFEST + real dirty work -> raw `ninja -n` reports the
#                                           regeneration edge and exits 0 (the
#                                           defect, asserted as still present)
#                                           while ninja-dry says PENDING, exit 1
#   T4  PERPETUALLY dirty generator      -> ninja-dry exits 2 (FAILED) and does
#                                           NOT say CLEAN
#
# T4 is the point. A check that has never been shown to fail is not verified;
# T4 constructs a manifest that genuinely cannot converge and requires
# ninja-dry to refuse to answer rather than hand back a comforting "CLEAN".
#
# Usage: tools/tests/test-ninja-dry.sh

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NINJA_DRY="$SCRIPT_DIR/../ninja-dry"
[ -x "$NINJA_DRY" ] || { echo "FATAL: $NINJA_DRY not executable"; exit 2; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

pass=0
fail=0
check() { # check <name> <condition-description> <actual> <expected-substring>
    local name="$1" what="$2" actual="$3" want="$4"
    if printf '%s' "$actual" | grep -qF -- "$want"; then
        echo "  ok   $name: $what"
        pass=$((pass + 1))
    else
        echo "  FAIL $name: $what"
        echo "       wanted substring: $want"
        echo "       got: $actual"
        fail=$((fail + 1))
    fi
}
check_rc() {
    local name="$1" what="$2" actual="$3" want="$4"
    if [ "$actual" = "$want" ]; then
        echo "  ok   $name: $what (exit $actual)"
        pass=$((pass + 1))
    else
        echo "  FAIL $name: $what — wanted exit $want, got $actual"
        fail=$((fail + 1))
    fi
}

# ---------------------------------------------------------------------------
# Scratch project. `gen.sh` plays the part of configure.py: it writes
# build.ninja. `trigger` plays the part of config/<VER>/objects.json — an input
# to the generator edge that ordinary work touches.
# ---------------------------------------------------------------------------
make_project() { # make_project <self_dirtying:0|1>
    local self_dirty="$1"
    # Making a generator edge GENUINELY perpetually dirty is harder than the
    # folk model suggests, and getting it wrong makes T4 vacuous. Two drafts
    # failed before this one (both recorded in
    # docs/decomp/ninja-dry-run-false-negative.md):
    #   * `touch trigger` BEFORE writing build.ninja — build.ninja ends up
    #     newer than its own input; converges on the first try.
    #   * `touch trigger` AFTER writing build.ninja — still converges. ninja
    #     >=1.11 records the COMMAND-COMPLETION time in .ninja_log and compares
    #     THAT (not the output's mtime) against inputs, with a strict `<`. The
    #     touch and the completion land in the same timestamp, so it reads
    #     clean. This is also why rb3's own generator edge is *not* perpetually
    #     dirty: one real ninja run settles it.
    # A FUTURE mtime is what actually defeats the recorded-mtime scheme, and it
    # is the realistic failure too — ninja's own diagnostic for this state is
    # "perhaps system time is not set".
    cat > "$WORK/gen.sh" <<EOF
#!/bin/sh
cd "\$(dirname "\$0")"
cat > build.ninja <<'NINJA'
rule cp
  command = cp \$in \$out
  description = CP \$out

rule regen
  command = ./gen.sh
  description = RUN configure.py
  generator = 1

build build.ninja: regen | gen.sh trigger

build a.out: cp a.in
build b.out: cp b.in

default a.out b.out
NINJA
$( [ "$self_dirty" = 1 ] && echo "touch -d '+1 hour' trigger   # future mtime: can never converge" )
EOF
    chmod +x "$WORK/gen.sh"
    : > "$WORK/trigger"
    echo a > "$WORK/a.in"
    echo b > "$WORK/b.in"
    (cd "$WORK" && ./gen.sh)
}

echo "== scratch project: $WORK"
make_project 0
(cd "$WORK" && ninja) >/dev/null 2>&1 || { echo "FATAL: scratch build failed"; exit 2; }

# --- T1: clean ---------------------------------------------------------------
echo "T1 clean tree"
out=$(cd "$WORK" && "$NINJA_DRY" 2>&1); rc=$?
check    T1 "reports CLEAN"        "$out" "ninja-dry: CLEAN"
check    T1 "states a denominator" "$out" "/2 edge(s) pending"
check_rc T1 "exit code"            "$rc"  0

# --- T2: one dirty input -----------------------------------------------------
echo "T2 one dirty input (a.in)"
sleep 0.02; touch "$WORK/a.in"
out=$(cd "$WORK" && "$NINJA_DRY" -q 2>&1); rc=$?
check    T2 "reports PENDING 1/2" "$out" "ninja-dry: PENDING 1/2"
check_rc T2 "exit code"           "$rc"  1
(cd "$WORK" && ninja) >/dev/null 2>&1

# --- T3: stale manifest hiding real work (THE DEFECT) ------------------------
echo "T3 stale manifest + real dirty work"
sleep 0.02; touch "$WORK/trigger" "$WORK/a.in" "$WORK/b.in"
raw=$(cd "$WORK" && ninja -n 2>&1); raw_rc=$?
check    T3 "raw \`ninja -n\` is truncated to the generator edge" "$raw" "RUN configure.py"
if printf '%s' "$raw" | grep -qF "CP a.out"; then
    echo "  FAIL T3: raw \`ninja -n\` listed the real work — the ninja defect this"
    echo "       tool exists for is GONE (ninja $(ninja --version)). Re-evaluate"
    echo "       whether tools/ninja-dry is still needed before deleting it."
    fail=$((fail + 1))
else
    echo "  ok   T3: raw \`ninja -n\` HID the 2 genuinely-dirty edges"
    pass=$((pass + 1))
fi
check_rc T3 "raw \`ninja -n\` exits 0 anyway (the false negative)" "$raw_rc" 0

out=$(cd "$WORK" && "$NINJA_DRY" -q 2>&1); rc=$?
check    T3 "ninja-dry reports PENDING 2/2" "$out" "ninja-dry: PENDING 2/2"
check_rc T3 "ninja-dry exit code"           "$rc"  1
(cd "$WORK" && ninja) >/dev/null 2>&1

# --- T4: perpetually dirty generator — the check MUST refuse to answer -------
echo "T4 perpetually dirty generator (gen.sh gives its own input a FUTURE mtime)"
make_project 1
(cd "$WORK" && ninja) >/dev/null 2>&1
out=$(cd "$WORK" && "$NINJA_DRY" -q 2>&1); rc=$?
check    T4 "reports FAILED"              "$out" "ninja-dry: FAILED"
check_rc T4 "exit code"                   "$rc"  2
if printf '%s' "$out" | grep -qF "ninja-dry: CLEAN"; then
    echo "  FAIL T4: reported CLEAN on an unanswerable check"
    fail=$((fail + 1))
else
    echo "  ok   T4: did NOT report CLEAN"
    pass=$((pass + 1))
fi

# --- T5: manifest with NO generator edge -------------------------------------
# Trivially always current. Must be a normal verdict, not FAILED. (Regression:
# ninja-dry reported "FAILED manifest regeneration exited 1" here, because
# `ninja build.ninja` says "unknown target".)
echo "T5 hand-written manifest with no generator edge"
S="$WORK/static"; mkdir -p "$S"
printf 'rule cp\n  command = cp $in $out\n  description = CP $out\nbuild a.out: cp a.in\ndefault a.out\n' > "$S/build.ninja"
echo a > "$S/a.in"
(cd "$S" && ninja) >/dev/null 2>&1
out=$(cd "$S" && "$NINJA_DRY" -q 2>&1); rc=$?
check    T5 "reports CLEAN" "$out" "ninja-dry: CLEAN   0/1"
check_rc T5 "exit code"     "$rc"  0
sleep 0.02; touch "$S/a.in"
out=$(cd "$S" && "$NINJA_DRY" -q 2>&1); rc=$?
check    T5 "still detects real work" "$out" "ninja-dry: PENDING 1/1"
check_rc T5 "exit code"               "$rc"  1

# --- T6: the two wrappers must agree on the lock path ------------------------
# ninja-dry duplicates ninja-locked's build-dir/lock derivation (deliberately —
# ninja-locked is on the fleet's hot path and sourcing a helper adds a failure
# mode). Guard the duplication instead of refactoring it.
echo "T6 ninja-dry and ninja-locked derive the same lock path"
blk() { sed -n '/^build_cwd="\$PWD"$/,/^fi$/p' "$1" | grep -vE '^[[:space:]]*(#|$)'; }
if [ -f "$SCRIPT_DIR/../ninja-locked" ] && \
   [ "$(blk "$SCRIPT_DIR/../ninja-locked")" = "$(blk "$NINJA_DRY")" ] && \
   [ -n "$(blk "$NINJA_DRY")" ]; then
    echo "  ok   T6: derivation code is identical ($(blk "$NINJA_DRY" | wc -l) lines)"
    pass=$((pass + 1))
else
    echo "  FAIL T6: ninja-locked and ninja-dry have DIVERGED on lock derivation."
    echo "       Two spellings of one build dir can now take two different locks."
    diff <(blk "$SCRIPT_DIR/../ninja-locked") <(blk "$NINJA_DRY")
    fail=$((fail + 1))
fi

echo
echo "ninja-dry tests: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
