#!/bin/bash
# I16 step 3d: false starts out (jgld / Terrain), extents in (jgld / Terrain 9_boundaries, sound 6_setbodies_args), re-export.
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
L=log/roadmap/merges/I16; G=$L/gh.sh
for m in jgld.dll Terrain.dll; do bash $G $m DeleteFunctions.java $(cat $L/del_$m.txt); bash $G $m SetBodies.java $(cat $L/set_$m.txt); done
bash $G sound.dll SetBodies.java $(cat log/roadmap/S10/6_setbodies_args.txt)
for m in sound.dll jgld.dll Terrain.dll; do bash $G $m ListFunctions.java; done
echo "ghidra_d done"
