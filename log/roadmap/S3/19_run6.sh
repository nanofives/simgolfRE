#!/bin/bash
# S3.19 (inside 19_heavy.sh): the call oracle with both scroll arms asserted, then the default file
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_gfx_s3_19.py -q -p no:cacheprovider -k "original or calls_negctl" 2>&1 | tail -3
~/bin/py -3.12 -m pytest port/test/test_gfx_s3_19.py -q -p no:cacheprovider 2>&1 | tail -2
