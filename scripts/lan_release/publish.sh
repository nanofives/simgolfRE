#!/usr/bin/env bash
# publish.sh - build the SimGolf port from the committed HEAD, stage the LAN release folder and publish it to the
# mini-server. Run from LIVING-PC; publish_simgolf.bat wraps it for a double-click.
#
#   publish.sh             clean build of HEAD (git archive of port/ + re/model/include), stage, deploy
#   publish.sh --no-build  stage and deploy the last build again (e.g. after a server reset)
#
# The roadmap orchestrator runs it on its own when release_due.py says DUE (every 2 merge rounds, or 1 after 24 h).
#
# The build is a fresh `git archive HEAD` so uncommitted work in the shared tree never ships (2026-10-28: a commit
# that depended on a working-tree file did not build from its archive). Builds into port/build/lan_build, stages into
# port/build/lan_release (both gitignored), then runs deploy_lan_release.sh.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HERE="$ROOT/scripts/lan_release"
BUILD="$ROOT/port/build/lan_build"
STAGE="$ROOT/port/build/lan_release"
INSTALL="$ROOT/original"
APP="$BUILD/port/build/x64/app/Release"
cd "$ROOT"

sha="$(git rev-parse --short HEAD)"
subject="$(git log -1 --format=%s HEAD | cut -c1-60)"
label="SimGolf port $sha ($(git log -1 --format=%cd --date=short HEAD))"

if [ "${1:-}" != "--no-build" ]; then
    echo "[+] Clean build of HEAD $sha: $subject"
    rm -rf "$BUILD"
    mkdir -p "$BUILD"
    git archive HEAD port re/model/include | tar -x -C "$BUILD"
    ( cd "$BUILD" && SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed' py -3.12 port/test/build.py --arch x64 )
fi
[ -f "$APP/simgolf_port.exe" ] || { echo "[!] no $APP/simgolf_port.exe (build failed?)"; exit 1; }

echo "[+] Staging $label"
py -3.12 "$HERE/stage_lan_release.py" --app "$(cygpath -w "$APP")" --install "$(cygpath -w "$INSTALL")" \
    --out "$(cygpath -w "$STAGE")" --label "$label"

echo "[+] Publishing"
bash "$HERE/deploy_lan_release.sh" "$STAGE"

# history of what went out (release_due.py reads the server's manifest, not this file)
mkdir -p "$ROOT/log/roadmap"
printf '%s\t%s\t%s\n' "$(date '+%F %T')" "$sha" "$label" >> "$ROOT/log/roadmap/lan_releases.tsv"
