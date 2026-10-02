#!/bin/bash
# ghidra_assert.sh — pre-flight guards against common Ghidra-session hallucinations.
#
# Pure file checks. MCP-level asserts (open-program path match, RVA existence,
# write-tool gating on scribe role) are agent-side procedural rules documented
# in .claude/skills/ghidra-pool/SKILL.md and .claude/skills/multi-session/SKILL.md.
#
# Usage:
#   ghidra_assert.sh slot-match <SimGolf_poolN>   verify .pool_slot matches expected
#   ghidra_assert.sh staleness  [slot]           fail if slot is older than last master sync
#   ghidra_assert.sh scribe-check                report whether master.WIP-* flag is held
#   ghidra_assert.sh preflight  <SimGolf_poolN>   run slot-match + staleness + scribe-check

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cmd="${1:-help}"

cmd_slot_match() {
    local expected="${1:?usage: slot-match <SimGolf_poolN>}"
    local pool_slot_file="$ROOT/.pool_slot"
    if [[ ! -f "$pool_slot_file" ]]; then
        echo "FAIL slot-match: no .pool_slot file at $pool_slot_file" >&2
        exit 1
    fi
    local actual
    actual="$(tr -d '[:space:]' < "$pool_slot_file")"
    if [[ "$actual" != "$expected" ]]; then
        echo "FAIL slot-match: .pool_slot=$actual, expected=$expected" >&2
        exit 1
    fi
    echo "ok slot-match: bound to $actual"
}

cmd_staleness() {
    local slot="${1:-}"
    if [[ -z "$slot" && -f "$ROOT/.pool_slot" ]]; then
        slot="$(tr -d '[:space:]' < "$ROOT/.pool_slot")"
    fi
    if [[ -z "$slot" ]]; then
        echo "FAIL staleness: no slot specified and no .pool_slot bound" >&2
        exit 2
    fi
    local sync_stamp="$ROOT/.last_master_sync"
    local slot_rep="$ROOT/simgolf_pool/${slot}.rep"
    if [[ ! -d "$slot_rep" ]]; then
        echo "FAIL staleness: slot rep missing at $slot_rep" >&2
        exit 2
    fi
    if [[ ! -f "$sync_stamp" ]]; then
        echo "ok staleness: no master sync recorded yet (slot $slot assumed current)"
        return 0
    fi
    if [[ "$sync_stamp" -nt "$slot_rep" ]]; then
        echo "FAIL staleness: slot $slot pre-dates last master sync — release + re-acquire to pick up master writes" >&2
        exit 1
    fi
    echo "ok staleness: $slot is current vs master"
}

cmd_scribe_check() {
    local flags
    flags=$(ls "$ROOT"/master.WIP-* 2>/dev/null || true)
    if [[ -n "$flags" ]]; then
        echo "MASTER HELD by scribe(s):"
        echo "$flags"
        return 0
    fi
    echo "ok scribe-check: no master.WIP-* flag; master is free"
}

cmd_preflight() {
    local expected="${1:?usage: preflight <SimGolf_poolN>}"
    cmd_slot_match "$expected"
    cmd_staleness  "$expected"
    cmd_scribe_check
}

case "$cmd" in
    slot-match)   shift; cmd_slot_match   "$@" ;;
    staleness)    shift; cmd_staleness    "$@" ;;
    scribe-check)        cmd_scribe_check       ;;
    preflight)    shift; cmd_preflight    "$@" ;;
    help|*)
        cat <<'EOF'
ghidra_assert.sh — guards against hallucinated Ghidra session control.

Commands:
  slot-match <SimGolf_poolN>   .pool_slot in repo root must match expected slot
  staleness   [slot]          fail if master synced after this slot was refreshed
  scribe-check                report whether master.WIP-* flag is held
  preflight   <SimGolf_poolN>  run all three above in order

MCP-side asserts (agent must perform — not in this script):
  1. After project_program_open_existing, call program_list_open and assert the
     returned project path contains your bound slot name.
  2. Before citing any RVA, call function_at or listing_code_unit_at and assert
     the result is non-null. If null, halt — do not cite the address.
  3. Master writes (decomp_global_rename, function_rename, layout_struct_*, etc.)
     are forbidden unless YOU hold a master.WIP-<sessionid> flag in repo root.
EOF
        ;;
esac
