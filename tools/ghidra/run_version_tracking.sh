#!/usr/bin/env bash
#
# run_version_tracking.sh — drive real Ghidra Version Tracking on the RB3 project
# via the SUPPORTED analyzeHeadless path (Bank 5 DWARF markup -> Bank 8 target).
#
# This is the headless replacement for the FAILED hand-rolled tools/ghidra/version_track.py
# (its VTSessionDB construction throws LockException under the GhidraProject open-model —
# the GhidraProject open does not give the session exclusive control of the destination
# program). analyzeHeadless gives the AutoVersionTrackingTask the ServiceProvider/tool +
# program ownership it needs, so VT runs cleanly with no GUI.
#
# *** DO NOT RUN WHILE THE pyghidra-mcp SERVICE IS UP ***
#   The service holds the project lock (ghidra_projects/RB3/RB3/RB3.lock). analyzeHeadless
#   needs an exclusive write lock on the project to -process the destination program.
#   Stop the service first, run this, then restart it:
#       tools/ghidra/pyghidra-service.sh stop
#       tools/ghidra/run_version_tracking.sh
#       tools/ghidra/pyghidra-service.sh start
#   This wrapper refuses to run if the lock is present, as a guard.
#
# What it does (RB3AutoVersionTrackingScript.java, a self-contained AutoVT runner):
#   - exact-symbol correlator over all ~41,680 shared CodeWarrior mangled names
#   - exact function bytes + exact function instructions/mnemonics (byte-identical bodies)
#   - duplicate-function + reference + implied-match correlators
#   - applies Bank 5 markup: function SIGNATURE, param names, param/return data types,
#     calling convention, LABELS, comments, and GLOBAL DATA variable types
#   - FUNCTION_NAME = EXCLUDE  -> keeps our CodeWarrior mangled names (the one critical
#     override; the stock SetAutoVersionTrackingOptionsScript path CANNOT express it,
#     which is why we ship our own runner instead of the 2-postScript stock flow).
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR:-/opt/ghidra}"
HEADLESS="$GHIDRA_INSTALL_DIR/support/analyzeHeadless"

# analyzeHeadless takes the PROJECT LOCATION DIRECTORY (the dir containing <name>.gpr)
# and the PROJECT NAME separately. For RB3 the .gpr lives at
#   ghidra_projects/RB3/RB3/RB3.gpr  -> location dir = ghidra_projects/RB3/RB3, name = RB3
PROJECT_LOCATION="${RB3_VT_PROJECT_LOCATION:-$REPO_DIR/ghidra_projects/RB3/RB3}"
PROJECT_NAME="${RB3_VT_PROJECT_NAME:-RB3}"

# In-project program names (different per import; resolved dynamically below if not given).
# Defaults match the current project; the orchestrator may override via args or env.
DEST_PROGRAM="${RB3_VT_DEST_PROGRAM:-}"   # Bank 8 target (-process)
SRC_PROGRAM="${RB3_VT_SRC_PROGRAM:-}"     # Bank 5 DWARF source (project path)

# VT session to create (must NOT already exist).
SESSION_FOLDER="${RB3_VT_SESSION_FOLDER:-/}"
# Ghidra domain-file names cannot contain '>' — 'b5->b8' triggers InvalidNameException.
SESSION_NAME="${RB3_VT_SESSION_NAME:-RB3_b5_to_b8_autoVT}"

# Headless JVM heap: VT across two ~40k-symbol programs wants more than the 2G default.
export GHIDRA_HEADLESS_MAXMEM="${GHIDRA_HEADLESS_MAXMEM:-8G}"
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk}"
# Keep our headless run off the service's user dir to avoid any settings collision.
export GHIDRA_USER_HOME="${GHIDRA_USER_HOME:-/tmp/claude/ghidra_user_vt}"
mkdir -p "$GHIDRA_USER_HOME"

usage() {
	cat <<EOF
usage: $(basename "$0") [DEST_PROGRAM] [SRC_PROGRAM_PATH]

  DEST_PROGRAM      in-project name of the Bank 8 target (the -process program),
                    e.g. bank8_target.elf-42264e   (default: auto-detect)
  SRC_PROGRAM_PATH  in-project path of the Bank 5 DWARF source program,
                    e.g. /band_r_wii.elf-781439     (default: auto-detect)

env overrides: RB3_VT_PROJECT_LOCATION, RB3_VT_PROJECT_NAME, RB3_VT_DEST_PROGRAM,
               RB3_VT_SRC_PROGRAM, RB3_VT_SESSION_FOLDER, RB3_VT_SESSION_NAME,
               GHIDRA_HEADLESS_MAXMEM, GHIDRA_INSTALL_DIR
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
	usage
	exit 0
fi
[[ -n "${1:-}" ]] && DEST_PROGRAM="$1"
[[ -n "${2:-}" ]] && SRC_PROGRAM="$2"

# --- Safety guard: never run against a locked (in-use) project. ---
LOCKFILE="$PROJECT_LOCATION/$PROJECT_NAME.lock"
if [[ -e "$LOCKFILE" ]]; then
	echo "ERROR: project lock present: $LOCKFILE" >&2
	echo "       The pyghidra-mcp service (or another Ghidra instance) is using the project." >&2
	echo "       Stop it first:  tools/ghidra/pyghidra-service.sh stop" >&2
	exit 1
fi

if [[ ! -x "$HEADLESS" ]]; then
	echo "ERROR: analyzeHeadless not found/executable at: $HEADLESS" >&2
	exit 1
fi
if [[ ! -f "$PROJECT_LOCATION/$PROJECT_NAME.gpr" ]]; then
	echo "ERROR: no Ghidra project at $PROJECT_LOCATION/$PROJECT_NAME.gpr" >&2
	exit 1
fi

# --- Resolve in-project program names from the .rep index if not supplied. ---
# The project's idata/~index.dat lines look like: NNNNNNNN:<programName>:<hash>
INDEX="$PROJECT_LOCATION/$PROJECT_NAME.rep/idata/~index.dat"
if [[ -z "$DEST_PROGRAM" || -z "$SRC_PROGRAM" ]]; then
	if [[ -f "$INDEX" ]]; then
		# Bank 8 destination = the bank8_target program; Bank 5 source = band_r_wii.
		AUTO_DEST="$(grep -aoE 'bank8_target\.elf-[0-9a-f]+' "$INDEX" | head -n1 || true)"
		AUTO_SRC="$(grep -aoE 'band_r_wii\.elf-[0-9a-f]+' "$INDEX" | head -n1 || true)"
		[[ -z "$DEST_PROGRAM" ]] && DEST_PROGRAM="$AUTO_DEST"
		[[ -z "$SRC_PROGRAM"  ]] && SRC_PROGRAM="/$AUTO_SRC"
	fi
fi

if [[ -z "$DEST_PROGRAM" || -z "$SRC_PROGRAM" || "$SRC_PROGRAM" == "/" ]]; then
	echo "ERROR: could not resolve program names." >&2
	echo "       Pass them explicitly: $(basename "$0") <DEST_PROGRAM> <SRC_PROGRAM_PATH>" >&2
	echo "       (look in $INDEX)" >&2
	exit 1
fi

echo "Version Tracking (headless, no GUI):"
echo "  project    : $PROJECT_LOCATION  (name: $PROJECT_NAME)"
echo "  destination: $DEST_PROGRAM  (-process, Bank 8 target — gets markup)"
echo "  source     : $SRC_PROGRAM  (Bank 5 DWARF)"
echo "  session    : $SESSION_FOLDER$SESSION_NAME"
echo "  heap       : $GHIDRA_HEADLESS_MAXMEM"
echo

# -noanalysis : both programs are already analyzed; do not re-run analysis.
# -scriptPath : where RB3AutoVersionTrackingScript.java lives.
# -postScript : our self-contained AutoVT runner; args = sessionFolder, sessionName, srcPath.
exec "$HEADLESS" \
	"$PROJECT_LOCATION" "$PROJECT_NAME" \
	-process "$DEST_PROGRAM" \
	-noanalysis \
	-scriptPath "$SCRIPT_DIR" \
	-postScript RB3AutoVersionTrackingScript.java "$SESSION_FOLDER" "$SESSION_NAME" "$SRC_PROGRAM"
