#!/bin/bash
# S3.24: the default census (GlRaster terrain) under a line rule, in batches of 14 scenarios (one lock hold each, 24_turn.sh).
# usage: 24_census.sh <rule> <batch index 0..9>   -> log/roadmap/S3/24_out/census_r<rule>.tsv (+ .d per scenario; --resume)
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_24 || exit 1
export SG_PORTSIM_DLL="$(pwd -W)/port/build/x64/Release/portsim_c.dll"
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_OVERLAY_RULE=$1
O=../../../log/roadmap/S3/24_out; mkdir -p $O
SC=$(sed -n "$(( $2 * 14 + 1 )),$(( $2 * 14 + 14 ))p" ../../../log/roadmap/S3/24_census_scen.txt | tr '\n' ' ')
[ -z "$SC" ] && { echo "empty batch"; exit 0; }
echo "batch $2: $SC"
~/bin/py -3.12 -m port.test.framegate $SC --jobs 1 --no-probe --out $O/census_r$1_b$2.tsv --resume
