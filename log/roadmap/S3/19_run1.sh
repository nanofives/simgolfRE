#!/bin/bash
# S3.19 first heavy run (inside 19_heavy.sh, cwd = port/build/S3_19): build x64, then the two short test files.
set -o pipefail
~/bin/py -3.12 port/test/build.py --arch x64 2>&1 | tail -25 || exit 1
~/bin/py -3.12 -m pytest port/test/test_static.py -q -p no:cacheprovider 2>&1 | tail -5
~/bin/py -3.12 -m pytest port/test/test_gfx_s3_19.py -q -p no:cacheprovider -rA -x -s 2>&1 | tail -60
