#!/usr/bin/env bash
#
# Check for decomp regressions over one or more time windows.
#
# Resolves each window (e.g. 6h, 24h) to the last commit at-or-before that
# cutoff, then calls scripts/measure_progress.sh with --regressions --functions
# to surface any function whose match% dropped vs that baseline.
#
# Baseline reports are cached in build/SZBE69_B8/baselines/<commit>.json by the
# underlying measure_progress.sh, so re-runs within the same window are cheap.
#
# Usage:
#   scripts/check_regressions.sh                       # 6h 12h 24h 48h (default)
#   scripts/check_regressions.sh 1h 6h 24h             # custom windows
#   scripts/check_regressions.sh --improvements 6h     # include improvements
#   scripts/check_regressions.sh --limit 200 6h 24h    # raise function cap
#
# Windows accept any value the `date -d "<N> <unit> ago"` form accepts:
# `30m`, `2h`, `1d`, `1w`, etc.
#
set -euo pipefail

MAIN_REPO="$(cd "$(dirname "$0")/.." && pwd)"
MEASURE="${MAIN_REPO}/scripts/measure_progress.sh"

WINDOWS=()
INCLUDE_IMPROVEMENTS=0
LIMIT=100
EXTRA_FLAGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --improvements|-i)
            INCLUDE_IMPROVEMENTS=1
            shift
            ;;
        --limit)
            LIMIT="$2"
            shift 2
            ;;
        --detailed)
            EXTRA_FLAGS+=("--detailed")
            shift
            ;;
        --help|-h)
            sed -n '2,19p' "$0"
            exit 0
            ;;
        *)
            WINDOWS+=("$1")
            shift
            ;;
    esac
done

if [[ ${#WINDOWS[@]} -eq 0 ]]; then
    WINDOWS=(6h 12h 24h 48h)
fi

# --- Translate a window token into seconds for `date -d` ---
window_to_human() {
    local w="$1"
    local n="${w%[smhdwSMHDW]}"
    local u="${w: -1}"
    case "$u" in
        s|S) echo "${n} seconds ago" ;;
        m|M) echo "${n} minutes ago" ;;
        h|H) echo "${n} hours ago" ;;
        d|D) echo "${n} days ago" ;;
        w|W) echo "${n} weeks ago" ;;
        *)
            # Fallback: treat as a literal date phrase
            echo "$w"
            ;;
    esac
}

# --- Resolve each window to its baseline commit, dedup, preserve order ---
declare -A SEEN
PLAN=()  # entries of the form "<window>|<short>|<commit>|<subject>"
for w in "${WINDOWS[@]}"; do
    human="$(window_to_human "$w")"
    cutoff="$(date -d "$human" '+%Y-%m-%d %H:%M:%S')"
    commit="$(git -C "${MAIN_REPO}" log --first-parent --until="$cutoff" --format='%H' -n 1)"
    if [[ -z "$commit" ]]; then
        echo "Warning: no commit found at-or-before $cutoff (window $w); skipping"
        continue
    fi
    short="$(git -C "${MAIN_REPO}" rev-parse --short "$commit")"
    subject="$(git -C "${MAIN_REPO}" log -1 --format='%s' "$commit")"
    if [[ -n "${SEEN[$commit]:-}" ]]; then
        echo "Window ${w}: resolves to ${short} — same as ${SEEN[$commit]}, skipping rebuild"
        PLAN+=("${w}|${short}|${commit}|${subject}|dup")
        continue
    fi
    SEEN[$commit]="$w"
    PLAN+=("${w}|${short}|${commit}|${subject}|run")
done

echo ""
echo "==============================================================="
echo "  REGRESSION CHECK PLAN"
echo "==============================================================="
for entry in "${PLAN[@]}"; do
    IFS='|' read -r w short commit subject status <<<"$entry"
    flag=""
    [[ "$status" == "dup" ]] && flag=" (duplicate of earlier window)"
    echo "  ${w}: ${short} — ${subject}${flag}"
done
echo ""

# --- Run for each unique baseline ---
COMPARE_FLAGS=("--functions" "--limit" "${LIMIT}" "${EXTRA_FLAGS[@]}")
if [[ ${INCLUDE_IMPROVEMENTS} -eq 0 ]]; then
    COMPARE_FLAGS+=("--regressions")
fi

OVERALL_STATUS=0
MAX_FALLBACK=8  # commits to walk forward when a baseline doesn't build

for entry in "${PLAN[@]}"; do
    IFS='|' read -r w short commit subject status <<<"$entry"
    [[ "$status" == "dup" ]] && continue

    # Try the requested commit; if its build fails (broken historical state),
    # walk forward (toward HEAD) up to MAX_FALLBACK commits to find one that
    # does build. Skip commits we've already used.
    attempt=0
    try_commit="$commit"
    try_short="$short"
    try_subject="$subject"
    while :; do
        echo ""
        echo "==============================================================="
        echo "  Window: ${w}    Baseline: ${try_short}"
        echo "  ${try_subject}"
        if [[ $attempt -gt 0 ]]; then
            echo "  (fallback +${attempt} from original ${short})"
        fi
        echo "==============================================================="
        if "${MEASURE}" "${COMPARE_FLAGS[@]}" "${try_commit}"; then
            break
        fi
        attempt=$((attempt + 1))
        if [[ $attempt -gt $MAX_FALLBACK ]]; then
            echo "Error: measure_progress.sh failed for window ${w}; exhausted ${MAX_FALLBACK} fallbacks"
            OVERALL_STATUS=1
            break
        fi
        # Pick the next-newer commit after try_commit on the master line
        next_commit="$(git -C "${MAIN_REPO}" log --first-parent --reverse --format='%H' "${try_commit}..HEAD" | head -n 1)"
        if [[ -z "$next_commit" ]]; then
            echo "Error: no newer commit to fall back to from ${try_short}"
            OVERALL_STATUS=1
            break
        fi
        if [[ -n "${SEEN[$next_commit]:-}" ]]; then
            echo "Fallback ${next_commit:0:8} already used as baseline for ${SEEN[$next_commit]}; skipping to next"
        fi
        SEEN[$next_commit]="$w (fallback)"
        try_commit="$next_commit"
        try_short="$(git -C "${MAIN_REPO}" rev-parse --short "$try_commit")"
        try_subject="$(git -C "${MAIN_REPO}" log -1 --format='%s' "$try_commit")"
        echo ""
        echo "Build failed; walking forward to next commit: ${try_short} — ${try_subject}"
    done
done

echo ""
echo "==============================================================="
echo "  Done. Baselines cached in build/SZBE69_B8/baselines/"
echo "==============================================================="
exit $OVERALL_STATUS
