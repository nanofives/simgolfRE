#!/bin/sh
# S2.11: the gates on the mirror port/build/S2_11_head, one after another, every output tee'd to log/roadmap/S2/11_*.
# usage: gates.sh [steps...]   (default: all; steps: segs64 segs32 boot census suite64 dlg32)
R=/c/Users/maria/Desktop/Proyectos/SimGolf
M=$R/port/build/S2_11_head
L=$R/log/roadmap/S2
DLL=C:/Users/maria/Desktop/Proyectos/SimGolf/port/build/S2_11/Release/portsim_c.dll
DLL32=C:/Users/maria/Desktop/Proyectos/SimGolf/port/build/S2_11_head/port/build/Win32/Release/portsim_c.dll
STEPS="${*:-segs64 segs32 boot census suite64 dlg32}"
cd "$M" || exit 2
export SG_PORTSIM_DLL="$DLL"
N=$(echo $STEPS | wc -w); K=0
for s in $STEPS; do
  K=$((K + 1))
  case $s in
    segs64) ~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll "$DLL" --jobs 3 --out "$L/11_segs_x64.tsv" > "$L/11_segs_x64.log" 2>&1 ;;
    segs32) ~/bin/py -3.12 -m port.test.integrator segments --backend win32 --dll32 "$DLL32" --jobs 3 --out "$L/11_segs_win32.tsv" > "$L/11_segs_win32.log" 2>&1 ;;
    boot) ~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out "$L/11_boot_gate.tsv" > "$L/11_boot_gate.log" 2>&1 ;;
    census) ~/bin/py -3.12 -m port.test.framegate --all --jobs 3 --no-probe --out "$L/11_census_x64.tsv" > "$L/11_census_x64.log" 2>&1 ;;
    suite64) sh "$L/13_tools/run_files.sh" "$M" "$DLL" x64 "$L/11_x64" $(cd port/test && ls test_*.py) > "$L/11_x64_suite.log" 2>&1 ;;
    dlg32) sh "$L/13_tools/run_files.sh" "$M" "$DLL" win32 "$L/11_w32" test_gfx_dialogs2.py test_gfx.py test_gfx_dialogs.py test_gfx_reports.py > "$L/11_w32_suite.log" 2>&1 ;;
  esac
  echo "[$K/$N] $s rc=$? $(tail -n 1 "$L/11_$( [ $s = segs64 ] && echo segs_x64 || ([ $s = segs32 ] && echo segs_win32) || ([ $s = boot ] && echo boot_gate) || ([ $s = census ] && echo census_x64) || ([ $s = suite64 ] && echo x64_suite) || echo w32_suite).log" | cut -c1-200)"
done
echo "[done]"
