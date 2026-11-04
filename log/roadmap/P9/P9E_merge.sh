#!/usr/bin/env bash
# P9E_merge.sh <n> "<commit subject>" : apply log/roadmap/P9/P9E_<n>.patch, commit with the P9E deliverables, publish (LAN +
# GitHub), commit the release logs and push. The orchestrator's loop step for each P9E fix.
set -euo pipefail
cd "$(dirname "$0")/../../.."
n="$1"; subject="$2"; P=log/roadmap/P9
git apply --check "$P/P9E_$n.patch"
git apply "$P/P9E_$n.patch"
git add port/ re/history/ROADMAP.md
git add -f "$P/P9E_BRIEF.md" "$P/P9E_mirror.py" "$P/P9E_merge.sh" "$P/P9E_notes.md" "$P/P9E_$n.patch" $(ls "$P"/P9E_${n}_files.txt "$P"/P9E_${n}_gates.log 2>/dev/null)
for f in "${@:3}"; do git add -f "$f"; done
trailer=$'\n\nGenerated with [Claude Code](https://claude.ai/code)\nvia [Happy](https://happy.engineering)\n\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\nCo-Authored-By: Happy <yesreply@happy.engineering>'
git commit -q -m "$subject$trailer"
sha=$(git rev-parse --short HEAD)
bash scripts/lan_release/publish.sh > "$P/P9E_${n}_publish.log" 2>&1
tail -4 "$P/P9E_${n}_publish.log"
git add -f log/roadmap/github_releases.tsv log/roadmap/lan_releases.tsv
git commit -q -m "roadmap: LAN release $sha published (P9E fix $n) and GitHub release$trailer"
git push -q origin main
git log --oneline -2
