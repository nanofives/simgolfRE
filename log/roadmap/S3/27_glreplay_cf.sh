#!/bin/sh
# S3.27: replay one step of the s327_s2b corpus through Windows' GL (glreplay.exe, read-only use)
R=log/p5/gl/corpus/s327_s2b; O=log/p5/s327/replay; mkdir -p $O
for s in "$@"; do re/tools/glexact/glreplay.exe $R $s $O/$s.bin; echo "$s rc=$?"; done
