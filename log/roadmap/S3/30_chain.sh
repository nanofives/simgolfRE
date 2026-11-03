#!/bin/sh
# S3.30: the remaining gate steps in one detached chain (started with PowerShell Start-Process; log 30_work/chain.log)
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
export PATH="$HOME/bin:$PATH"
sh log/roadmap/S3/30_run_gates.sh "$@" 2>&1 | grep -v "slot [0-9]:\|test-slot\]" >> log/roadmap/S3/30_work/chain.log
echo "[chain done] $*" >> log/roadmap/S3/30_work/chain.log
