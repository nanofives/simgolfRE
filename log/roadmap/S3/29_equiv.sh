#!/bin/sh
# S3.29: S3.10 bit-identity (12_equiv.py) on port/build/S3_29, default and with the textured walk on (SG_RASTER_TUNE=32=1);
# each run one w8_lock hold. Logs / TSVs in log/roadmap/S3/29_out/.
cd "$(dirname "$0")/../../.."
PYX=/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe
export SG_TREE=$PWD/port/build/S3_29
export SG_PORTSIM_DLL=$SG_TREE/port/build/x64/Release/portsim_c.dll
for cfg in default 32=1; do
  if [ "$cfg" = default ]; then unset SG_RASTER_TUNE; else export SG_RASTER_TUNE=$cfg; fi
  tag=$(echo $cfg | tr '=' '_')
  ~/bin/py -3.12 log/roadmap/S3/w8_lock.py run S3.29 --what "12_equiv $cfg" -- $PYX log/roadmap/S3/12_equiv.py --out log/roadmap/S3/29_out/equiv_$tag.tsv > log/roadmap/S3/29_out/equiv_$tag.log 2>&1
  echo "$cfg rc=$?"; tail -2 log/roadmap/S3/29_out/equiv_$tag.log
done
