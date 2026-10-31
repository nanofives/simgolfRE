#!/bin/sh
# P2: one pytest file in the mirror. $1 = backend (x64|win32), $2 = file, rest = pytest args
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/P2 || exit 1
B=$1; F=$2; shift 2
SG_BACKEND=$B ~/bin/py -3.12 -m pytest port/test/$F.py -q -p no:cacheprovider "$@" > ../../../log/roadmap/P2/t_${B}_$F.log 2>&1; echo "rc=$?" >> ../../../log/roadmap/P2/t_${B}_$F.log
