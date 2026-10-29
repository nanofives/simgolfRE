#!/bin/bash
# I17: run the FIXED ghidra/scripts/ApplyNames.java on a scratch copy of the pre-S10 master (ghidra/backup/*.pre_s10_20261029,
# which still holds the ANALYSIS labels that made I16's first pass throw DuplicateNameException) with I16's names dir.
# Expected: DUPLABEL lines, no SCRIPT ERROR, and I16's counts (exe renamed=74, sound renamed=342).
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
T='C:\Users\maria\AppData\Local\Temp\claude\C--Users-maria-Desktop-Proyectos-SimGolf\a8611727-3653-4e04-91c9-2598da4cac05\scratchpad\anproj'
TU=/c/Users/maria/AppData/Local/Temp/claude/C--Users-maria-Desktop-Proyectos-SimGolf/a8611727-3653-4e04-91c9-2598da4cac05/scratchpad/anproj
rm -rf "$TU"; mkdir -p "$TU"
cp -r ghidra/backup/SimGolf.rep.pre_s10_20261029 "$TU/SimGolf.rep"; cp ghidra/backup/SimGolf.gpr.pre_s10_20261029 "$TU/SimGolf.gpr"
AH='C:\Users\maria\Desktop\Proyectos\TD5RE\ghidra\ghidra_12.0.3_PUBLIC\support\analyzeHeadless.bat'
N='C:\Users\maria\Desktop\Proyectos\SimGolf\log\roadmap\merges\I16\names_apply'
for m in golf_clean.exe sound.dll; do
  echo "== $m"
  MSYS_NO_PATHCONV=1 cmd /c "$AH" "$T" SimGolf -process "$m" -noanalysis -scriptPath 'ghidra\scripts' \
    -postScript ApplyNames.java "$N" 2>&1 | grep -E "DUPLABEL|DUPFAIL|APPLYNAMES|SCRIPT ERROR|DuplicateName"
done
rm -rf "$TU"
