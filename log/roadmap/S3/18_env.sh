# S3.18: sourced by 17_gates.sh with cwd = the tree (port/build/S3_18); a copy of 12_env.sh.
M=/c/Users/maria/Desktop/Proyectos/SimGolf/port/build/${TREE:-S3_18}
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
export SG_PORTSIM_DLL32=$M/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$M/port/build/Win32/Release/portsim_runner32.exe
export SG_APP_EXE=$M/port/build/x64/app/Release/simgolf_port.exe
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_TREE=$M
unset SG_TEXT_METRICS SG_EMU_ORACLE SG_APP_PRIORITY SG_RASTER_THREADS SG_TERRAIN_PASS_LOG

waitmem() {
  while :; do
    FREE=$(powershell -NoProfile -Command "[int]((Get-CimInstance Win32_OperatingSystem).FreeVirtualMemory/1MB)")
    [ "${FREE:-0}" -ge "${MINFREE:-16}" ] && return
    echo "  (free commit ${FREE} GB < ${MINFREE:-16}, waiting)"
    sleep 60
  done
}
