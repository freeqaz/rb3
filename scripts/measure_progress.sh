#!/usr/bin/env bash
#
# Measure incremental decomp progress between a baseline commit and HEAD.
#
# Uses a git worktree to build the baseline report, then compares it
# against the main repo's current report using compare_progress.py.
#
# Usage:
#   scripts/measure_progress.sh                    # Compare HEAD vs HEAD~1
#   scripts/measure_progress.sh dd02a3e            # Compare HEAD vs specific commit
#   scripts/measure_progress.sh --worktree /path   # Use existing worktree dir
#   scripts/measure_progress.sh --detailed HEAD~5  # Show per-unit breakdown
#   scripts/measure_progress.sh --functions c8d98a # Show function-level changes
#   scripts/measure_progress.sh --regressions      # Only show regressions
#   scripts/measure_progress.sh --current-dir /path/to/worktree HEAD  # Use worktree as "current"
#
set -euo pipefail

MAIN_REPO="$(cd "$(dirname "$0")/.." && pwd)"
REPORT_REL="build/SZBE69_B8/report.json"
BASELINE_REF="HEAD~1"
COMPARE_FLAGS=()
WORKTREE_DIR="/tmp/claude/rb3-measure-progress"
CREATED_WORKTREE=0
CURRENT_DIR=""

# --- Parse arguments ---
while [[ $# -gt 0 ]]; do
    case "$1" in
        --worktree)
            WORKTREE_DIR="$2"
            shift 2
            ;;
        --detailed)
            COMPARE_FLAGS+=("--detailed")
            shift
            ;;
        --functions|-f)
            COMPARE_FLAGS+=("--functions")
            shift
            ;;
        --regressions|-r)
            COMPARE_FLAGS+=("--regressions")
            shift
            ;;
        --current-dir)
            CURRENT_DIR="$2"
            shift 2
            ;;
        --limit)
            COMPARE_FLAGS+=("--limit" "$2")
            shift 2
            ;;
        --help|-h)
            sed -n '2,16p' "$0"
            exit 0
            ;;
        *)
            BASELINE_REF="$1"
            shift
            ;;
    esac
done

WORKTREE="${WORKTREE_DIR}"
CACHE_DIR="${MAIN_REPO}/build/SZBE69_B8/baselines"

# --- Resolve current directory (main repo or worktree) ---
if [[ -n "${CURRENT_DIR}" ]]; then
    CURRENT_DIR="$(cd "${CURRENT_DIR}" && pwd)"
    if [[ ! -f "${CURRENT_DIR}/${REPORT_REL}" ]]; then
        echo "Current report not found in worktree, building..."
        ninja -C "${CURRENT_DIR}" "${REPORT_REL}" -j"$(nproc)" 2>&1 | tail -1
    fi
    CURRENT_REPORT="${CURRENT_DIR}/${REPORT_REL}"
    CURRENT_LABEL="worktree:$(basename "${CURRENT_DIR}")"
else
    CURRENT_DIR="${MAIN_REPO}"
    CURRENT_REPORT="${MAIN_REPO}/${REPORT_REL}"
    CURRENT_LABEL="working tree"
fi

# --- Verify prerequisites ---
if [[ ! -f "${CURRENT_REPORT}" ]]; then
    echo "Error: Current report not found: ${CURRENT_REPORT}"
    echo "Run 'ninja' first."
    exit 1
fi

if [[ ! -d "${MAIN_REPO}/orig/SZBE69_B8" ]]; then
    echo "Error: orig/ binaries not found in main repo."
    exit 1
fi

# Resolve the baseline ref to an actual commit hash
BASELINE_COMMIT=$(git -C "${MAIN_REPO}" rev-parse "${BASELINE_REF}")
BASELINE_SHORT=$(git -C "${MAIN_REPO}" rev-parse --short "${BASELINE_COMMIT}")
CURRENT_SHORT="${CURRENT_LABEL}"

echo "Measuring progress: ${BASELINE_SHORT} (baseline) -> ${CURRENT_SHORT} (current)"

# --- Check baseline cache ---
CACHED_REPORT="${CACHE_DIR}/${BASELINE_COMMIT}.json"
if [[ -f "${CACHED_REPORT}" ]]; then
    echo "Using cached baseline report for ${BASELINE_SHORT}"
    BASELINE_REPORT="${CACHED_REPORT}"
else
    echo "No cached baseline for ${BASELINE_SHORT}, building..."
    echo "Using worktree: ${WORKTREE}"

    # --- Create worktree if it doesn't exist ---
    if [[ ! -d "${WORKTREE}" ]]; then
        echo "Creating worktree at ${WORKTREE}..."
        git -C "${MAIN_REPO}" worktree add --detach "${WORKTREE}" HEAD --quiet
        CREATED_WORKTREE=1
    fi

    # --- Save worktree state for restoration ---
    ORIGINAL_COMMIT=$(git -C "${WORKTREE}" rev-parse HEAD)

    cleanup() {
        echo ""
        if [[ "${CREATED_WORKTREE}" -eq 1 ]]; then
            echo "Removing temporary worktree..."
            git -C "${MAIN_REPO}" worktree remove --force "${WORKTREE}" 2>/dev/null || true
        else
            echo "Restoring worktree to ${ORIGINAL_COMMIT:0:7}..."
            git -C "${WORKTREE}" reset --hard --quiet "${ORIGINAL_COMMIT}" 2>/dev/null || true
        fi
    }
    trap cleanup EXIT

    # --- Reset worktree to baseline commit ---
    echo "Resetting worktree to baseline ${BASELINE_SHORT}..."
    git -C "${WORKTREE}" reset --hard --quiet "${BASELINE_COMMIT}"

    # Clean untracked source files but preserve build artifacts and symlinks
    git -C "${WORKTREE}" clean -fd \
        --exclude=build/ \
        --exclude=bin/ \
        --exclude=orig \
        --exclude=scripts \
        --exclude=compile_commands.json \
        --exclude=decomp.db \
        --exclude=objdiff.json \
        --exclude=build.ninja \
        --quiet 2>/dev/null || true

    # --- Ensure orig/ symlink ---
    if [[ ! -L "${WORKTREE}/orig" || "$(readlink "${WORKTREE}/orig")" != "${MAIN_REPO}/orig" ]]; then
        rm -rf "${WORKTREE}/orig"
        ln -sf "${MAIN_REPO}/orig" "${WORKTREE}/orig"
        echo "Restored orig/ symlink"
    fi

    # --- Ensure scripts symlink ---
    if [[ ! -L "${WORKTREE}/scripts" || "$(readlink "${WORKTREE}/scripts")" != "${MAIN_REPO}/scripts" ]]; then
        rm -rf "${WORKTREE}/scripts"
        ln -sf "${MAIN_REPO}/scripts" "${WORKTREE}/scripts"
        echo "Restored scripts/ symlink"
    fi

    # --- Ensure build tools and compilers are available ---
    # RB3 downloads tools into build/tools/ and build/compilers/ via download_tool.py.
    # Use the main repo's copies, but COPY individual tools (not symlink the dir)
    # so ninja can overwrite a version-mismatched tool in the worktree without
    # hitting ETXTBSY on the main repo's binary if it's in use by another agent.
    mkdir -p "${WORKTREE}/build/tools" "${WORKTREE}/build/compilers"
    if [[ -d "${MAIN_REPO}/build/tools" ]]; then
        for tool in "${MAIN_REPO}/build/tools"/*; do
            dest="${WORKTREE}/build/tools/$(basename "$tool")"
            if [[ ! -e "$dest" ]]; then
                if [[ -d "$tool" ]]; then
                    ln -sf "$tool" "$dest"
                else
                    cp -p "$tool" "$dest"
                fi
            fi
        done
    fi
    # Compilers are large; safe to share via symlink (never written by build).
    if [[ -d "${MAIN_REPO}/build/compilers" ]]; then
        for comp in "${MAIN_REPO}/build/compilers"/*; do
            dest="${WORKTREE}/build/compilers/$(basename "$comp")"
            [[ -e "$dest" ]] || ln -sf "$comp" "$dest"
        done
    fi

    # --- Extract configure args from main repo ---
    CONFIGURE_ARGS=()
    if [[ -f "${MAIN_REPO}/build.ninja" ]]; then
        raw_args=$(sed -n '/^configure_args/{ :a; /\$$/{ N; s/\$\n\s*/ /; ba }; s/^configure_args = //; p }' \
            "${MAIN_REPO}/build.ninja")
        for arg in $raw_args; do
            CONFIGURE_ARGS+=("$arg")
        done
    fi

    # Fallback if no args found
    if [[ ${#CONFIGURE_ARGS[@]} -eq 0 ]]; then
        CONFIGURE_ARGS=(--version SZBE69_B8 --map)
    fi

    # --- Resolve relative tool paths to absolute (for worktrees outside source tree) ---
    # configure.py defaults to ../objdiff/target/release/objdiff-cli which is
    # relative to the repo root. Resolve it from the main repo and pass explicitly.
    resolve_tool() {
        local rel_path="$1"
        local abs_path
        abs_path="$(cd "${MAIN_REPO}" && realpath -e "${rel_path}" 2>/dev/null)" || return 1
        echo "${abs_path}"
    }

    # Check if --objdiff was already specified in configure_args
    has_objdiff=0
    for arg in "${CONFIGURE_ARGS[@]}"; do
        [[ "$arg" == "--objdiff" ]] && has_objdiff=1
    done
    if [[ "${has_objdiff}" -eq 0 ]]; then
        # Resolve the default relative path from the main repo
        objdiff_abs="$(resolve_tool "../objdiff/target/release/objdiff-cli")" && \
            CONFIGURE_ARGS+=("--objdiff" "${objdiff_abs}")
    fi

    echo "Using configure args: ${CONFIGURE_ARGS[*]}"

    # --- Find dtk for split step ---
    DTK_BIN="${MAIN_REPO}/build/tools/dtk"
    if [[ ! -x "${DTK_BIN}" ]]; then
        # Try worktree's copy
        DTK_BIN="${WORKTREE}/build/tools/dtk"
    fi

    # --- Generate split config explicitly ---
    if [[ -x "${DTK_BIN}" ]]; then
        echo "Generating baseline split config (dtk dol split)..."
        SPLIT_LOG="$(mktemp -t rb3_measure_progress_split.XXXXXX.log)"
        if ! (cd "${WORKTREE}" && "${DTK_BIN}" dol split config/SZBE69_B8/config.yml build/SZBE69_B8) \
            >"${SPLIT_LOG}" 2>&1; then
            echo "Error: Failed to generate baseline split config with dtk:"
            echo "  ${DTK_BIN} dol split config/SZBE69_B8/config.yml build/SZBE69_B8"
            echo ""
            tail -100 "${SPLIT_LOG}" || true
            echo ""
            echo "Hint: the selected baseline may require a different dtk version or a cached baseline report."
            exit 1
        fi
        rm -f "${SPLIT_LOG}" 2>/dev/null || true
    else
        echo "Warning: dtk not found, skipping split step (ninja will attempt it)"
    fi

    # --- Reconfigure for baseline's file set ---
    echo "Reconfiguring baseline..."
    (cd "${WORKTREE}" && python3 configure.py "${CONFIGURE_ARGS[@]}") >/dev/null

    # Normalize timestamps to prevent ninja manifest-dirty loops
    normalize_manifest_timestamps() {
        local deps=(
            "${WORKTREE}/build/SZBE69_B8/config.json"
            "${WORKTREE}/configure.py"
            "${WORKTREE}/tools/project.py"
            "${WORKTREE}/tools/ninja_syntax.py"
            "${WORKTREE}/config/SZBE69_B8/config.json"
            "${WORKTREE}/config/SZBE69_B8/objects.json"
        )
        local touched_any=0
        for dep in "${deps[@]}"; do
            if [[ -e "${dep}" ]]; then
                touch "${dep}" 2>/dev/null || true
                touched_any=1
            fi
        done
        if [[ "${touched_any}" -eq 1 ]]; then
            sleep 1
        fi
        touch "${WORKTREE}/build.ninja" "${WORKTREE}/objdiff.json" 2>/dev/null || true
    }
    normalize_manifest_timestamps

    # --- Build baseline report ---
    echo "Building baseline report (this may take a moment)..."
    BUILD_LOG="$(mktemp -t rb3_measure_progress_ninja.XXXXXX.log)"
    if ninja -C "${WORKTREE}" "${REPORT_REL}" -j"$(nproc)" >"${BUILD_LOG}" 2>&1; then
        tail -1 "${BUILD_LOG}" || true
    else
        tail -100 "${BUILD_LOG}" || true
        if grep -q "manifest 'build.ninja' still dirty" "${BUILD_LOG}" && \
           grep -q "output build/SZBE69_B8/config.json doesn't exist" "${BUILD_LOG}"; then
            echo ""
            echo "Hint: ninja's manifest-dirty loop is usually a secondary symptom."
            echo "      The baseline split step failed, so build/SZBE69_B8/config.json was never created."
        fi
        exit 1
    fi
    rm -f "${BUILD_LOG}" 2>/dev/null || true

    if [[ ! -f "${WORKTREE}/${REPORT_REL}" ]]; then
        echo "Error: Baseline report was not generated."
        exit 1
    fi

    # --- Cache the baseline report ---
    mkdir -p "${CACHE_DIR}"
    cp "${WORKTREE}/${REPORT_REL}" "${CACHED_REPORT}"
    echo "Cached baseline report -> ${CACHED_REPORT}"

    BASELINE_REPORT="${WORKTREE}/${REPORT_REL}"
fi

# --- Compare ---
echo ""
python3 "${MAIN_REPO}/scripts/analysis/compare_progress.py" \
    "${COMPARE_FLAGS[@]}" \
    "${BASELINE_REPORT}" \
    "${CURRENT_REPORT}"
