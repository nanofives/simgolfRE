#!/bin/bash
# I16 step 3c: ApplyNamesI16 (ApplyNames + duplicate-label retry) on the filtered names dir, then the four exports.
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I16/gh.sh
SPX='log\roadmap\merges\I16\ghidra'
N='C:\Users\maria\Desktop\Proyectos\SimGolf\log\roadmap\merges\I16\names_apply'
for m in golf_clean.exe sound.dll jgld.dll Terrain.dll; do SP="$SPX" bash $G $m ApplyNamesI16.java "$N" $OVERWRITE; done
for m in golf_clean.exe sound.dll jgld.dll Terrain.dll; do bash $G $m ListFunctions.java; done
echo "ghidra_c done"
