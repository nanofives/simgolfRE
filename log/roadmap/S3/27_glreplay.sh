#!/bin/sh
# S3.27: replay the original's recorded passes through Windows' GL (re/tools/glexact/glreplay.exe, read-only use) and keep the result
R=log/p5/gl/corpus/s327_s2; O=log/p5/s327/replay; mkdir -p $O
for s in "$@"; do re/tools/glexact/glreplay.exe $R $s $O/$s.bin; echo "$s rc=$?"; done
