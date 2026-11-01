#!/bin/bash
# S3.26: one heavy hold per scenario (w8_lock.py run), each a 26_snap.py boot in the S3_26 tree. Args: [--raster] scenario ...
S=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
PY=/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe
EXTRA=""
if [ "$1" = "--raster" ]; then EXTRA="--raster"; shift; fi
TAG=${TAG:-host}
for sc in "$@"; do
  echo "[snap] $sc $TAG start $(date +%T)"
  ~/bin/py -3.12 $S/w8_lock.py run S3.26 --what "snap $sc $TAG" -- bash $S/26_run.sh $PY $S/26_snap.py $sc $EXTRA --out $S/26_img/$TAG > $S/26_snap_${sc}_$TAG.log 2>&1
  echo "[snap] $sc $TAG rc=$? $(date +%T)"; grep -v "^\[w8_lock\]" $S/26_snap_${sc}_$TAG.log | tail -12
done
