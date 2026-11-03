#!/bin/sh
# S3 wave 10 (lead): the whole wave gate as one detached chain of single-step holds; log log/roadmap/S3/w10_out/chain.log
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
export PATH="$HOME/bin:$PATH"
mkdir -p log/roadmap/S3/w10_out
sh log/roadmap/S3/w10_run_gates.sh "$@" >> log/roadmap/S3/w10_out/chain.log 2>&1
echo "[chain done] $*" >> log/roadmap/S3/w10_out/chain.log
