#!/bin/sh
# S3.13: run log/roadmap/S3/13_gates.sh with the short steps first (segments, boot, FreeType, chain, ledger, gfx_check, census,
# terrain), then static, the x64 suite and the Win32 suite. Detached with nohup; progress in log/roadmap/S3/13_gates.log.
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/S3/13_gates.sh
sh $G 3 6; sh $G 8 14; sh $G 1 2; sh $G 7 7
echo "[all done]"
