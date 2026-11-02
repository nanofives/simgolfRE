# I22: sourced by gates.sh / suite_by_file.sh with cwd = the mirror port/build/I22 (copy of I20 env.sh).
M=/c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I22
R=/c/Users/maria/Desktop/Proyectos/SimGolf
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
export SG_PORTSIM_DLL32=$M/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$M/port/build/Win32/Release/portsim_runner32.exe
export SG_APP_EXE=$M/port/build/x64/app/Release/simgolf_port.exe
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
unset SG_TEXT_METRICS SG_EMU_ORACLE SG_APP_PRIORITY   # SG_BACKEND is the caller's (suite_by_file.sh win32 pass)

waitmem() {
  while :; do
    FREE=$(powershell -NoProfile -Command "[int]((Get-CimInstance Win32_OperatingSystem).FreeVirtualMemory/1MB)")
    [ "${FREE:-0}" -ge "${MINFREE:-16}" ] && return
    echo "  (free commit ${FREE} GB < ${MINFREE:-16}, waiting)"
    sleep 60
  done
}

# U-1398 resolved by wave 6: the app tests write their facts to SG_APP_FACTS_DIR.
export SG_APP_FACTS_DIR=C:/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I22/facts
mkdir -p $SG_APP_FACTS_DIR
restore_facts() { :; }
export SG_TREE=$M
# I22: one S3 heavy run at a time with the S3 lead (log/roadmap/S3/w8_lock.py; take checks >= 16 GB free commit itself)
LOCKPY="$HOME/bin/py -3.12 $R/log/roadmap/S3/w8_lock.py"
lock_take() { $LOCKPY take I22 --priority --what "$1" --need-gb ${MINFREE:-16} > /dev/null 2>&1; }
lock_release() { $LOCKPY release I22 > /dev/null 2>&1; }
