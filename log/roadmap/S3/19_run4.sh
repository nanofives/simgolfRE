#!/bin/bash
# S3.19 (inside 19_heavy.sh, cwd = port/build/S3_19): rebuild x64, then the whole test file with the emulator oracle (update mode
# prints the call-log digest)
set -o pipefail
~/bin/py -3.12 port/test/build.py --arch x64 2>&1 | grep -E "error|warning C|portsim_c.dll" | head -30 || exit 1
SG_EMU_ORACLE=update ~/bin/py -3.12 -m pytest port/test/test_gfx_s3_19.py -q -p no:cacheprovider -rA -s 2>&1 | tail -40
