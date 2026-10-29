#!/bin/bash
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
for f in re/match/smsg_*.cpp; do ~/bin/py -3.12 re/tools/match.py "$f"; echo "rc=$? $f"; done
echo "evidence done"
