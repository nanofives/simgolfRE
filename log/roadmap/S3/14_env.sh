# S3.14: sourced with cwd = a mirror root (port/build/S3_14 or port/build/S3_14_head); points every test at that mirror's builds.
M=$(pwd)
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
export SG_PORTSIM_DLL32=$M/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$M/port/build/Win32/Release/portsim_runner32.exe
export SG_APP_EXE=$M/port/build/x64/app/Release/simgolf_port.exe
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
unset SG_TEXT_METRICS SG_EMU_ORACLE
