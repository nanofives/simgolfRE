#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
L=log/roadmap/S4
PY="$HOME/bin/py -3.12"
$PY -m port.test.integrator segments --dll port/build/S4_9_mirror/x64/Release/portsim_c.dll --jobs 3 --out $L/9_segments_mirror.tsv > $L/9_segments_mirror.log 2>&1
$PY -m port.test.integrator segments --backend win32 --dll32 port/build/S4_9_mirror/w32/Release/portsim_c.dll --jobs 3 --out $L/9_segments_mirror_w32.tsv > $L/9_segments_mirror_w32.log 2>&1
SG_PORTSIM_DLL=port/build/S4_9_mirror/x64/Release/portsim_c.dll $PY -m port.test.boot --all --jobs 3 --out $L/9_boot_gate_mirror.tsv > $L/9_boot_gate_mirror.log 2>&1
echo done > $L/9_gates.done
