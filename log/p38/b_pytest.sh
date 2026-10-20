#!/bin/sh
# 38b: run pytest against the private mirror build (never port/build/x64)
cd "$(dirname "$0")/../.." || exit 1
DLL=$(ls -t port/build/p38b_run/*.dll | head -1)
[ -z "$DLL" ] && { echo "no mirror DLL: run log/p38/b_mirror.py --build"; exit 1; }
export SG_PORTSIM_DLL="$DLL"
exec ~/bin/py -3.12 -m pytest -p no:cacheprovider "$@"
