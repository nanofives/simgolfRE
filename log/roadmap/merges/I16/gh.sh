#!/bin/bash
# I16: one headless Ghidra run on the master.  gh.sh <module> <Script.java> [script args...]
# ListFunctions with no args writes the module's re/functions_ghidra*.tsv (path chosen here).
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
MOD="$1"; SCRIPT="$2"; shift 2
AH='C:\Users\maria\Desktop\Proyectos\TD5RE\ghidra\ghidra_12.0.3_PUBLIC\support\analyzeHeadless.bat'
EXTRA=()
if [ "$SCRIPT" = "ListFunctions.java" ] && [ $# -eq 0 ]; then
  RE='C:\Users\maria\Desktop\Proyectos\SimGolf\re'
  if [ "$MOD" = "golf_clean.exe" ]; then set -- "$RE\\functions_ghidra.tsv"; else set -- "$RE\\functions_ghidra_$MOD.tsv"; fi
  EXTRA=(-readOnly)
fi
echo "== $(date -Iseconds) $MOD $SCRIPT $* ${EXTRA[*]}"
MSYS_NO_PATHCONV=1 cmd /c "$AH" 'C:\Users\maria\Desktop\Proyectos\SimGolf' SimGolf -process "$MOD" -noanalysis \
  -scriptPath "${SP:-ghidra\\scripts}" -postScript "$SCRIPT" "$@" "${EXTRA[@]}" 2>&1 \
  | grep -v "^INFO  \(Loading\|Using\|HEADLESS\|Searching\|Opening\|REPORT\|Headless\|Analyzing\)"
echo "rc=${PIPESTATUS[0]}"
