#!/usr/bin/env bash
# deploy_lan_release.sh - publish the staged SimGolf port build to the mini-server (copy of SimCity's
# re/scripts/lan_release/deploy_lan_release.sh, itself a copy of TD5RE's scripts/deploy_release.sh). Run from LIVING-PC
# (Git Bash) after stage_lan_release.py; publish.sh does both.
#
# Served by the existing td5re-files nginx (DockerApps, port 8088) from a subfolder, so the server needs no new
# service, port or firewall rule: http://mariano-server.local:8088/simgolf/. TD5RE's deploy never deletes outside
# its re/assets, so the subfolder survives TD5RE publishes (as simcity/ does).
#
# Upload: first publish = the whole folder as one tar over SSH; later publishes = diff the local manifest against
# the server's and send only the changed files (then the manifest last, so a client never sees a manifest that
# names a file not uploaded yet). Then every server file the new manifest no longer lists is deleted (no old copies kept).
set -euo pipefail

PI_HOST="${PI_HOST:-mariano@mariano-server.local}"
SSH_KEY="${SSH_KEY:-$HOME/.ssh/id_ed25519_rpi}"
REMOTE_DIR="${REMOTE_DIR:-/home/mariano/td5re-release/simgolf}"
FILES_PORT="${FILES_PORT:-8088}"
STAGE="${1:-$HOME/Desktop/Proyectos/SimGolf/port/build/lan_release}"
SSH_OPTS="-o ConnectTimeout=15 -o BatchMode=yes -o StrictHostKeyChecking=accept-new"
SSH_CMD="ssh -i $SSH_KEY $SSH_OPTS"

[ -f "$STAGE/manifest.json" ] || { echo "[!] no manifest.json in $STAGE (run stage_lan_release.py first)"; exit 1; }

echo "[+] Pre-flight: $PI_HOST over SSH ..."
if ! $SSH_CMD "$PI_HOST" true 2>/dev/null; then
    echo "[!] $PI_HOST not reachable with $SSH_KEY. Nothing was published."
    exit 2
fi
$SSH_CMD "$PI_HOST" "mkdir -p '$REMOTE_DIR' && df -h '$REMOTE_DIR' | tail -1"

tmp_remote="$(mktemp)"; list="$(mktemp)"
trap 'rm -f "$tmp_remote" "$list"' EXIT
$SSH_CMD "$PI_HOST" "cat '$REMOTE_DIR/manifest.json' 2>/dev/null" > "$tmp_remote" || true

py -3.12 - "$(cygpath -w "$STAGE/manifest.json")" "$(cygpath -w "$tmp_remote")" "$(cygpath -w "$list")" <<'EOF'
import json, sys
local = json.load(open(sys.argv[1], encoding="utf-8"))
try:
    remote = {f["path"]: f["sha256"] for f in json.load(open(sys.argv[2], encoding="utf-8"))["files"]}
except Exception:
    remote = {}
changed = [f for f in local["files"] if remote.get(f["path"]) != f["sha256"]]
with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as fh:
    fh.write("".join(f["path"] + "\n" for f in changed))
print("    %s publish: %d of %d file(s) to send, %.1f MB" % ("incremental" if remote else "first", len(changed),
      len(local["files"]), sum(f["size"] for f in changed) / 1e6))
EOF

if [ -s "$list" ]; then
    tar --hard-dereference -czf - -C "$STAGE" -T "$list" | $SSH_CMD "$PI_HOST" "tar -xzmf - -C '$REMOTE_DIR'"
fi
tar -czf - -C "$STAGE" manifest.json | $SSH_CMD "$PI_HOST" "tar -xzmf - -C '$REMOTE_DIR'"

# Old copies: delete every file on the server the new manifest no longer lists (Mariano, 2026-10-29), after the new
# manifest is in place so no client is pointed at a removed file. Only inside REMOTE_DIR, and refused when more than half
# of the server's files would go (a broken manifest), unless PRUNE_FORCE=1.
case "$REMOTE_DIR" in */simgolf) ;; *) echo "[!] REMOTE_DIR $REMOTE_DIR does not end in /simgolf: no prune"; exit 3 ;; esac
remote_list="$(mktemp)"; stale="$(mktemp)"
trap 'rm -f "$tmp_remote" "$list" "$remote_list" "$stale"' EXIT
$SSH_CMD "$PI_HOST" "cd '$REMOTE_DIR' && find . -type f | sed 's|^\./||'" > "$remote_list"
py -3.12 - "$(cygpath -w "$STAGE/manifest.json")" "$(cygpath -w "$remote_list")" "$(cygpath -w "$stale")" "${PRUNE_FORCE:-0}" <<'EOF'
import json, sys
keep = {f["path"] for f in json.load(open(sys.argv[1], encoding="utf-8"))["files"]} | {"manifest.json"}
have = [l.rstrip("\r\n") for l in open(sys.argv[2], encoding="utf-8") if l.strip()]
gone = sorted(p for p in have if p not in keep)
if gone and len(gone) * 2 > len(have) and sys.argv[4] != "1":
    print("[!] prune refused: %d of %d server files are not in the manifest (PRUNE_FORCE=1 to allow)" % (len(gone), len(have)))
    gone = []
open(sys.argv[3], "wb").write(b"".join(p.encode("utf-8") + b"\0" for p in gone))
print("    prune: %d old file(s) to delete of %d on the server%s" % (len(gone), len(have), "".join("\n      - " + p for p in gone[:20])))
EOF
if [ -s "$stale" ]; then
    $SSH_CMD "$PI_HOST" "cd '$REMOTE_DIR' && xargs -0 rm -f -- && find . -mindepth 1 -type d -empty -delete" < "$stale"
fi

echo "[+] Verifying through nginx on the server ..."
$SSH_CMD "$PI_HOST" "curl -fsS -o /dev/null http://localhost:$FILES_PORT/simgolf/manifest.json && echo '    OK: http://mariano-server.local:$FILES_PORT/simgolf/manifest.json'"
echo "[+] Done. On each PC: put update.bat in an empty folder (first time) and run it; it fetches the rest."
