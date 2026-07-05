#!/bin/bash
# pyghidra-mcp HTTP service manager for RB3 (Wii)
#
# Usage:
#   ./tools/ghidra/pyghidra-service.sh start   # Start the service
#   ./tools/ghidra/pyghidra-service.sh stop    # Stop the service
#   ./tools/ghidra/pyghidra-service.sh status  # Check status
#   ./tools/ghidra/pyghidra-service.sh restart # Restart
#   ./tools/ghidra/pyghidra-service.sh logs    # View logs
#   ./tools/ghidra/pyghidra-service.sh diagnose # Run diagnostics
#
# This is SEPARATE from the DC3 Ghidra MCP (port 8000).
# RB3 uses port 8001.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

HOST=ghidra.local
PORT=8001
PROJECT_PATH="$PROJECT_DIR/ghidra_projects/RB3/RB3"
PIDFILE="/tmp/claude/pyghidra-mcp-rb3.pid"
LOGFILE="/tmp/claude/pyghidra-mcp-rb3.log"
MILOHAX_DIR="$(cd "$PROJECT_DIR/.." && pwd)"
PYGHIDRA_MCP="$MILOHAX_DIR/pyghidra-mcp"

# ---------------------------------------------------------------------------
# TWO binaries, ONE server. pyghidra-mcp is multi-binary: a single project holds
# several programs and every tool call selects one by `binary_name`. So this one
# service on port 8001 exposes BOTH eras of RB3, and the caller picks per call:
#
#   * band_r_wii  (Bank 5 DWARF ELF, ~mid-2009) — rich DWARF TYPES + source intent,
#     but the body is wrong-era for ~20% of functions (e.g. BandHeadShaper::Init
#     is 840B here vs 3084B in the target). DEFAULT binary for back-compat.
#   * bank8_target (the real orig/ Bank 8 DOL, ~2010) — TARGET-ACCURATE body
#     (names from the CodeWarrior map, no DWARF types). Use when bank_divergence
#     says MISLEADING. bin/analyze-function --bank8 selects this program.
#
# Ghidra does NOT auto-correlate the two programs; our tooling routes per call.
# Gauge per-symbol trust with scripts/analysis/bank_divergence.py <SYMBOL>.
# Skip the heavier Bank 8 import with RB3_GHIDRA_NO_BANK8=1 (Bank 5 only).
# ---------------------------------------------------------------------------
ELF_PATH="$MILOHAX_DIR/milo-executable-library/rb3/Wii Proto (Bank 5) (Debug)/band_r_wii.elf"
DOL_PATH="$PROJECT_DIR/orig/SZBE69_B8/sys/main.dol"
MAP_FILE="$PROJECT_DIR/orig/SZBE69_B8/files/band_r_wii.map"
BANK8_ELF="$PROJECT_DIR/build/SZBE69_B8/ghidra/bank8_target.elf"

BINARY_PATHS=()
if [[ -f "$ELF_PATH" ]]; then
    BINARY_PATHS+=("$ELF_PATH")
    echo "Bank 5 DWARF (types/source intent): $ELF_PATH"
else
    BINARY_PATHS+=("$DOL_PATH")
    echo "Bank 5 ELF missing; using Bank 8 DOL only."
fi

if [[ "${RB3_GHIDRA_NO_BANK8:-0}" != "1" && -f "$DOL_PATH" ]]; then
    # Transcode the Bank 8 DOL -> symbolized Gekko ELF (idempotent; rebuilds when
    # the DOL or map changes). pyghidra-mcp then imports it like any ELF and
    # auto-selects PowerPC:BE:32:Gekko_Broadway from its 0x8xxxxxxx entry point.
    # Only (re)build on start/restart so status/stop/logs stay instant.
    if [[ "${1:-}" =~ ^(start|restart)$ && ( ! -f "$BANK8_ELF" || "$DOL_PATH" -nt "$BANK8_ELF" || "$MAP_FILE" -nt "$BANK8_ELF" ) ]]; then
        echo "Building Bank 8 target ELF (DOL->ELF via gamecube_dol)..."
        mkdir -p "$(dirname "$BANK8_ELF")"
        PYTHONPATH="$PYGHIDRA_MCP/src" python3 -m pyghidra_mcp.gamecube_dol \
            "$DOL_PATH" -m "$MAP_FILE" -o "$BANK8_ELF" || echo "  (bank8 ELF build failed; serving Bank 5 only)"
    fi
    [[ -f "$BANK8_ELF" ]] && BINARY_PATHS+=("$BANK8_ELF") && echo "Bank 8 TARGET (real body): $BANK8_ELF"
fi

BINARY_PATH="${BINARY_PATHS[0]}"  # for log/status lines below

export JAVA_HOME="/usr/lib/jvm/java-17-openjdk"
# The RB3 project's programs are PowerPC:BE:32:Gekko_Broadway (Wii). /opt/ghidra
# (stock 12.1.2) does NOT define that language id, so opening the project throws
# LanguageNotFoundException. Use the local 12.2_DEV build (symlink -> ghidra_12.2_DEV),
# which HAS Gekko_Broadway and is what the working rb3-xenon MCP instance uses.
export GHIDRA_INSTALL_DIR="/home/free/code/milohax/ghidra/build/ghidra"
# Use writable temp directory for Ghidra user home
export GHIDRA_USER_HOME="/tmp/claude/ghidra_user_rb3"

_kill_port_users() {
    # Kill any process listening on our port (catches orphaned Java/Python processes)
    local pids
    pids=$(lsof -ti ":$PORT" 2>/dev/null || true)
    if [[ -n "$pids" ]]; then
        echo "Killing orphaned processes on port $PORT: $pids"
        echo "$pids" | xargs kill 2>/dev/null || true
        sleep 1
        # Force-kill any survivors
        pids=$(lsof -ti ":$PORT" 2>/dev/null || true)
        if [[ -n "$pids" ]]; then
            echo "$pids" | xargs kill -9 2>/dev/null || true
            sleep 0.5
        fi
    fi
}

cmd_start() {
    # Check if already running via PID file
    if [[ -f "$PIDFILE" ]] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
        echo "Service already running (PID: $(cat "$PIDFILE"))"
        return 0
    fi

    # Check if port is already in use (catches orphaned processes from previous runs)
    if lsof -ti ":$PORT" > /dev/null 2>&1; then
        echo "Warning: port $PORT already in use by orphaned process"
        _kill_port_users
    fi

    # Clear any stale locks (safe now that orphaned processes are gone)
    rm -f "$PROJECT_PATH"/*.lock* 2>/dev/null || true

    # Ensure directories exist
    mkdir -p "$(dirname "$LOGFILE")" 2>/dev/null || true
    mkdir -p "$(dirname "$PROJECT_PATH")" 2>/dev/null || true
    mkdir -p "$GHIDRA_USER_HOME" 2>/dev/null || true

    echo "Starting pyghidra-mcp service for RB3..."
    echo "  Project: $PROJECT_PATH"
    echo "  Binaries: ${BINARY_PATHS[*]}"
    echo "  Port: $PORT (DC3 is on 8000)"
    echo "  Log: $LOGFILE"

    # --map-file is NOT used: pyghidra-mcp's --map-file only parses MSVC maps. The
    # Bank 5 ELF carries full DWARF; the Bank 8 program carries CodeWarrior-map
    # symbols baked into the synthetic ELF's .symtab (see pyghidra_mcp.gamecube_dol),
    # so Ghidra auto-imports both without a runtime map.
    #
    # Multiple positional binaries -> one project, multiple programs (select per
    # tool call via binary_name). --wait-for-analysis blocks until BOTH are ready.
    setsid nohup uv run --python 3.10 --project "$PYGHIDRA_MCP" pyghidra-mcp \
        --transport streamable-http \
        --host "$HOST" \
        --port "$PORT" \
        --project-path "$PROJECT_PATH" \
        --wait-for-analysis \
        --cache-dir "$PROJECT_DIR" \
        --log-file "$LOGFILE" \
        "${BINARY_PATHS[@]}" \
        > "$LOGFILE" 2>&1 &

    PID=$!
    echo $PID > "$PIDFILE"
    echo "Started with PID: $PID"
    echo ""
    echo "Server is starting in the background..."
    echo "First-time analysis of the ELF may take 10-30 minutes."
    echo "Check logs with: $0 logs"

    sleep 5
    if ps -p $PID > /dev/null 2>&1; then
        echo "Service process is running (PID: $PID)"
        return 0
    else
        echo "Error: Service process exited unexpectedly"
        tail -20 "$LOGFILE"
        return 1
    fi
}

cmd_stop() {
    if [[ -f "$PIDFILE" ]]; then
        PID=$(cat "$PIDFILE")
        if kill -0 "$PID" 2>/dev/null; then
            echo "Stopping service (PID: $PID)..."
            # Kill the entire process group (uv -> python -> java)
            kill -- -"$PID" 2>/dev/null || kill "$PID" 2>/dev/null || true
            rm -f "$PIDFILE"
            sleep 1
            # Clean up anything still on the port
            _kill_port_users
            echo "Stopped."
        else
            echo "PID file exists but process not running. Cleaning up."
            rm -f "$PIDFILE"
            _kill_port_users
        fi
    else
        # Try to find and kill any running instance
        PIDS=$(pgrep -f "pyghidra-mcp.*$PORT" || true)
        if [[ -n "$PIDS" ]]; then
            echo "Killing pyghidra-mcp processes: $PIDS"
            kill $PIDS 2>/dev/null || true
        fi
        _kill_port_users
        if [[ -z "$PIDS" ]] && ! lsof -ti ":$PORT" > /dev/null 2>&1; then
            echo "Service not running."
        fi
    fi
}

cmd_status() {
    if [[ -f "$PIDFILE" ]] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
        echo "Service running (PID: $(cat "$PIDFILE"))"
        echo "URL: http://ghidra.local:$PORT/mcp"

        # Check if responsive (use 127.0.0.1 for reliability, ghidra.local for clients)
        if curl -s "http://127.0.0.1:$PORT/mcp" > /dev/null 2>&1; then
            echo "Status: Ready"
        else
            echo "Status: Starting/Not responding"
        fi
    else
        # Check for orphaned processes on the port
        if lsof -ti ":$PORT" > /dev/null 2>&1; then
            echo "Service not running (stale PID), but port $PORT is in use by orphaned process"
            echo "Run '$0 stop' to clean up"
            return 1
        fi
        echo "Service not running."
        return 1
    fi
}

cmd_logs() {
    if [[ -f "$LOGFILE" ]]; then
        tail -f "$LOGFILE"
    else
        echo "No log file found at $LOGFILE"
    fi
}

cmd_restart() {
    cmd_stop
    sleep 2
    cmd_start
}

cmd_diagnose() {
    echo "Running Ghidra service diagnostics for RB3..."
    echo ""
    echo "Configuration:"
    echo "  Port: $PORT"
    echo "  Project: $PROJECT_PATH"
    echo "  Binary: $BINARY_PATH"
    echo "  Ghidra: $GHIDRA_INSTALL_DIR"
    echo "  Java: $JAVA_HOME"
    echo ""
    uv run --python 3.10 --project "$PYGHIDRA_MCP" pyghidra-mcp --diagnose
}

case "${1:-}" in
    start)      cmd_start ;;
    stop)       cmd_stop ;;
    status)     cmd_status ;;
    restart)    cmd_restart ;;
    logs)       cmd_logs ;;
    diagnose)   cmd_diagnose ;;
    *)
        echo "Usage: $0 {start|stop|status|restart|logs|diagnose}"
        echo ""
        echo "RB3 Ghidra MCP service (port $PORT)"
        echo "DC3 Ghidra MCP is on port 8000 -- this is separate."
        exit 1
        ;;
esac
