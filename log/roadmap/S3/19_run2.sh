#!/bin/bash
# S3.19 second heavy run (inside 19_heavy.sh, cwd = port/build/S3_19): rebuild x64, then the S3.19 test file.
set -o pipefail
~/bin/py -3.12 port/test/build.py --arch x64 2>&1 | grep -E "error|warning C|Build succeeded|portsim_c.dll" | head -30 || exit 1
~/bin/py -3.12 -m pytest port/test/test_gfx_s3_19.py -q -p no:cacheprovider -rA -s 2>&1 | tail -40
