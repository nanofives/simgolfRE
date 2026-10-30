#!/bin/bash
# S3.19 oracle run (inside 19_heavy.sh, cwd = port/build/S3_19): the emulator call oracle and its controls
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_gfx_s3_19.py -q -p no:cacheprovider -rA -k "original or calls_negctl" 2>&1 | tail -30
