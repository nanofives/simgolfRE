#!/bin/bash
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I16/gh.sh
for m in golf_clean.exe sound.dll jgld.dll Terrain.dll; do bash $G $m ApplyNames.java 're\names'; done
for m in golf_clean.exe sound.dll jgld.dll Terrain.dll; do bash $G $m ListFunctions.java; done
echo "ghidra_c done"
