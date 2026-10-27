# I10: sourced by gates.sh / suite_by_file.sh with cwd = the mirror port/build/I10.
M=/c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I10
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
export SG_PORTSIM_DLL32=$M/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$M/port/build/Win32/Release/portsim_runner32.exe
export SG_APP_EXE=$M/port/build/x64/app/Release/simgolf_port.exe
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
unset SG_TEXT_METRICS SG_EMU_ORACLE

waitmem() {
  while :; do
    FREE=$(powershell -NoProfile -Command "[int]((Get-CimInstance Win32_OperatingSystem).FreeVirtualMemory/1MB)")
    [ "${FREE:-0}" -ge "${MINFREE:-16}" ] && return
    echo "  (free commit ${FREE} GB < ${MINFREE:-16}, waiting)"
    sleep 60
  done
}
