#!/bin/bash
# S3.19: run one heavy command under the wave 7 token (log/roadmap/S3/W7_HEAVY_RULE.md), from the S3.19 mirror.
#   bash log/roadmap/S3/19_heavy.sh <what> <log> <command...>
# Waits for the token (mkdir), writes owner.txt, runs the command with the mirror's env, releases the token.
S3=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
LOCK=$S3/w7_heavy.lock
WHAT=$1; LOG=$2; shift 2
until mkdir "$LOCK" 2>/dev/null; do sleep 5; done
echo "S3.19 $WHAT $(date '+%Y-%m-%d %H:%M:%S')" > "$LOCK/owner.txt"
trap 'rm -rf "$LOCK"' EXIT
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_19 || exit 2
export TREE=S3_19
source "$S3/18_env.sh"
echo "== $(date '+%H:%M:%S') $*"
"$@" 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
echo "== $(date '+%H:%M:%S') rc=$rc" | tee -a "$LOG"
exit $rc
