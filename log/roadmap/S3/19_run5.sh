#!/bin/bash
# S3.19 (inside 19_heavy.sh, cwd = port/build/S3_19): the test file with the oracle, then without it, then port test_static
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_gfx_s3_19.py -q -p no:cacheprovider -rA 2>&1 | grep -E "PASSED|FAILED|SKIPPED|passed|failed|Error"
~/bin/py -3.12 -m pytest port/test/test_gfx_s3_19.py -q -p no:cacheprovider 2>&1 | tail -3
~/bin/py -3.12 -m pytest port/test/test_static.py -q -p no:cacheprovider 2>&1 | tail -3
