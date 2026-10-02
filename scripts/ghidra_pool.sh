#!/bin/bash
# ghidra_pool.sh — Manage a pool of Ghidra project clones for parallel sessions.
#
# Ghidra locks .gpr files at the project level, so multiple Claude Code sessions
# cannot share a single project. This script maintains N clones of the master
# SimGolf.gpr/SimGolf.rep so each session can acquire its own slot. Clones are
# created ON DEMAND — not pre-allocated — to save disk.
#
# Usage:
#   ghidra_pool.sh init              — One-time setup of pool dir; verify master exists
#   ghidra_pool.sh acquire           — Print first available slot name; create if needed
#   ghidra_pool.sh acquire <N>       — Acquire specifically slot N (creates it if absent;
#                                       fails if it's currently locked)
#   ghidra_pool.sh release [N]       — Clear lock files for slot N (or all if omitted)
#   ghidra_pool.sh sync              — Refresh all unlocked slots from master
#   ghidra_pool.sh status            — Show pool state
#   ghidra_pool.sh cleanup           — Remove stale lock files everywhere
#   ghidra_pool.sh add [N]           — Force-create N additional slots
#   ghidra_pool.sh remove <N>        — Drop slot N (only if unlocked)

set -euo pipefail

PROJECT_ROOT="C:/Users/maria/Desktop/Proyectos/SimGolf"
MASTER_GPR="$PROJECT_ROOT/SimGolf.gpr"
MASTER_REP="$PROJECT_ROOT/SimGolf.rep"
HEADLESS_GPR="$PROJECT_ROOT/SimGolf_headless.gpr"
POOL_DIR="$PROJECT_ROOT/simgolf_pool"
MAX_SLOTS=16

slot_gpr()  { echo "$POOL_DIR/SimGolf_pool${1}.gpr"; }
slot_rep()  { echo "$POOL_DIR/SimGolf_pool${1}.rep"; }
slot_lock() { echo "$POOL_DIR/SimGolf_pool${1}.lock"; }

is_locked() { [[ -f "$(slot_lock "$1")" ]]; }

# Strip "SimGolf_pool" prefix so both "0" and "SimGolf_pool0" are accepted.
normalize_slot() { echo "${1#SimGolf_pool}"; }

next_free_index() {
    for i in $(seq 0 $((MAX_SLOTS - 1))); do
        if [[ ! -f "$(slot_gpr "$i")" ]]; then
            echo "$i"; return 0
        fi
    done
    return 1
}

create_slot() {
    local i="$1"
    local gpr="$(slot_gpr "$i")"
    local rep="$(slot_rep "$i")"
    if [[ ! -f "$MASTER_GPR" || ! -d "$MASTER_REP" ]]; then
        echo "ERROR: Master $MASTER_GPR / $MASTER_REP not found. Run Ghidra GUI once and import golf_clean.exe into a project named 'SimGolf', then retry." >&2
        exit 1
    fi
    echo "Creating slot $i (clone of master $MASTER_REP)..." >&2
    cp "$MASTER_GPR" "$gpr"
    [[ -d "$rep" ]] && rm -rf "$rep"
    cp -r "$MASTER_REP" "$rep"
    rm -f "$(slot_lock "$i")" "$(slot_lock "$i")~"
}

cmd_init() {
    mkdir -p "$POOL_DIR"
    if [[ ! -f "$MASTER_GPR" ]]; then
        echo "WARNING: Master $MASTER_GPR not found yet."
        echo "         Open Ghidra GUI, create project 'SimGolf' at $PROJECT_ROOT,"
        echo "         import golf_clean.exe, run auto-analysis, then save & exit."
        echo "         (Optionally also create 'SimGolf_headless' for the headless MCP backend.)"
        exit 0
    fi
    echo "Pool dir ready: $POOL_DIR"
    echo "Master:        $MASTER_GPR"
    [[ -f "$HEADLESS_GPR" ]] && echo "Headless:      $HEADLESS_GPR" || echo "Headless:      not created (optional)"
    cmd_status
}

_try_acquire() {
    for gpr in "$POOL_DIR"/SimGolf_pool*.gpr; do
        [[ -f "$gpr" ]] || continue
        local slot
        slot=$(echo "$gpr" | sed 's/.*pool\([0-9]*\).*/\1/')
        if ! is_locked "$slot"; then echo "SimGolf_pool${slot}"; return 0; fi
    done
    local idx
    if ! idx=$(next_free_index); then return 1; fi
    create_slot "$idx"
    echo "SimGolf_pool${idx}"
    return 0
}

_acquire_specific() {
    # Acquire a specific slot by index. Creates the slot if absent. Fails (exit 1)
    # if the slot is currently locked.
    local i="$1"
    local lock="$(slot_lock "$i")"
    local gpr="$(slot_gpr "$i")"
    if is_locked "$i"; then
        echo "ERROR: slot $i is currently LOCKED ($lock exists)" >&2
        return 1
    fi
    if [[ ! -f "$gpr" ]]; then
        create_slot "$i"
    fi
    # Stake the lock so concurrent acquires don't grab the same slot.
    : > "$lock"
    echo "SimGolf_pool${i}"
}

cmd_acquire() {
    [[ ! -d "$POOL_DIR" ]] && { mkdir -p "$POOL_DIR"; }
    # If a slot was explicitly requested (`acquire 3` or `acquire SimGolf_pool3`),
    # honour it. This was previously ignored — sub-agents that wanted slot N got
    # whatever the first-free pick was, defeating the per-session slot
    # pre-assignment used by parallel-fanout batches.
    if [[ -n "${1:-}" ]]; then
        local idx; idx=$(normalize_slot "$1")
        if [[ ! "$idx" =~ ^[0-9]+$ ]] || (( idx < 0 )) || (( idx >= MAX_SLOTS )); then
            echo "ERROR: invalid slot index '$1' (must be 0..$((MAX_SLOTS - 1)))" >&2
            exit 1
        fi
        if _acquire_specific "$idx"; then exit 0; fi
        exit 1
    fi
    local result
    if result=$(_try_acquire); then
        # Also stake the lock so a subsequent acquire-without-args doesn't
        # hand out the same slot.
        local idx; idx=$(normalize_slot "$result")
        : > "$(slot_lock "$idx")"
        echo "$result"; exit 0
    fi
    # All MAX_SLOTS exist and are locked — auto-cleanup stale locks then retry once.
    echo "WARNING: all $MAX_SLOTS slots locked; running cleanup and retrying..." >&2
    cmd_cleanup >&2
    if result=$(_try_acquire); then
        local idx; idx=$(normalize_slot "$result")
        : > "$(slot_lock "$idx")"
        echo "$result"; exit 0
    fi
    echo "ERROR: All $MAX_SLOTS slots still locked after cleanup. Run 'ghidra_pool.sh status' to inspect." >&2
    exit 1
}

cmd_release() {
    if [[ -n "${1:-}" ]]; then
        local idx; idx=$(normalize_slot "$1")
        rm -f "$(slot_lock "$idx")" "$(slot_lock "$idx")~"
        echo "Released slot $idx"
    else
        rm -f "$POOL_DIR"/*.lock "$POOL_DIR"/*.lock~ 2>/dev/null || true
        echo "Released all pool slots"
    fi
}

cmd_sync() {
    [[ ! -d "$POOL_DIR" ]] && { echo "ERROR: pool not initialized" >&2; exit 1; }
    echo "Syncing pool slots from master..."
    # Stamp BEFORE the cp loop so any slot refreshed below has mtime > .last_master_sync.
    # Locked slots keep their pre-sync mtime, which marks them stale to ghidra_assert.sh.
    : > "$PROJECT_ROOT/.last_master_sync"
    local synced=0 skipped=0
    for gpr in "$POOL_DIR"/SimGolf_pool*.gpr; do
        [[ -f "$gpr" ]] || continue
        local slot
        slot=$(echo "$gpr" | sed 's/.*pool\([0-9]*\).*/\1/')
        local rep="$(slot_rep "$slot")"
        if is_locked "$slot"; then
            echo "  Slot $slot: LOCKED (skipped)"; skipped=$((skipped+1))
        else
            echo "  Slot $slot: refreshing..."
            rm -rf "$rep"; cp -r "$MASTER_REP" "$rep"; synced=$((synced+1))
        fi
    done
    rm -f "$PROJECT_ROOT/SimGolf.lock" "$PROJECT_ROOT/SimGolf.lock~"
    rm -f "$PROJECT_ROOT/SimGolf_headless.lock" "$PROJECT_ROOT/SimGolf_headless.lock~"
    echo "Sync done: $synced refreshed, $skipped skipped"
}

cmd_status() {
    if [[ ! -d "$POOL_DIR" ]]; then echo "Pool not initialized."; exit 0; fi
    echo "SimGolf Ghidra Pool"
    echo "  Master: $MASTER_GPR"
    echo "  Pool:   $POOL_DIR"
    echo "  Max:    $MAX_SLOTS slots (on-demand)"
    local n=0
    for gpr in "$POOL_DIR"/SimGolf_pool*.gpr; do
        [[ -f "$gpr" ]] || continue
        local slot
        slot=$(echo "$gpr" | sed 's/.*pool\([0-9]*\).*/\1/')
        if is_locked "$slot"; then echo "  Slot $slot: LOCKED"; else echo "  Slot $slot: available"; fi
        ((n++))
    done
    [[ $n -eq 0 ]] && echo "  (no slots created yet — first acquire will create slot 0)"
    echo ""
    [[ -f "$PROJECT_ROOT/SimGolf.lock" ]] && echo "  Master SimGolf.gpr: LOCKED" || echo "  Master SimGolf.gpr: available"
    [[ -f "$PROJECT_ROOT/SimGolf_headless.lock" ]] && echo "  SimGolf_headless.gpr: LOCKED" || echo "  SimGolf_headless.gpr: available"
}

cmd_cleanup() {
    echo "Cleaning all stale Ghidra lock files..."
    rm -f "$POOL_DIR"/*.lock "$POOL_DIR"/*.lock~ 2>/dev/null || true
    rm -f "$PROJECT_ROOT/SimGolf.lock" "$PROJECT_ROOT/SimGolf.lock~"
    rm -f "$PROJECT_ROOT/SimGolf_headless.lock" "$PROJECT_ROOT/SimGolf_headless.lock~"
    echo "Done."
}

cmd_add() {
    local n="${1:-1}"
    for ((k=0; k<n; k+=1)); do
        local idx
        if ! idx=$(next_free_index); then
            echo "ERROR: pool full ($MAX_SLOTS)" >&2; exit 1
        fi
        create_slot "$idx"
    done
    cmd_status
}

cmd_remove() {
    local i="${1:-}"
    [[ -z "$i" ]] && { echo "Usage: ghidra_pool.sh remove <N>" >&2; exit 1; }
    i=$(normalize_slot "$i")
    if is_locked "$i"; then echo "ERROR: slot $i is LOCKED" >&2; exit 1; fi
    rm -f "$(slot_gpr "$i")"
    rm -rf "$(slot_rep "$i")"
    echo "Removed slot $i"
}

case "${1:-status}" in
    init)    cmd_init ;;
    acquire) cmd_acquire "${2:-}" ;;
    release) cmd_release "${2:-}" ;;
    sync)    cmd_sync ;;
    status)  cmd_status ;;
    cleanup) cmd_cleanup ;;
    add)     cmd_add "${2:-}" ;;
    remove)  cmd_remove "${2:-}" ;;
    *)
        echo "Usage: ghidra_pool.sh {init|acquire|release|sync|status|cleanup|add|remove}"
        exit 1 ;;
esac
