#!/usr/bin/env bash
# S3.21: the wave-8 heavy-run lock (log/roadmap/S3/W8_HEAVY_RULE.md).
#   21_lock.sh acquire "<what>"   waits until the lock is free and free commit >= 16 GB, then takes it
#   21_lock.sh release            removes the lock if owner.txt names S3.21
L=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3/w8_heavy.lock
CF=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3/w8_commit_free.py
case "$1" in
  acquire)
    while true; do
      if ~/bin/py -3.12 "$CF" >/dev/null 2>&1 && mkdir "$L" 2>/dev/null; then
        echo "S3.21 $2 $(date +%H:%M)" > "$L/owner.txt"
        echo "acquired $(date +%H:%M:%S)"
        exit 0
      fi
      sleep 3
    done ;;
  release)
    if grep -q "^S3.21 " "$L/owner.txt" 2>/dev/null; then rm -rf "$L"; echo "released $(date +%H:%M:%S)"; else echo "not ours"; fi ;;
esac
