#!/bin/sh
# S2.10: the heavy gates on the integration mirror, one after another, every output tee'd to log/roadmap/S2/10_*.
# usage: gates.sh  (after the x64 suite finished: it rebuilds the x64 DLL first)
R=/c/Users/maria/Desktop/Proyectos/SimGolf
M=$R/port/build/S2_10_head
L=$R/log/roadmap/S2
DLL=C:/Users/maria/Desktop/Proyectos/SimGolf/port/build/S2_10/Release/portsim_c.dll
DLL32=C:/Users/maria/Desktop/Proyectos/SimGolf/port/build/S2_10_head/port/build/Win32/Release/portsim_c.dll
cd "$R" || exit 2
echo "[1/7] rebuild x64"
~/bin/py -3.12 log/roadmap/S2/build.py "$M" C:/Users/maria/Desktop/Proyectos/SimGolf/port/build/S2_10 --log log/roadmap/S2/10_build.log > /dev/null 2>&1
echo "[1/7] rebuild x64 rc=$?"
cd "$M" || exit 2
echo "[2/7] rebuild Win32"
~/bin/py -3.12 port/test/build.py --arch Win32 > "$L/10_build_w32.log" 2>&1
echo "[2/7] rebuild Win32 rc=$?"
export SG_PORTSIM_DLL="$DLL"
echo "[3/7] segments x64"
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll "$DLL" --jobs 4 --out "$L/10_segs_x64.tsv" 2>&1 | tee "$L/10_segs_x64.log" | grep -E "^\{|Traceback"
echo "[4/7] segments win32"
~/bin/py -3.12 -m port.test.integrator segments --backend win32 --dll32 "$DLL32" --jobs 4 --out "$L/10_segs_win32.tsv" 2>&1 | tee "$L/10_segs_win32.log" | grep -E "^\{|Traceback"
echo "[5/7] boot gate"
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out "$L/10_boot_gate.tsv" 2>&1 | tee "$L/10_boot_gate.log" | grep -E "^\{|Traceback"
echo "[6/7] census"
~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --no-probe --out "$L/10_census_x64.tsv" 2>&1 | tee "$L/10_census_x64.log" | grep -E "^\{|Traceback"
echo "[7/7] Win32 gfx files"
sh "$L/13_tools/run_files.sh" "$M" "$DLL" win32 "$L/10_w32" test_gfx.py test_gfx_credits.py test_gfx_routing.py test_gfx_boot_screens.py test_gfx_reports.py test_gfx_dialogs.py test_s2_6_scenario.py test_buyland.py test_dialogs.py test_chareditor.py
echo "[done]"
